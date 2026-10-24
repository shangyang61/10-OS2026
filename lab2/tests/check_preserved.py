"""Verify member-one page_init and the original framework tests verbatim."""
from pathlib import Path
from hashlib import sha256
import json
import re

ROOT = Path(__file__).resolve().parents[1]

def extract_function(text, name):
    match = re.search(r'\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{', text)
    if not match:
        raise ValueError('Function not found: '+name)
    start=match.start(); pos=match.end()-1; depth=0; state='code'; escape=False
    while pos<len(text):
        char=text[pos]; nxt=text[pos+1:pos+2]
        if state=='line':
            if char=='\n':state='code'
        elif state=='comment':
            if char=='*' and nxt=='/':state='code';pos+=1
        elif state in ('string','char'):
            if escape:escape=False
            elif char=='\\':escape=True
            elif (state=='string' and char=='"') or (state=='char' and char=="'"):state='code'
        elif char=='/' and nxt=='/':state='line';pos+=1
        elif char=='/' and nxt=='*':state='comment';pos+=1
        elif char=='"':state='string'
        elif char=="'":state='char'
        elif char=='{':depth+=1
        elif char=='}':
            depth-=1
            if depth==0:return text[start:pos+1]
        pos+=1
    raise ValueError('Unclosed function: '+name)

def main():
    manifest=json.loads((ROOT/'specs/protected_functions.json').read_text(encoding='utf-8'))
    for item in manifest:
        code=(ROOT/item['file']).read_text(encoding='utf-8')
        actual=sha256(extract_function(code,item['function']).encode()).hexdigest()
        if actual!=item['sha256']:
            raise SystemExit('FAIL: changed protected function '+item['file']+':'+item['function'])
        print('PASS protected '+item['file']+':'+item['function'])

if __name__=='__main__':main()
