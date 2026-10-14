import sys
d=open(r"C:\Program Files (x86)\e\lib\krnln.fne","rb").read()
pats={
 "white-ish 0x00FFFFFF":bytes.fromhex("FFFFFF00"),
 "BD0 98B4D0 as 0x00D0B498":bytes.fromhex("98B4D000"),
 "calt B9D1EA -> 0x00EAD1B9":bytes.fromhex("EAD1B900"),
 "3FCEF2 -> 0x00F2CE3F":bytes.fromhex("3FCEF200"),
 "28CFE4 -> 0x00E4CF28":bytes.fromhex("28CFE400"),
 "B8432C -> 0x002C43B8":bytes.fromhex("B8432C00"),
 "431422 -> 0x00221443":bytes.fromhex("43142200"),
}
for k,v in pats.items():
    i=0; hits=[]
    while True:
        j=d.find(v,i)
        if j<0: break
        hits.append(j); i=j+1
        if len(hits)>6: break
    print("%-30s hits=%d %s"%(k,len(hits),[hex(h) for h in hits[:5]]))
