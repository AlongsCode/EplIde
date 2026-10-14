import sys, struct
sys.path.insert(0,"tools")
import e_resource as R
res=R.load_resource(r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e")
recs=R.parse(res)
for ri,rec in enumerate(recs):
    for eid,off,el,elen in R.elems(res,rec):
        blk=res[el:el+elen]
        print("==== id=%08X elen=%d"%(eid,elen))
        for o in range(0,len(blk),4):
            v=struct.unpack_from("<I",blk,o)[0]
            s="".join(chr(c) if 32<=c<127 else "." for c in blk[o:o+4])
            print("  %3d  %08X  %-4d  %s"%(o,v,v,s))
