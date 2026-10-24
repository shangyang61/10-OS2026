"""Build both startup profiles and inspect the emitted root PTEs."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
from run_checks import ROOT,copy_tree,capture

def main():
    logs=ROOT/'logs';logs.mkdir(exist_ok=True);results=[]
    with tempfile.TemporaryDirectory(prefix='lab2_boot_profiles_') as tmp:
        for portable in (0,1):
            tree=Path(tmp)/str(portable);copy_tree(tree)
            capture(['make','-j2','PORTABLE_BOOT='+str(portable)],tree,
                    logs/('member2_boot_profile_'+str(portable)+'_build.log'))
            output=subprocess.check_output(['riscv64-unknown-elf-nm','bin/kernel'],cwd=tree,text=True)
            address=next(int(line.split()[0],16) for line in output.splitlines()
                         if line.split()[-1]=='boot_page_table_sv39')
            image=(tree/'bin/ucore.img').read_bytes()
            offset=address-0xffffffffc0200000
            entries=struct.unpack_from('<512Q',image,offset)
            expected={511:0x200000cf}
            if portable:expected[2]=0x200000cf
            observed={i:value for i,value in enumerate(entries) if value}
            assert observed==expected,(portable,observed)
            item={'portable_boot':portable,'root_va':hex(address),
                  'nonzero_root_ptes':{str(k):hex(v) for k,v in observed.items()},
                  'image_sha256':hashlib.sha256(image).hexdigest(),'result':'PASS'}
            results.append(item);print('PASS startup profile',portable,observed)
    (logs/'member2_boot_profiles.json').write_text(json.dumps(results,indent=2),encoding='utf-8')

if __name__=='__main__':main()
