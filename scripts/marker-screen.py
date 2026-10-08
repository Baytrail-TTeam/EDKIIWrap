"""Decode the diagnostic font from a QEMU P6 screenshot (no OCR dependency)."""
import re
from pathlib import Path
root=Path(__file__).resolve().parent.parent
font_text=(root/'edk2-platform/Library/MarkerLib/Marker.c').read_text().split('Font[][5]={',1)[1].split('};',1)[0]
font=[tuple(int(x,0) for x in group.split(',')) for group in re.findall(r'\{([^{}]+)\}',font_text)]
glyphs=dict(zip(font,'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'));glyphs[(0,0,0,0,0)]=' '
def marker(pixels,width,row,color):
 text=''
 for n in range(48):
  cols=[]
  for x in range(5):
   bits=0
   for y in range(7):
    off=((8+row*32+6+y*2)*width+16+n*16+x*2)*3
    if pixels[off:off+3]==color:bits|=1<<y
   cols.append(bits)
  text+=glyphs.get(tuple(cols),'?')
 return text.rstrip()
def read_ppm(path):
 with Path(path).open('rb') as f:
  assert f.readline().strip()==b'P6';dims=f.readline()
  while dims.startswith(b'#'):dims=f.readline()
  w,h=map(int,dims.split());assert f.readline().strip()==b'255';pixels=f.read()
 assert len(pixels)==w*h*3
 return pixels,w,h
