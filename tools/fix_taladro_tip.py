"""Completa la punta del taladro (frames 4-7, fila 0) de la capsula de Rocksteady 1-2.
El arte original venia cortado plano en la fila 62 de la celda.
Uso: python3 tools/fix_taladro_tip.py ENTRADA.png SALIDA.png [preview.png]
Entrada original guardada en res/sprites/orig/taladro_capsula_v2_sin_punta.png
"""
import sys
import numpy as np
from PIL import Image
SRC=sys.argv[1]; DST=sys.argv[2]; PV=sys.argv[3] if len(sys.argv)>3 else None
im=Image.open(SRC); a=np.array(im)
W,H=96,120
GRAY=[1,2,6,11,13]
SLOPE=0.85
Y0=62
# donantes: filas del taladro (solo grises) de los frames 2..7, con su ancho
donors=[]
for f in range(2,8):
    c=a[0:H,f*W:(f+1)*W]
    for y in range(Y0,100):
        row=c[y]
        g=np.isin(row,GRAY)
        xx=np.nonzero(g)[0]
        if len(xx)<1: continue
        x0,x1=xx.min(),xx.max()
        seg=row[x0:x1+1].copy()
        seg[~np.isin(seg,GRAY)]=0
        if (seg!=0).mean()<0.85: continue      # filas con escombro/huecos: no
        donors.append((x1-x0+1,seg))
def donor(w):
    best=min(donors,key=lambda d:abs(d[0]-w))
    seg=best[1]
    idx=np.minimum((np.arange(w)*len(seg)/w).astype(int),len(seg)-1)
    return seg[idx]
out=a.copy()
for f in (4,5,6,7):
    c=out[0:H,f*W:(f+1)*W]
    xx=np.nonzero(np.isin(c[Y0],GRAY))[0]
    x0,x1=xx.min(),xx.max(); w0=x1-x0+1; cx=(x0+x1)/2
    h=int(round(w0/SLOPE))
    for k in range(1,h+1):
        y=Y0-k
        w=max(1,int(round(w0*(h-k)/h)))
        if w<1: break
        seg=donor(w)
        xs=int(round(cx-(w-1)/2))
        dst=c[y,xs:xs+w]
        m=(seg!=0)&(dst==0)
        dst[m]=seg[m]
    print(f,'ancho',w0,'punta',h,'px -> fila',Y0-h)
o=Image.fromarray(out,'P'); o.putpalette(im.getpalette()); o.save(DST,transparency=0)
if not PV: sys.exit(0)
pv=Image.new('RGBA',(W*4*2+20,H*2),(60,120,60,255))
A=Image.open(SRC).convert('RGBA'); B=o.convert('RGBA')
for i,f in enumerate((4,5,6,7)):
    pv.alpha_composite(A.crop((f*W,0,(f+1)*W,H)),(i*W,0))
    pv.alpha_composite(B.crop((f*W,0,(f+1)*W,H)),(W*4+20+i*W,0))
pv.convert('RGB').resize(((W*8+20)*2,H*4),Image.NEAREST).save(PV)
