// viewer.js
// Modal renderer state: orbit, spin, zoom, dimensions read-out.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

const MVs = { R:null, yaw:0.9, pitch:0.35, dist:2.6 };
function mDraw(){ if(MVs.R) render(MVs.R, MVs.yaw, MVs.pitch, MVs.dist, S.dims); }
function mSize(){
  const R=MVs.R; if(!R) return;
  const dpr=Math.min(devicePixelRatio||1,2);
  R.canvas.width=R.canvas.clientWidth*dpr;
  R.canvas.height=R.canvas.clientHeight*dpr;
}
// A turntable about Z, the model's up axis. Driven by elapsed time rather than
// a per-frame constant so the speed is the same on a 60Hz and a 120Hz display.
let spinRAF=null, spinPrev=0;
function spinTick(ts){
  if(!S.spin || !MVs.R){ spinRAF=null; return; }
  if(spinPrev) MVs.yaw += S.spinSpeed*S.spinDir*(ts-spinPrev)/1000*Math.PI/180;
  spinPrev = ts;
  mDraw();
  spinRAF = requestAnimationFrame(spinTick);
}
function startSpin(){
  if(spinRAF || !S.spin || !MVs.R) return;
  spinPrev = 0;                     // first frame only sets the clock
  spinRAF = requestAnimationFrame(spinTick);
}
function stopSpin(){
  if(spinRAF) cancelAnimationFrame(spinRAF);
  spinRAF = null;
}

function spread(pts){
  const [a,b] = [...pts.values()];
  return Math.hypot(a[0]-b[0], a[1]-b[1]) || 1;
}

// Pointer events, not mouse events: one code path covers mouse, touch and pen,
// and setPointerCapture keeps a drag alive outside the canvas WITHOUT the
// window-level listeners the mouse version added on every single model opened.
function glOrbit(c){
  const pts = new Map();
  let last = null, pinch = 0;
  c.style.touchAction = 'none';        // or the browser pans the page instead
  c.onpointerdown = e=>{
    c.setPointerCapture(e.pointerId);
    pts.set(e.pointerId, [e.clientX, e.clientY]);
    if(pts.size === 1) last = [e.clientX, e.clientY];
    if(pts.size === 2) pinch = spread(pts);
    e.preventDefault();
  };
  c.onpointermove = e=>{
    if(!pts.has(e.pointerId)) return;
    pts.set(e.pointerId, [e.clientX, e.clientY]);
    if(pts.size >= 2){
      const d = spread(pts);
      if(pinch) MVs.dist = Math.max(DIST_NEAR, Math.min(DIST_FAR,
                                                        MVs.dist*(pinch/d)));
      pinch = d; syncZoom(); mDraw(); return;
    }
    if(!last) return;
    MVs.yaw   -= (e.clientX-last[0])*0.01;
    MVs.pitch  = Math.max(-1.5, Math.min(1.5, MVs.pitch+(e.clientY-last[1])*0.01));
    last = [e.clientX, e.clientY];
    mDraw();
  };
  const up = e=>{
    pts.delete(e.pointerId);
    if(pts.size < 2) pinch = 0;
    last = pts.size ? [...pts.values()][0] : null;
  };
  c.onpointerup = up; c.onpointercancel = up;
  c.onwheel = e=>{ e.preventDefault();
    MVs.dist = Math.max(DIST_NEAR, Math.min(DIST_FAR,
                        MVs.dist*Math.exp(e.deltaY*0.001)));
    syncZoom(); mDraw(); };
}
function showDims(g){
  const box=$('mdims');
  // A model ALWAYS reserves this row and only its visibility changes. Toggling
  // display removes it from the layout, and a centred modal then jumps by half
  // the height difference on every click.
  if(!g){ box.style.display='none'; return; }
  const [x,y,z]=g.size;
  let html = '';
  if(S.dims) html += '<span class=x>&#9473; X <b>'+x.toFixed(2)+'</b> yd</span>'
    + '<span class=y>&#9473; Y <b>'+y.toFixed(2)+'</b> yd</span>'
    + '<span class=z>&#9473; Z <b>'+z.toFixed(2)+'</b> yd (height)</span>'
    + '<span>' + g.n_verts + ' verts &middot; ' + g.n_tris
    + (g.capped?'+':'') + ' tris</span>';
  if(S.ref) html += '<span class=ref>&#9611; ' + S.ref + ' yd ref &middot; this is '
    + (z/S.ref).toFixed(1) + '&times;</span>';
  // gen_blockout.py divides by the COLLISION box, so the number that sizes this
  // on the map is not always the height above.
  const m = mmeta;
  if(S.dims && m && m.scale_h && Math.abs(m.scale_h - z) > 0.05)
    html += '<span class=warn>scales by ' + m.scale_h + ' yd</span>';
  box.innerHTML = html;
  box.style.display='flex';
  box.style.visibility = (S.dims || S.ref) ? 'visible' : 'hidden';
}
$('mdim') && ($('mdim').onchange = e=>{ S.dims=e.target.checked;
  localStorage.setItem('dims', S.dims?'1':'0');
  showDims(mgeo); mDraw(); });
const DIST_NEAR = 0.6, DIST_FAR = 12;
const zoomToDist = z => DIST_FAR*Math.pow(DIST_NEAR/DIST_FAR, z/100);
const distToZoom = d => 100*Math.log(d/DIST_FAR)/Math.log(DIST_NEAR/DIST_FAR);
function syncZoom(){ $('mzoom').value = Math.round(distToZoom(MVs.dist)); }

const PRESET = {angled:[0.9,0.35], front:[0,0.15],
                side:[Math.PI/2,0.15], top:[0,1.45]};
