#!/usr/bin/env python3
"""Checks that the DLLs of a packed Windows folder give each other what they ask for, without running anything.

A DLL that lacks a function another one imports makes Windows refuse to start the program, with only 0xC0000139 to say
so, or a dialog no one sees on a CI machine. Here it gets a name. Runs on any system.

    check_pe_imports.py <folder> [file ...]      # exit 1 if a symbol is missing; the listed files' imports are shown
"""
import struct,sys,os,glob
def rva2off(secs,rva):
    for va,vs,ptr,sz in secs:
        if va<=rva<va+max(vs,sz): return rva-va+ptr
    return None
def cstr(b,o):
    e=b.index(b'\0',o); return b[o:e].decode('latin1')
def parse(p):
    b=open(p,'rb').read()
    pe=struct.unpack_from('<I',b,0x3c)[0]
    n=struct.unpack_from('<H',b,pe+6)[0]; opt=struct.unpack_from('<H',b,pe+20)[0]; o=pe+24
    magic=struct.unpack_from('<H',b,o)[0]; dd=o+(112 if magic==0x20b else 96)
    secs=[]; s=o+opt
    for i in range(n):
        vs,va,sz,ptr=struct.unpack_from('<IIII',b,s+8+40*i); secs.append((va,vs,ptr,sz))
    exp=set(); erva,_=struct.unpack_from('<II',b,dd)
    if erva:
        e=rva2off(secs,erva)
        nn=struct.unpack_from('<I',b,e+24)[0]; names=struct.unpack_from('<I',b,e+32)[0]; off=rva2off(secs,names)
        for i in range(nn):
            r=struct.unpack_from('<I',b,off+4*i)[0]; exp.add(cstr(b,rva2off(secs,r)))
    imp={}; irva,_=struct.unpack_from('<II',b,dd+8)
    if irva:
        i=rva2off(secs,irva)
        while True:
            oft,_,_,nm,ft=struct.unpack_from('<IIIII',b,i)
            if nm==0: break
            dll=cstr(b,rva2off(secs,nm)).lower(); t=rva2off(secs,oft or ft); L=[]
            while True:
                v=struct.unpack_from('<Q',b,t)[0]
                if v==0: break
                if not v>>63: L.append(cstr(b,rva2off(secs,v&0x7fffffff)+2))
                t+=8
            imp.setdefault(dll,[]).extend(L); i+=20
    return imp,exp
d=sys.argv[1]
files={}
for r,_,fs in os.walk(d):
    for f in fs:
        if f.lower().endswith(('.dll','.exe')) and os.path.dirname(os.path.join(r,f))==d: files[f.lower()]=os.path.join(r,f)
data={}
for k,p in files.items():
    try: data[k]=parse(p)
    except Exception as e: print('parse fail',k,e)
missing=0; ext=set()
for k,(imp,_) in sorted(data.items()):
    for dll,names in imp.items():
        if dll in data:
            miss=[n for n in names if n not in data[dll][1]]
            if miss: missing+=len(miss); print(f'{k} -> {dll}: {len(miss)} missing: {miss[:8]}')
        else: ext.add(dll)
print('checked',len(data),'files; missing symbols',missing)
print('outside the folder:',sorted(ext))
want=sys.argv[2:] 
for k in want:
    print(k,'imports',sorted(data[k][0]))
sys.exit(1 if missing else 0)
