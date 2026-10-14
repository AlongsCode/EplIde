import sys,struct
sys.path.insert(0,"tools")
import e_resource as R
import e_sections as S
for path in (r"C:\Users\Along\AppData\Local\Temp\e_re\samples\flowcmd.e",
             r"C:\Users\Along\AppData\Local\Temp\e_re\samples\superlist.e",
             r"C:\Users\Along\Desktop\eclone\shots\e_samples\graphic.e"):
    res=R.load_resource(path)
    recs=R.parse(res)
    print("==",path)
    for ri,rec in enumerate(recs):
        for eid,off,el,elen in R.elems(res,rec):
            d=R.parse_elem(res[el:el+elen])
            if (d["dataType"]&0xFFFF)!=1: continue
            blk=res[el:el+elen]
            blob=blk[d["tailOff"]+45:]
            print("   form blob=%d  tail=%s"%(len(blob),blob[-40:].hex()))
            # last [u32 len][gbk]
            n=struct.unpack_from("<I",blob,len(blob)-4-0)[0] if len(blob)>=4 else 0
            print("   last8=%s"%blob[-8:].hex())
