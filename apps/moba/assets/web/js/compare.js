// compare.js
// Side-by-side compare view, one GL context per cell.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

// ----------------------------------------------------------------- compare
// One WebGL context PER CELL. They keep their own buffers, so orbiting is a
// redraw rather than re-uploading every model each frame -- capped at 8 because
// contexts are a scarce browser resource and the modal may hold one too.
const CMP_MAX = 8;
const CMP = { geo:new Map(), yaw:0.9, pitch:0.35, dist:2.8, kind:'model' };

async function buildCompare(){
  disposeCells();
  syncCmpKind();
  const same = [...S.star].filter(p=>kindOf(p)===CMP.kind);
  const list = same.slice(0, CMP_MAX);
  const grid = $('cgrid');
  $('cnote').textContent = same.length > CMP_MAX
    ? 'showing ' + CMP_MAX + ' of ' + same.length : '';
  if(!list.length){
    grid.innerHTML = '<div class=cempty>nothing starred of this kind</div>';
    return;
  }
  for(const p of list){
    const cell = document.createElement('div');
    cell.className = 'ccell';
    cell.innerHTML = (kindOf(p)==='texture' ? '<div class=cview></div>'
                                            : '<canvas class=cview></canvas>')
                   + '<div class=cname></div>';
    cell.querySelector('.cname').textContent = p.split('\\').pop();
    cell._path = p;
    grid.append(cell);
  }
  for(const cell of grid.children){
    const p = cell._path;
    if(kindOf(p)==='texture'){
      const v = cell.querySelector('.cview');
      v.style.backgroundImage = 'url("/tex/'+encodeURIComponent(p)+'")';
      const px = (v.clientWidth||220)*(S.uv/PATCH);
      v.style.backgroundSize = px+'px '+px+'px';
      continue;
    }
    if(!CMP.geo.has(p)){
      const g = await (await fetch('/api/model/'+encodeURIComponent(p))).json();
      CMP.geo.set(p, (g.error||g.empty) ? null : g);
    }
    const g = CMP.geo.get(p);
    const c = cell.querySelector('canvas');
    if(!g){ cell.querySelector('.cname').textContent += '  \u2014 no geometry';
            continue; }
    const R = Rend(c);
    if(!R) continue;
    cell._R = R; cell._g = g; cell._texs = await prefetch(R, g);
  }
  bindCompare();
}

async function openCompare(){
  // Opens on the current tab's kind, but the header can switch it: a texture
  // swatch and a 3D model share no scale or camera, so they never mix in one
  // grid -- which is not the same as being stuck with whichever tab you left.
  CMP.kind = S.kind;
  if([...S.star].filter(p=>kindOf(p)===CMP.kind).length < 2)
    CMP.kind = CMP.kind==='model' ? 'texture' : 'model';
  if([...S.star].filter(p=>kindOf(p)===CMP.kind).length < 2)
    return toast('star at least 2 assets of one kind to compare');
  $('cmp').classList.add('show');
  $('cdim').checked = S.dims;
  $('cref').value = S.ref;
  $('ccols').value = S.ccols;
  applyCmpCols();
  await buildCompare();
}

function applyCmpCols(){
  const g = $('cgrid');
  const w = g.clientWidth || innerWidth;
  const fit = Math.max(1, Math.floor(w/160));
  const n = S.ccols==='auto' ? 0 : Math.min(+S.ccols, fit);
  g.style.gridTemplateColumns = n
    ? 'repeat('+n+',1fr)'
    : 'repeat(auto-fit,minmax(min(220px,100%),1fr))';
}

function bindCompare(){
  const cells = [...$('cgrid').children].filter(c=>c._R);
  if(!cells.length) return;
  // the common yard basis is the biggest asset in the set, so it fills the
  // frame and everything else is drawn against it
  const maxY = Math.max(...cells.map(c=>Math.max.apply(null, c._g.size)));
  const dpr = Math.min(devicePixelRatio||1, 2);
  for(const cell of cells){
    const c = cell._R.canvas;
    c.width = Math.max(1, Math.round(c.clientWidth*dpr));
    c.height = Math.max(1, Math.round(c.clientHeight*dpr));
    const rel = $('crel').checked
      ? Math.max.apply(null, cell._g.size)/maxY : 1;
    bindGeo(cell._R, cell._g, cell._texs, rel);
    cell._R.refGeo = cell._g;
    cell._R.refYards = S.ref;
  }
  drawCompare();
}

function drawCompare(){
  for(const cell of $('cgrid').children)
    if(cell._R) render(cell._R, CMP.yaw, CMP.pitch, CMP.dist, S.dims);
}

function disposeCells(){
  for(const cell of $('cgrid').children){
    if(!cell._R) continue;
    const ext = cell._R.gl.getExtension('WEBGL_lose_context');
    if(ext) ext.loseContext();
    cell._R = null;
  }
  $('cgrid').innerHTML = '';
}

function closeCompare(){
  disposeCells();
  $('cmp').classList.remove('show');
}

function syncCmpKind(){
  for(const [id,k] of [['ckt','texture'],['ckm','model']]){
    const n = [...S.star].filter(p=>kindOf(p)===k).length;
    $(id).textContent = (k==='texture' ? 'textures ' : 'models ') + '(' + n + ')';
    $(id).classList.toggle('on', CMP.kind===k);
  }
}

(function(){
  const g = $('cgrid');
  let last = null;
  g.onpointerdown = e=>{
    if(!e.target.closest('.ccell')) return;   // gutter: let the grid scroll
    g.setPointerCapture(e.pointerId);
    last = [e.clientX, e.clientY]; };
  g.onpointermove = e=>{
    if(!last) return;
    CMP.yaw   -= (e.clientX-last[0])*0.01;
    CMP.pitch  = Math.max(-1.5, Math.min(1.5, CMP.pitch+(e.clientY-last[1])*0.01));
    last = [e.clientX, e.clientY];
    drawCompare();
  };
  const up = ()=>{ last = null; };
  g.onpointerup = up; g.onpointercancel = up;
  g.onwheel = e=>{
    if(!e.target.closest('.ccell')) return;   // gutter: scroll, do not zoom
    e.preventDefault();
    CMP.dist = Math.max(DIST_NEAR, Math.min(DIST_FAR,
                        CMP.dist*Math.exp(e.deltaY*0.001)));
    drawCompare(); };
})();
$('cmpbtn').onclick = openCompare;
$('ckt').onclick = ()=>{ CMP.kind='texture'; buildCompare(); };
$('ckm').onclick = ()=>{ CMP.kind='model';   buildCompare(); };
$('cclose').onclick = closeCompare;
$('crel').onchange = bindCompare;
$('cdim').onchange = e=>{
  S.dims = e.target.checked;
  localStorage.setItem('dims', S.dims?'1':'0');
  drawCompare();
};
$('cref').onchange = e=>{
  S.ref = +e.target.value;
  localStorage.setItem('ref', S.ref);
  for(const cell of $('cgrid').children) if(cell._R) cell._R.refYards = S.ref;
  drawCompare();
};
$('ccols').onchange = e=>{
  S.ccols = e.target.value;
  localStorage.setItem('ccols', S.ccols);
  applyCmpCols();
  // cell width drives the canvas backing store, so rebind after the relayout
  requestAnimationFrame(bindCompare);
};
