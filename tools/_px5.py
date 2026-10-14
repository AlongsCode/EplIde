import sys,struct
sys.path.insert(0,"tools")
import e_resource as R
res=R.load_resource(r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e")
recs=R.parse(res)
for ri,rec in enumerate(recs):
    for eid,off,el,elen in R.elems(res,rec):
        d=R.parse_elem(res[el:el+elen])
        if (d["dataType"]&0xFFFF)!=1: continue
        blk=res[el:el+elen]
        print("form element elen=%d tailOff=%d"%(elen,d["tailOff"]))
        for o in range(0,elen,16):
            ch=blk[o:o+16]
            print("%4d  %-47s %s"%(o," ".join("%02X"%c for c in ch),"".join(chr(c) if 32<=c<127 else "." for c in ch)))
