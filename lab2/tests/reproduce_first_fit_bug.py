"""Reproduce the original First-Fit tail-link defect in an isolated copy.

Success means the old framework check passes, then the new invariant check
detects the expected defect. The delivered fixed sources are never modified.
"""
import argparse
import json
from pathlib import Path
import tempfile

from run_checks import ROOT, FIRST_FIT_TEST, copy_tree, qemu_check

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--profile',choices=['course','portable'],default='course')
    parser.add_argument('--qemu',default='qemu-system-riscv64')
    args=parser.parse_args();args.memory='128M'
    logs=ROOT/'logs';logs.mkdir(exist_ok=True)
    name='member2_first_fit_defect_'+args.profile
    with tempfile.TemporaryDirectory(prefix='lab2_original_first_fit_') as tmp:
        tree=Path(tmp)/'lab2';copy_tree(tree)
        source=tree/'kern/mm/default_pmm.c';text=source.read_text()
        for statement in ('                break; // Stop before visiting and reinserting the new tail.\n',
                          '                break; // Preserve the actual predecessor of the appended block.\n'):
            if text.count(statement)!=1:raise RuntimeError('Missing exact regression-removal anchor')
            text=text.replace(statement,'')
        source.write_text(text+FIRST_FIT_TEST)
        pmm=tree/'kern/mm/pmm.c';pmm.write_text(pmm.read_text().replace(
            'pmm_manager = &best_fit_pmm_manager;','pmm_manager = &default_pmm_manager;'))
        init=tree/'kern/init/init.c';init.write_text(init.read_text().replace(
            '    member2_page_check();','    extern void first_fit_tail_check(void);\n    first_fit_tail_check();'))
        try:
            qemu_check(tree,args,name,logs,['first_fit_tail_check() succeeded!'])
        except RuntimeError:
            text=(logs/(name+'_qemu.log')).read_text()
            expected='assertion failed: list_next(le)->prev == le'
            if 'check_alloc_page() succeeded!' not in text or expected not in text:
                raise RuntimeError('Did not observe the intended controlled failure')
        else:raise RuntimeError('Original defect unexpectedly passed')
    result={'result':'EXPECTED_FAILURE','framework_check':'PASS',
            'new_invariant':'detected broken predecessor link','profile':args.profile,
            'delivered_source_modified':False}
    (logs/(name+'_result.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('PASS regression reproduction: original check passes; new invariant catches the old defect.')

if __name__=='__main__':main()
