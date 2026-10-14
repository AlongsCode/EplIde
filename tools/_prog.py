from PIL import Image, ImageDraw
rows=[("r1  31.07%","shots\\des_round\\r1\\cmp.png"),("r2  90.89%","shots\\des_round\\r2\\cmp.png"),
      ("r6  98.43%","shots\\des_round\\r6\\cmp.png"),("r10 99.59%","shots\\des_round\\r10\\cmp.png")]
imgs=[(t,Image.open(p).convert("RGB")) for t,p in rows]
W=1180
scaled=[]
for t,im in imgs:
    h=int(im.size[1]*W/im.size[0]); scaled.append((t,im.resize((W,h),Image.LANCZOS)))
labelh=16
H=sum(im.size[1]+labelh+6 for t,im in scaled)
sheet=Image.new("RGB",(W,H),(25,25,25)); d=ImageDraw.Draw(sheet); y=0
for t,im in scaled:
    d.text((4,y+2),t,fill=(255,255,0)); y+=labelh
    sheet.paste(im,(0,y)); y+=im.size[1]+6
sheet.save(r"shots\des_round\progress.png")
print("shots\\des_round\\progress.png", sheet.size)
