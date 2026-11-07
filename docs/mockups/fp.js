const PI=Math.PI;
const L=s=>new Promise(r=>{const i=new Image();i.onload=()=>r(i);i.src=s});
function rr(x,y,w,h,r,ctx=g){ctx.beginPath();ctx.roundRect(x,y,w,h,r);}
function hex(h,a){const n=parseInt(h.slice(1),16);return `rgba(${n>>16},${(n>>8)&255},${n&255},${a})`;}
let seed=11;const rnd=()=>{seed=(seed*16807)%2147483647;return (seed-1)/2147483646;};

// ---- the faceplate camera: tilted 11 deg, orthographic. panel px (+ height z in Blender units) -> screen px.
let FW=980,FH=544;const SX=1/1.01,SY=0.98163/1.01,ZK=100*Math.sin(11*PI/180)/1.01;
let BX=0,BY=0;
const sx=v=>BX+FW/2+(v-FW/2)*SX, sy=(v,z=0)=>BY+FH/2+(v-FH/2)*SY-z*ZK;

// ---- text centred on its ink (not its em box), letter by letter so spacing never pushes it off centre,
// drawn in the plane of the surface it sits on (the same squash the camera gives that surface).
function inkDraw(ctx,t,font,ls=0){
  ctx.font=font;ctx.textBaseline='alphabetic';ctx.textAlign='left';
  const ch=[...t],adv=ch.map(c=>ctx.measureText(c).width);
  const m0=ctx.measureText(ch[0]),mN=ctx.measureText(ch[ch.length-1]),all=ctx.measureText(t);
  const pre=adv.slice(0,-1).reduce((a,b)=>a+b,0)+ls*(ch.length-1);
  const left=-m0.actualBoundingBoxLeft,right=pre+mN.actualBoundingBoxRight;
  let x=-(left+right)/2;const y=(all.actualBoundingBoxAscent-all.actualBoundingBoxDescent)/2;
  ch.forEach((c,i)=>{ctx.fillText(c,x,y);x+=adv[i]+ls;});
}
// ---- layout check: every label, readout, plaque, knob and part registers its box (panel px); checkLayout() reports
// anything that touches or comes closer than GAP, and any label that spills out of the thing it sits in.
let LAY=[],LAYW=0;const GAP=5;
function lay(id,x0,y0,x1,y1,parent=null,kind='part'){LAY.push({id,x0,y0,x1,y1,parent,kind,win:LAYW});}
function layC(id,cx,cy,r,parent=null){lay(id,cx-r,cy-r,cx+r,cy+r,parent,'part');Object.assign(LAY[LAY.length-1],{circle:true,cx,cy,r});}
function layR(id,cx,cy,w,h,parent=null){lay(id,cx-w/2,cy-h/2,cx+w/2,cy+h/2,parent,'part');}
function layS(id,cx,cy,w,h){lay(id,cx-w/2,cy-h/2,cx+w/2,cy+h/2,null,'part');LAY[LAY.length-1].strict=true;}
function inkBox(t,font,ls){g.save();g.font=font;const ch=[...t],adv=ch.map(c=>g.measureText(c).width),all=g.measureText(t);const w=adv.slice(0,-1).reduce((a,b)=>a+b,0)+ls*(ch.length-1)+g.measureText(ch[ch.length-1]).actualBoundingBoxRight+g.measureText(ch[0]).actualBoundingBoxLeft;g.restore();return [w,all.actualBoundingBoxAscent+all.actualBoundingBoxDescent];}
function checkLayout(tag){const bad=[];const own=(a,b)=>a.parent===b.id||b.parent===a.id;
  const W=LAY.filter(a=>a.win===LAYW);
  for(let i=0;i<W.length;i++)for(let j=i+1;j<W.length;j++){const a=W[i],b=W[j];if(own(a,b))continue;if(a.kind==='part'&&b.kind==='part'&&!(a.strict||b.strict))continue;
    const c=a.circle?a:b.circle?b:null,o=c===a?b:a;
    if(c&&!o.circle){const dx=Math.max(o.x0-c.cx,0,c.cx-o.x1),dy=Math.max(o.y0-c.cy,0,c.cy-o.y1);if(Math.hypot(dx,dy)<c.r+GAP)bad.push(`${a.id}  x  ${b.id}`);continue;}
    if(a.x0<b.x1+GAP&&b.x0<a.x1+GAP&&a.y0<b.y1+GAP&&b.y0<a.y1+GAP)bad.push(`${a.id}  x  ${b.id}`);}
  W.filter(a=>a.parent).forEach(a=>{const p=W.find(b=>b.id===a.parent);if(!p)return;
    if(p.circle){const far=Math.max(...[[a.x0,a.y0],[a.x1,a.y0],[a.x0,a.y1],[a.x1,a.y1]].map(([x,y])=>Math.hypot(x-p.cx,y-p.cy)));if(far>p.r-2&&a.kind==='text')bad.push(`${a.id} spills out of ${p.id}`);return;}
    if(a.x0<p.x0+3||a.x1>p.x1-3||a.y0<p.y0+2||a.y1>p.y1-2)bad.push(`${a.id} spills out of ${p.id}`);});
  console.log(`LAYOUT ${tag}: ${bad.length?bad.length+' problem(s)\n  '+bad.join('\n  '):'clean'}`);LAYW++;return bad;}
function surfText(t,px,py,z,font,col,{ls=0,glow=null,blur=8,wear=0,parent=null,nolay=false}={}){
  const x=sx(px),y=sy(py,z);
  if(!nolay){const [w,h]=inkBox(t,font,ls);lay('"'+t+'"',px-w/2,py-h/2,px+w/2,py+h/2,parent,'text');}
  if(wear){ // worn paint: draw it off screen, chip it, then place it
    const oc=document.createElement('canvas');oc.width=700;oc.height=120;const o=oc.getContext('2d');o.translate(350,60);o.fillStyle=col;inkDraw(o,t,font,ls);
    o.setTransform(1,0,0,1,0,0);o.globalCompositeOperation='destination-out';
    for(let i=0;i<300*wear;i++){o.fillStyle=`rgba(0,0,0,${0.4+rnd()*0.6})`;o.beginPath();o.arc(rnd()*700,rnd()*120,0.3+rnd()*1.1,0,2*PI);o.fill();}
    for(let i=0;i<8*wear;i++){o.fillStyle='rgba(0,0,0,0.85)';o.fillRect(rnd()*700,rnd()*120,2+rnd()*8,0.6+rnd()*0.8);}
    g.save();g.translate(x,y);g.scale(SX,SY);g.shadowColor='rgba(0,0,0,0.7)';g.shadowOffsetY=1;g.shadowBlur=1;g.drawImage(oc,-350,-60);g.restore();return;}
  g.save();g.translate(x,y);g.scale(SX,SY);g.fillStyle=col;
  if(glow){g.shadowColor=glow;g.shadowBlur=blur;inkDraw(g,t,font,ls);g.shadowBlur=blur*2.2;inkDraw(g,t,font,ls);g.shadowBlur=0;g.fillStyle='rgba(255,240,220,0.4)';}
  inkDraw(g,t,font,ls);g.restore();
}
function stamped(t,px,py,z,font,ls,parent=null){surfText(t,px,py+0.8,z,font,'rgba(255,255,255,0.16)',{ls,nolay:true});surfText(t,px,py,z,font,'rgba(10,9,8,0.85)',{ls,parent});}
// flat ui text (title bar) still ink-centred
function uiText(t,x,y,font,col,ls=0){g.save();g.translate(x,y);g.fillStyle=col;inkDraw(g,t,font,ls);g.restore();}
function uiLeft(t,x,y,font,col,ls=0){g.save();g.font=font;const m=g.measureText(t);g.translate(x+m.width/2,y);g.fillStyle=col;inkDraw(g,t,font,ls);g.restore();}

function screen(x,y,w,h,r,tint='#07090c'){g.save();rr(x-2,y-2,w+4,h+4,r+2);const f=g.createLinearGradient(x,y-2,x,y+h+2);f.addColorStop(0,'rgba(0,0,0,0.8)');f.addColorStop(1,'rgba(255,255,255,0.12)');g.fillStyle=f;g.fill();rr(x,y,w,h,r);g.fillStyle=tint;g.fill();g.clip();const s=g.createLinearGradient(x,y,x,y+h*0.6);s.addColorStop(0,'rgba(0,0,0,0.6)');s.addColorStop(1,'rgba(0,0,0,0)');g.fillStyle=s;g.fillRect(x,y,w,h);g.restore();}
// orange readout set into the plate
function nixie(px,py,t,w=70,col='#ff7a2a',glow='#ff5a10',tint='#0b0503'){const x=sx(px),y=sy(py);screen(x-w*SX/2,y-9*SY,w*SX,18*SY,3,tint);const id='readout@'+px+','+py;lay(id,px-w/2,py-9,px+w/2,py+9,null,'text');surfText(t,px,py,0,'11px MB',col,{glow,blur:6,parent:id});}

function win(x,y,w,h,track,slot,col,title){
  g.save();g.shadowColor='rgba(0,0,0,0.9)';g.shadowBlur=44;g.shadowOffsetY=18;rr(x,y,w,h,10);g.fillStyle='#131315';g.fill();g.restore();
  g.save();rr(x,y,w,h,10);g.clip();const t=g.createLinearGradient(x,y,x,y+38);t.addColorStop(0,'#2b2b30');t.addColorStop(1,'#1a1a1e');g.fillStyle=t;g.fillRect(x,y,w,38);g.fillStyle='rgba(0,0,0,0.6)';g.fillRect(x,y+37,w,1);g.restore();
  const px=x+22,py=y+19;g.save();g.strokeStyle=col;g.lineWidth=2;g.lineCap='round';g.shadowColor=col;g.shadowBlur=8;g.beginPath();g.arc(px,py,6.5,-PI/2+0.55,-PI/2-0.55+2*PI);g.stroke();g.beginPath();g.moveTo(px,py-8);g.lineTo(px,py-1);g.stroke();g.restore();
  screen(x+42,y+9,150,20,4,'#0a0503');g.save();g.shadowColor=col;g.shadowBlur=6;uiText(track,x+117,y+19,'10.5px MB',col);g.restore();
  uiLeft('INSERT '+slot,x+212,y+19,'10px IS','rgba(220,220,230,0.45)',1.2);
  uiText(title,x+w/2,y+19,'13px IB','rgba(240,240,245,0.92)',2.5);
  const rx=x+w-14;uiText('✕',rx-15,y+19,'12px IS','rgba(230,230,235,0.6)');
  rr(rx-92,y+8,58,22,4);g.fillStyle='#232328';g.fill();uiText('A · B',rx-63,y+19,'10px IS','rgba(220,220,228,0.6)');
  screen(rx-262,y+9,160,20,4,'#0b0c0f');uiLeft('Default',rx-252,y+19,'10.5px IM','rgba(230,230,236,0.8)');uiText('▾',rx-114,y+19,'9px IM','rgba(230,230,236,0.5)');
  uiText('↶  ↷',rx-292,y+19,'13px IM','rgba(230,230,236,0.45)');
  return {x:x+10,y:y+46};
}

