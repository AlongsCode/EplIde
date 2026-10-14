import struct,sys
def exports(path):
    d=open(path,"rb").read()
    pe=struct.unpack_from("<I",d,0x3C)[0]
    opt=pe+24
    magic=struct.unpack_from("<H",d,opt)[0]
    nsec=struct.unpack_from("<H",d,pe+6)[0]
    optsz=struct.unpack_from("<H",d,pe+20)[0]
    ddoff=opt+(96 if magic==0x10B else 112)
    erva,esize=struct.unpack_from("<II",d,ddoff+0*8)
    secs=[]
    for i in range(nsec):
        o=pe+24+optsz+i*40
        name,vsize,vaddr,rawsize,rawptr=struct.unpack_from("<8sIIII",d,o)
        secs.append((vaddr,vsize,rawptr,rawsize))
    def r2o(rva):
        for vaddr,vsize,rawptr,rawsize in secs:
            if vaddr<=rva<vaddr+max(vsize,rawsize): return rawptr+(rva-vaddr)
        return None
    base=r2o(erva)
    if base is None: return []
    nnames,nfuncs=struct.unpack_from("<II",d,base+24)
    anames=struct.unpack_from("<I",d,base+32)[0]
    ao=base+(anames-erva)
    names=[]
    for i in range(nnames):
        nrva=struct.unpack_from("<I",d,ao+4*i)[0]
        off=r2o(nrva)
        e=d.index(b"\x00",off)
        names.append(d[off:e].decode("latin1"))
    return names
for p in [r"C:\Program Files (x86)\e\lib\krnln.fne"]:
    n=exports(p)
    print(p, len(n))
    print(n)
