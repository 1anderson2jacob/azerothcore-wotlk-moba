// gl.js
// WebGL: shaders, matrices, buffers, and the thumbnail queue.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

// ------------------------------------------------------- textured 3d viewer
// WebGL1, no libraries, TWO contexts: one bound to the modal canvas for orbiting
// and one offscreen that every grid thumbnail is rendered through and blitted
// from. A context per card would be 200 of them, which no browser will give.
// Anything that fails here falls back to the flat silhouette.

const VS = `
attribute vec3 aPos; attribute vec2 aUv; attribute vec3 aNrm;
uniform mat4 uMvp; varying vec2 vUv; varying vec3 vN;
void main(){ vUv=aUv; vN=aNrm; gl_Position=uMvp*vec4(aPos,1.0); }`;
const FS = `
precision mediump float;
varying vec2 vUv; varying vec3 vN;
uniform sampler2D uTex; uniform float uHasTex; uniform vec3 uTint;
void main(){
  vec4 c = uHasTex > 0.5 ? texture2D(uTex, vUv) : vec4(0.72,0.75,0.80,1.0);
  // Alpha TEST, not blend: doodad foliage is authored as cut-out cards, and
  // blending them needs a depth sort this viewer deliberately does not do.
  if (c.a < 0.35) discard;
  float l = 0.45 + 0.55*max(dot(normalize(vN), normalize(vec3(0.4,0.6,0.9))),0.0);
  gl_FragColor = vec4(c.rgb*l*uTint, 1.0);
}`;

function mul(a,b){ const o=new Float32Array(16);
  for(let i=0;i<4;i++)for(let j=0;j<4;j++){ let v=0;
    for(let k=0;k<4;k++) v+=a[k*4+j]*b[i*4+k]; o[i*4+j]=v; } return o; }
function persp(fov,asp,n,f){ const t=1/Math.tan(fov/2), o=new Float32Array(16);
  o[0]=t/asp; o[5]=t; o[10]=(f+n)/(n-f); o[11]=-1; o[14]=2*f*n/(n-f); return o; }
function lookAt(e,c,u){
  const z=[e[0]-c[0],e[1]-c[1],e[2]-c[2]];
  let l=Math.hypot(...z)||1; z.forEach((v,i)=>z[i]=v/l);
  const x=[u[1]*z[2]-u[2]*z[1], u[2]*z[0]-u[0]*z[2], u[0]*z[1]-u[1]*z[0]];
  l=Math.hypot(...x)||1; x.forEach((v,i)=>x[i]=v/l);
  const y=[z[1]*x[2]-z[2]*x[1], z[2]*x[0]-z[0]*x[2], z[0]*x[1]-z[1]*x[0]];
  return new Float32Array([x[0],y[0],z[0],0, x[1],y[1],z[1],0, x[2],y[2],z[2],0,
    -(x[0]*e[0]+x[1]*e[1]+x[2]*e[2]), -(y[0]*e[0]+y[1]*e[1]+y[2]*e[2]),
    -(z[0]*e[0]+z[1]*e[1]+z[2]*e[2]), 1]);
}

const hex2rgb = h => { const n = parseInt(h.slice(1), 16);
  return [(n>>16 & 255)/255, (n>>8 & 255)/255, (n & 255)/255]; };

const TEX_CAP = 200;          // GL textures held per context
const THUMB_CAP = 1400;       // widest offscreen buffer, in device px

function Rend(canvas){
  const gl = canvas.getContext('webgl',{antialias:true,alpha:false,
                                        preserveDrawingBuffer:true});
  if(!gl) return null;
  const mk=(t,src)=>{ const sh=gl.createShader(t); gl.shaderSource(sh,src);
    gl.compileShader(sh);
    if(!gl.getShaderParameter(sh,gl.COMPILE_STATUS))
      throw new Error(gl.getShaderInfoLog(sh)); return sh; };
  let prog;
  try {
    prog=gl.createProgram();
    gl.attachShader(prog,mk(gl.VERTEX_SHADER,VS));
    gl.attachShader(prog,mk(gl.FRAGMENT_SHADER,FS));
    gl.linkProgram(prog);
    if(!gl.getProgramParameter(prog,gl.LINK_STATUS))
      throw new Error(gl.getProgramInfoLog(prog));
  } catch(e){ console.warn('webgl unavailable:',e); return null; }
  const blank=gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D,blank);
  gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,1,1,0,gl.RGBA,gl.UNSIGNED_BYTE,
                new Uint8Array([200,200,205,255]));
  return {gl, prog, canvas, blank, tex:new Map(), order:[],
          uint:!!gl.getExtension('OES_element_index_uint'), buf:null};
}

function loadTex(R, path){
  if(R.tex.has(path)) return R.tex.get(path);
  const gl=R.gl;
  const pr = new Promise(res=>{
    const img=new Image();
    img.onerror=()=>res(null);
    img.onload=()=>{
      const t=gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D,t);
      // NO flip, and the UVs go in raw. WoW's v runs down the texture and an
      // unflipped upload puts the image's top row at t=0, so the two already
      // agree. blender_staging_setup.py:438 writes (u, 1.0 - v) only because
      // Blender's V runs the other way -- copying that here inverts every model.
      gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,img);
      const pot=x=>(x&(x-1))===0;
      if(pot(img.width)&&pot(img.height)){
        gl.generateMipmap(gl.TEXTURE_2D);
        gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR_MIPMAP_LINEAR);
      } else {
        // WebGL1 forbids REPEAT on a non-power-of-two texture; clamping is the
        // only legal wrap, so a tiling UV would sample the edge forever.
        gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_S,gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_WRAP_T,gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.LINEAR);
      }
      res(t);
    };
    img.src='/tex/'+encodeURIComponent(path);
  });
  R.tex.set(path,pr); R.order.push(path);
  while(R.order.length>TEX_CAP){
    const old=R.order.shift();
    const q=R.tex.get(old); R.tex.delete(old);
    Promise.resolve(q).then(t=>{ if(t) gl.deleteTexture(t); });
  }
  return pr;
}

// Textures FIRST and separately, because fetching them is the only async step.
// bindGeo below must not contain an await: three thumbnails share one renderer,
// so yielding midway lets the next model overwrite R.buf and delete the buffers
// this one is about to bind -- which is why only some cards used to draw.
async function prefetch(R, g){
  if(!g.uvs) return null;
  return Promise.all(g.parts.map(pt => pt.tex ? loadTex(R, pt.tex) : null));
}

function bindGeo(R, g, texs, rel){
  const gl=R.gl;
  if(!g.uvs || !texs) return false;
  const maxI=Math.max.apply(null,g.tris);
  if(maxI>65535 && !R.uint) return false;
  const IArr = maxI>65535 ? Uint32Array : Uint16Array;
  const itype = maxI>65535 ? gl.UNSIGNED_INT : gl.UNSIGNED_SHORT;
  const n=g.n_verts, P=new Float32Array(n*3), U=new Float32Array(n*2),
        N=new Float32Array(n*3);
  let mn=[1e9,1e9,1e9], mx=[-1e9,-1e9,-1e9];
  for(let i=0;i<n;i++) for(let k=0;k<3;k++){
    const v=g.verts[i*3+k]; if(v<mn[k])mn[k]=v; if(v>mx[k])mx[k]=v; }
  const ext=Math.max(mx[0]-mn[0],mx[1]-mn[1],mx[2]-mn[2])||1;
  // rel < 1 shrinks this model relative to the biggest in a compare set, so
  // several models can share one view at TRUE relative size.
  const rs = rel || 1;
  for(let i=0;i<n;i++){
    for(let k=0;k<3;k++){
      P[i*3+k]=(g.verts[i*3+k]-(mn[k]+mx[k])/2)/ext*rs;
      N[i*3+k]=(g.norms[i*3+k]-127)/127;
    }
    U[i*2]  =g.uv_lo[0]+g.uvs[i*2]  /4095*g.uv_span;
    U[i*2+1]=g.uv_lo[1]+g.uvs[i*2+1]/4095*g.uv_span;
  }
  if(R.buf) for(const b of R.buf) gl.deleteBuffer(b);
  const mkbuf=(data,t)=>{ const b=gl.createBuffer();
    gl.bindBuffer(t||gl.ARRAY_BUFFER,b);
    gl.bufferData(t||gl.ARRAY_BUFFER,data,gl.STATIC_DRAW); return b; };
  const bp=mkbuf(P), bu=mkbuf(U), bn=mkbuf(N);
  const bi=mkbuf(new IArr(g.tris), gl.ELEMENT_ARRAY_BUFFER);
  R.buf=[bp,bu,bn,bi];
  R.parts = g.parts.map((pt,i)=>({...pt, tex: texs[i] || null}));
  R.itype=itype; R.ibytes = itype===gl.UNSIGNED_INT?4:2;
  R.attrs={bp,bu,bn,bi};
  // The schematic lives in the SAME normalised space as the mesh above, so it
  // is rebuilt per model rather than scaled at draw time.
  const lo=[],hi=[];
  for(let k=0;k<3;k++){ const h=(mx[k]-mn[k])/(2*ext)*rs; lo.push(-h); hi.push(h); }
  R.nlo = lo; R.nhi = hi; R.rel = rs;
  // Grouped BY AXIS -- 4 edges each, in X, Y, Z order -- so drawBox can colour
  // each group with one draw and every wire states which dimension it measures.
  const L=[], corner=(i,j,k)=>[i?hi[0]:lo[0], j?hi[1]:lo[1], k?hi[2]:lo[2]];
  const edge=(a,b)=>{ L.push(...a,...b); };
  for(const k of [0,1]) for(const j of [0,1]) edge(corner(0,j,k),corner(1,j,k));
  for(const i of [0,1]) for(const k of [0,1]) edge(corner(i,0,k),corner(i,1,k));
  for(const i of [0,1]) for(const j of [0,1]) edge(corner(i,j,0),corner(i,j,1));
  if(R.boxBuf) gl.deleteBuffer(R.boxBuf);
  R.boxBuf = mkbuf(new Float32Array(L));
  return true;
}

function render(R, yaw, pitch, dist, showBox){
  const gl=R.gl, c=R.canvas; if(!R.attrs) return;
  gl.viewport(0,0,c.width,c.height);
  const bg = hex2rgb(S.bg);
  gl.clearColor(bg[0],bg[1],bg[2],1); gl.enable(gl.DEPTH_TEST);
  gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);
  gl.useProgram(R.prog);
  const cp=Math.cos(pitch),
        e=[Math.cos(yaw)*cp*dist, Math.sin(yaw)*cp*dist, Math.sin(pitch)*dist];
  // 0.05/20 rather than 0.01/100: the model is normalised to about a unit and
  // dist is clamped to 12, so a 2000:1 depth range only wasted precision.
  const mvp=mul(persp(0.9,c.width/c.height,0.05,20), lookAt(e,[0,0,0],[0,0,1]));
  gl.uniformMatrix4fv(gl.getUniformLocation(R.prog,'uMvp'),false,mvp);
  const bind=(b,name,sz)=>{ const l=gl.getAttribLocation(R.prog,name);
    if(l<0) return; gl.bindBuffer(gl.ARRAY_BUFFER,b); gl.enableVertexAttribArray(l);
    gl.vertexAttribPointer(l,sz,gl.FLOAT,false,0,0); };
  bind(R.attrs.bp,'aPos',3); bind(R.attrs.bu,'aUv',2); bind(R.attrs.bn,'aNrm',3);
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,R.attrs.bi);
  const hasTex=gl.getUniformLocation(R.prog,'uHasTex');
  const tint=gl.getUniformLocation(R.prog,'uTint');
  gl.uniform3f(tint,1,1,1);
  for(const pt of R.parts){
    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, pt.tex||R.blank);
    gl.uniform1i(gl.getUniformLocation(R.prog,'uTex'),0);
    gl.uniform1f(hasTex, pt.tex?1:0);
    gl.drawElements(gl.TRIANGLES, pt.count, R.itype, pt.start*R.ibytes);
  }
  if(showBox && R.boxBuf) drawBox(R, tint);
  // Independent of the box: a scale post is useful without the wireframe, and
  // gating it on `dimensions` made the scale-ref control do nothing at all.
  if(R.refYards) drawRef(R, R.refGeo, R.refYards);
}

// 1 normalised unit is the model's largest extent, so max(size) yards. That is
// all a reference post needs to be measured in the same space as the mesh.
// Both line passes need this. It used to live only in drawBox, so with the
// wireframe off the reference post inherited the MODEL's texture and vertex
// arrays -- 6 vertices reading a buffer sized for thousands.
function lineMode(R){
  const gl=R.gl;
  const lu=gl.getAttribLocation(R.prog,'aUv'), ln=gl.getAttribLocation(R.prog,'aNrm');
  // Constant attributes instead of arrays. The normal is set ALONG the light
  // direction so the lambert term is exactly 1 and the tint arrives unshaded.
  if(lu>=0){ gl.disableVertexAttribArray(lu); gl.vertexAttrib2f(lu,0,0); }
  if(ln>=0){ gl.disableVertexAttribArray(ln); gl.vertexAttrib3f(ln,0.4,0.6,0.9); }
  gl.uniform1f(gl.getUniformLocation(R.prog,'uHasTex'),0);
}

function drawRef(R, g, yards){
  const gl=R.gl;
  if(!R.nhi || !g || !yards) return;
  lineMode(R);
  const per = 1/Math.max.apply(null, g.size);
  const h = yards*per*(R.rel||1);
  const x = R.nhi[0]+0.15, z0 = R.nlo[2], t = 0.06;
  const L = [x,0,z0,  x,0,z0+h,
             x-t,0,z0, x+t,0,z0,
             x-t,0,z0+h, x+t,0,z0+h];
  if(R.refBuf) gl.deleteBuffer(R.refBuf);
  R.refBuf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, R.refBuf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(L), gl.STATIC_DRAW);
  const lp=gl.getAttribLocation(R.prog,'aPos');
  gl.enableVertexAttribArray(lp);
  gl.vertexAttribPointer(lp,3,gl.FLOAT,false,0,0);
  gl.uniform3f(gl.getUniformLocation(R.prog,'uTint'), 1.0, 0.78, 0.34);
  gl.drawArrays(gl.LINES, 0, 6);
}

function drawBox(R, tint){
  const gl=R.gl;
  lineMode(R);
  const lp=gl.getAttribLocation(R.prog,'aPos');
  gl.bindBuffer(gl.ARRAY_BUFFER,R.boxBuf);
  gl.enableVertexAttribArray(lp);
  gl.vertexAttribPointer(lp,3,gl.FLOAT,false,0,0);
  gl.uniform3f(tint,1.00,0.42,0.42); gl.drawArrays(gl.LINES, 0,8);   // X
  gl.uniform3f(tint,0.49,0.86,0.49); gl.drawArrays(gl.LINES, 8,8);   // Y
  gl.uniform3f(tint,0.42,0.71,1.00); gl.drawArrays(gl.LINES,16,8);   // Z
}

// --- the offscreen thumbnail renderer, plus its queue ----------------------
let TV = null, TVdead = false;
function thumbRend(){
  if(TV || TVdead) return TV;
  const c=document.createElement('canvas'); c.width=440; c.height=300;
  TV=Rend(c); TVdead=!TV; return TV;
}

const Q = { list:[], busy:0, max:3 };
function enqueue(card){ Q.list.push(card); pump(); }
function pump(){
  while(Q.busy<Q.max && Q.list.length){
    const card=Q.list.shift();
    if(!card.isConnected) continue;      // grid re-rendered under us
    Q.busy++;
    thumb(card).catch(()=>{}).then(()=>{ Q.busy--; pump(); });
  }
}

async function thumb(card){
  const p=card.dataset.path;
  let g=card._geo;
  if(!g){
    g=await (await fetch('/api/model/'+encodeURIComponent(p))).json();
    if(!card.isConnected) return;
    if(g.error||g.empty){
      card.querySelector('.meta').insertAdjacentHTML('beforeend',
        '<div class='+(g.error?'err':'')+'>'+(g.error||g.note)+'</div>');
      card._geo={empty:true};
      return;
    }
    card._geo=g;
    card.querySelector('.meta').insertAdjacentHTML('beforeend',
      '<div>'+g.size.map(n=>n.toFixed(1)).join(' × ')+' yd &middot; '
      + g.n_tris + (g.capped?'+':'') + ' tris</div>');
  }
  if(g.empty) return;
  const c=card.querySelector('canvas'); if(!c) return;
  const R=thumbRend();
  const texs = R ? await prefetch(R,g) : null;
  if(!card.isConnected) return;
  // Everything from here down is synchronous, so no other queued model can
  // touch the shared renderer between the bind and the blit.
  if(!R || !bindGeo(R,g,texs)){ draw(card); return; }
  // Shape the offscreen buffer like the card BEFORE rendering: render() takes
  // its projection aspect from the buffer, so a fixed 440x300 source blitted
  // into a differently shaped card is exactly how the models got stretched.
  // Reallocation is skipped when unchanged -- every card on a page is the same
  // size, so this costs one resize per column change, not one per card.
  {
    // measured off the CANVAS, the same element the blit targets -- the card's
    // border makes card.clientWidth a different number
    const d=Math.min(devicePixelRatio||1,2);
    let bw=Math.max(1,Math.round(c.clientWidth*d)),
        bh=Math.max(1,Math.round(c.clientHeight*d));
    if(bw>THUMB_CAP){ bh=Math.max(1,Math.round(bh*THUMB_CAP/bw)); bw=THUMB_CAP; }
    if(R.canvas.width!==bw || R.canvas.height!==bh){
      R.canvas.width=bw; R.canvas.height=bh;
    }
  }
  const v=PRESET[S.proj]||PRESET.angled;
  render(R, v[0], v[1], 2.6);
  const dpr=Math.min(devicePixelRatio||1,2);
  const cw=Math.max(1,Math.round(c.clientWidth*dpr)),
        ch=Math.max(1,Math.round(c.clientHeight*dpr));
  c.width=cw; c.height=ch;
  c.getContext('2d').drawImage(R.canvas,0,0,R.canvas.width,R.canvas.height,
                               0,0,cw,ch);
}

// Re-render every thumbnail that already has geometry. Textures are cached per
// context, so this is a re-upload and a draw, not a refetch.
function redrawThumbs(){
  for(const card of document.querySelectorAll('.card')){
    if(kindOf(card.dataset.path)==='model' && card._geo && !card._geo.empty)
      enqueue(card);
  }
}

// --- the modal's own interactive context -----------------------------------
