#!/usr/bin/env python3
"""Fetch hash-pinned upstream archives and extract regular source files."""
import hashlib,io,tarfile,urllib.request
from pathlib import Path
ARCHIVES=(
 ('libpd','libpd/libpd','ba0dc63262901d658af8bbda5e619a60fa975e78','7432b9108d6741f8ca5fd786ce83ed391d92b5197e93fd3762a5e81ae663095d'),
 ('pd','pure-data/pure-data','f009fd8d7b537e209e09898d487fdf1bf547da2b','29ad07f1f13dd199e2ebcd3c486dca4c496e4874dd856c925dc3891e11e58267'),
 ('qubit','Qu-Bit-Electronix/QB_Nebulae_V2','4be42b5159ae5c63466d84f0a8ab3b2fb4afd5a6','5d32c02655e6f6f3b0d31bf13417029caca8ed3f995c7c7fd7e6aff3de719530'))
def main():
 import sys
 output=Path(sys.argv[1]);output.mkdir()
 cache=Path(__file__).resolve().parent/'package';cache.mkdir(exist_ok=True)
 for name,repo,pin,sha in ARCHIVES:
  archive=cache/f'upstream-{name}.tar.gz'
  data=archive.read_bytes() if archive.exists() else urllib.request.urlopen(f'https://codeload.github.com/{repo}/tar.gz/{pin}',timeout=90).read(150*1024*1024)
  if hashlib.sha256(data).hexdigest()!=sha:raise ValueError(f'Archive identity mismatch: {name}')
  if not archive.exists():archive.write_bytes(data)
  with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as tar:
   for member in tar:
    parts=Path(member.name).parts[1:]
    if '..' in parts:raise ValueError('Unsafe archive path')
    if member.isfile() and parts:
     dest=output/name/Path(*parts);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(tar.extractfile(member).read())
  print(name,pin,sha)
if __name__=='__main__':main()
