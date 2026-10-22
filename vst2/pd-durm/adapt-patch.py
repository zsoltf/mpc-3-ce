#!/usr/bin/env python3
"""Reproducible narrow ingress/state adaptation of the pinned Durm graph."""
import re, sys
from pathlib import Path
class Canvas:
    def __init__(self, head): self.head=head; self.items=[]; self.connections=[]; self.tail=''
    def add(self, text):
        n=len(self.items); self.items.append(text); return n
    def connect(self, a, out, b, inlet=0): self.connections.append(f'#X connect {a} {out} {b} {inlet};')
    def render(self):
        return '\n'.join([self.head]+[i.render() if isinstance(i,Canvas) else i for i in self.items]+self.connections+([self.tail] if self.tail else []))
def parse(text):
    stack=[]
    for stmt in re.split(r'(?<!\\);',text):
        stmt=stmt.strip()
        if not stmt: continue
        stmt+=';'
        if stmt.startswith('#N canvas'):
            c=Canvas(stmt)
            if stack: stack[-1].items.append(c)
            else: root=c
            stack.append(c)
        elif stmt.startswith('#X restore'): stack.pop().tail=stmt
        elif stmt.startswith('#X connect'): stack[-1].connections.append(stmt)
        else: stack[-1].items.append(stmt)
    return root

def adapt(root):
    random_id=0; voices=[]
    def walk(c,path):
        nonlocal random_id
        name=c.head.split()[-2] if path else 'root'
        path=path+[name]
        original=list(c.items)
        for n,item in enumerate(original):
            if isinstance(item,Canvas): walk(item,path); continue
            if 'netreceive 3000;' in item: c.items[n]='#X obj 54 33 r pd-durm-hardware-unused;'
            if ' loadbang;' in item: c.items[n]=item.replace(' loadbang;',' r pd-durm-load;')
            # Random and noise streams are restarted with per-object fixed seeds.
            if item.startswith('#X obj ') and re.search(r' (random|noise~)( |;)',item):
                random_id+=1
                recv=c.add(f'#X obj 10 10 r pd-durm-seed;')
                msg=c.add(f'#X msg 10 30 seed {1009+random_id*7919};')
                c.connect(recv,0,msg);c.connect(msg,0,n)
            # Panic stops autonomous bursts and voice envelopes, preserving controls.
            if 'drum-synthesis' in path and re.search(r' (metro|delay|vline~|line~)( |;)',item):
                recv=c.add('#X obj 10 10 r pd-durm-panic;')
                msg=c.add('#X msg 10 30 stop;'); c.connect(recv,0,msg); c.connect(msg,0,n)
                if re.search(r' (vline~|line~)( |;)',item):
                    zero=c.add('#X msg 10 50 0;');c.connect(recv,0,zero);c.connect(zero,0,n)
        if name=='parameters':
            voice=path[-2]; assert voice in ('bd','sd','hh','fm'); voices.append(voice)
            holders=[37,38,39,40,41,42,56 if voice=='bd' else 55]
            for j,n in enumerate(holders):
                assert re.search(r' f;',c.items[n]),(voice,n,c.items[n])
                observe=c.add(f'#X obj 10 10 s pd-durm-held-{voice}-{j};')
                restore=c.add(f'#X obj 10 30 r pd-durm-restore-{voice}-{j};')
                c.connect(n,0,observe);c.connect(restore,0,n)
            # Explicit host lock value replaces only hardware button/toggle ingress.
            c.connections=[s for s in c.connections if not re.search(r' 46 1;$',s)]
            lock=c.add(f'#X obj 10 60 r pd-durm-lock-{voice};')
            invert=c.add('#X obj 10 80 == 0;')
            c.connect(lock,0,invert);c.connect(invert,0,46,1)
    walk(root,[])
    assert voices==['bd','sd','hh','fm']
    return random_id
if __name__=='__main__':
    root=parse(Path(sys.argv[1]).read_text()); count=adapt(root)
    Path(sys.argv[2]).write_text(root.render()+'\n')
    print(f'Adapted four voice latches and locks, {count} deterministic random/noise objects; synthesis connections preserved.')
