#!/usr/bin/env python3
"""Reproducible native RISC-V/QEMU checks; run from any Linux directory.

Example: python3 tests/run_checks.py --profile portable --grade --first-fit
Course profile preserves the provided QEMU 4.1.1 startup path.
No test pretends that a WSL run was executed in VMware.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
DEFAULT_MARKERS=['memory management: best_fit_pmm_manager','check_alloc_page() succeeded!',
                 'member2_page_check() succeeded!','slub_check() succeeded!']

def capture(command, cwd, logfile, timeout=120):
    result=subprocess.run(command,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
    logfile.write_bytes(result.stdout)
    if result.returncode:
        print(result.stdout.decode(errors='replace'))
        raise RuntimeError('Command failed: '+repr(command))
    return result.stdout.decode(errors='replace')

def copy_tree(path):
    shutil.copytree(ROOT,path,ignore=shutil.ignore_patterns(
        'obj','bin','logs','.git','__pycache__','cscope*','tags','.qemu.out','.gdb.in'))

def qemu_check(tree, args, name, logs, markers):
    capture(['make','-j2','PORTABLE_BOOT='+('1' if args.profile=='portable' else '0')],
            tree,logs/(name+'_build.log'))
    image=tree/'bin/ucore.img'
    command=[args.qemu,'-machine','virt','-m',args.memory,'-nographic','-bios','default']
    command+=['-kernel','bin/ucore.img'] if args.profile=='portable' else [
        '-device','loader,file=bin/ucore.img,addr=0x80200000']
    proc=subprocess.Popen(command,cwd=tree,stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    try:
        output,_=proc.communicate(timeout=8)
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:output,_=proc.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill();output,_=proc.communicate()
    text=output.decode(errors='replace');(logs/(name+'_qemu.log')).write_text(text,encoding='utf-8')
    if 'panic' in text.lower() or any(m not in text for m in markers):
        print(text)
        raise RuntimeError(name+': missing success marker or panic')
    print('PASS '+name)
    return {'case':name,'profile':args.profile,'memory':args.memory,'markers':markers,
            'image_sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'result':'PASS'}

FIRST_FIT_TEST='''
#include <stdio.h>
void first_fit_tail_check(void) {
    size_t n = nr_free_pages();
    struct Page *base = alloc_pages(n);
    assert(base != NULL && n > 3);
    free_page(base);
    free_page(base + 2);
    list_entry_t *le = &free_list;
    do {
        assert(list_next(le)->prev == le);
        assert(list_prev(le)->next == le);
        le = list_next(le);
    } while (le != &free_list);
    free_pages(base + 3, n - 3);
    free_page(base + 1);
    assert(nr_free_pages() == n && alloc_pages(n) == base);
    free_pages(base, n);
    cprintf("first_fit_tail_check() succeeded!\\n");
}
'''

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--profile',choices=['course','portable'],default='course')
    parser.add_argument('--qemu',default=os.environ.get('QEMU','qemu-system-riscv64'))
    parser.add_argument('--memory',choices=['64M','128M','256M'],default='128M')
    parser.add_argument('--grade',action='store_true')
    parser.add_argument('--first-fit',action='store_true')
    parser.add_argument('--log-prefix',default='member2')
    args=parser.parse_args()
    if args.grade and args.memory!='128M':
        parser.error('native make grade uses its default 128 MiB; use --memory 128M with --grade')
    if not args.log_prefix.replace('_','').replace('-','').isalnum():parser.error('Invalid log prefix')
    for name in ('make','gcc','riscv64-unknown-elf-gcc',args.qemu):
        if not shutil.which(name):raise RuntimeError('Tool not found on PATH: '+name)
    logs=ROOT/'logs';logs.mkdir(exist_ok=True)
    prefix=args.log_prefix+'_'+args.memory
    environment={'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
                 'system':platform.uname()._asdict(),'profile':args.profile,'qemu':shutil.which(args.qemu),
                 'compiler':shutil.which('riscv64-unknown-elf-gcc'),'memory':args.memory}
    for tool in ('riscv64-unknown-elf-gcc',args.qemu):
        environment[tool]=subprocess.check_output([tool,'--version'],text=True).splitlines()[0]
    (logs/(prefix+'_environment.json')).write_text(json.dumps(environment,indent=2),encoding='utf-8')
    capture([sys.executable,str(ROOT/'tests/check_preserved.py')],ROOT,logs/(prefix+'_preserved.log'))
    capture([sys.executable,str(ROOT/'tests/member1/test_page_init.py')],ROOT,logs/(prefix+'_page_init_boundaries.log'))
    results=[]
    with tempfile.TemporaryDirectory(prefix='lab2_member2_checks_') as tmp:
        tree=Path(tmp)/'normal';copy_tree(tree)
        results.append(qemu_check(tree,args,prefix,logs,DEFAULT_MARKERS))
        if args.grade:
            text=capture(['make','PORTABLE_BOOT='+('1' if args.profile=='portable' else '0'),
                          'QEMU='+args.qemu,'grade'],tree,logs/(prefix+'_grade.log'))
            if 'Total Score: 20/20' not in text:raise RuntimeError('Original grade did not report 20/20')
            results.append({'case':'original make grade','result':'PASS','score':'20/20'})
            print('PASS original make grade: 20/20')
        if args.first_fit:
            first=Path(tmp)/'first_fit';copy_tree(first)
            pmm=first/'kern/mm/pmm.c';pmm.write_text(pmm.read_text().replace(
                'pmm_manager = &best_fit_pmm_manager;','pmm_manager = &default_pmm_manager;'))
            init=first/'kern/init/init.c';init.write_text(init.read_text().replace(
                '    member2_page_check();','    extern void first_fit_tail_check(void);\n    first_fit_tail_check();'))
            with (first/'kern/mm/default_pmm.c').open('a') as f:f.write(FIRST_FIT_TEST)
            results.append(qemu_check(first,args,prefix+'_first_fit',logs,[
                'memory management: default_pmm_manager','check_alloc_page() succeeded!',
                'first_fit_tail_check() succeeded!','slub_check() succeeded!']))
    (logs/(prefix+'_results.json')).write_text(json.dumps(results,indent=2),encoding='utf-8')
    print('All requested checks passed. Logs:',logs)

if __name__=='__main__':
    try:main()
    except (RuntimeError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as e:
        print('FAIL:',e,file=sys.stderr);sys.exit(1)
