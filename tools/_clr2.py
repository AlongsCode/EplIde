import sys
files=[r"C:\Program Files (x86)\e\e.exe", r"C:\Program Files (x86)\e\lib\krnln.fne", r"D:\EPL\e.exe"]
pats={
 "98B4D0":bytes.fromhex("98B4D000"),
 "B9D1EA":bytes.fromhex("EAD1B900"),
 "3FCEF2":bytes.fromhex("3FCEF200"),
 "28CFE4":bytes.fromhex("28CFE400"),
 "B8432C":bytes.fromhex("B8432C00"),
 "431422":bytes.fromhex("43142200"),
 "343434":bytes.fromhex("34343400"),
 "646464":bytes.fromhex("64646400"),
}
for p in files:
    d=open(p,"rb").read()
    out=[]
    for k,v in pats.items():
        n=0; i=0
        while True:
            j=d.find(v,i)
            if j<0: break
            n+=1; i=j+1
        out.append("%s=%d"%(k,n))
    print("%-50s %s"%(p.split("\\")[-1], " ".join(out)))
