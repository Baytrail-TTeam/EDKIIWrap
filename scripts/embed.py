from pathlib import Path
root = Path(__file__).resolve().parent.parent
parts=[]
for name in ('transition','payload'):
 data=(root/'build'/f'{name}.bin').read_bytes()
 parts.append(f'static const unsigned char {name}_blob[]={{\n'+','.join(str(v) for v in data)+'\n};\n')
(root/'build/blobs.h').write_text(''.join(parts))
