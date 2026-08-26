// modal.js
// The asset modal itself, plus the YAML export.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

// ------------------------------------------------------------------- modal

let mpath = null, mgeo = null, mmeta = null;
function open_(p, el){
  mpath = p; pushHist(p);
  $('mname').textContent = p.split('\\').pop();
  $('mfoot').innerHTML = '';
  $('mfoot').append(Object.assign(document.createElement('span'),
    {className:'path', textContent:p, title:'click to copy this path'}));
  $('mfoot').firstChild.onclick = ()=>copyAnd(p, 'path');
  $('mstar').className = S.star.has(p)?'on starred':'';
  // Shown FIRST: mtile() reads clientWidth, and a display:none modal measures
  // 0, which silently sized every tile to 0px.
  $('modal').classList.add('show');
  $('mdim').checked = S.dims;
  $('mref').value = S.ref;
  stateToHash();
  document.querySelector('.mbox').classList.toggle('full', S.full);
  $('mfull').classList.toggle('on', S.full);
  const wrap = $('mviewwrap');
  if(kindOf(p)==='texture'){
    $('mtiled').style.display=''; $('mdimwrap').style.display='none';
    $('mdims').style.display='none'; $('mctl').style.display='none';
    stopSpin(); releaseR();
    wrap.innerHTML = '<div id=mview></div>';
    mtile();
  } else {
    $('mtiled').style.display='none'; $('mdimwrap').style.display='';
    $('mctl').style.display='flex';
    wrap.innerHTML = '<canvas id=mview></canvas>';
    const show = g => { mgeo = g;
      requestAnimationFrame(async ()=>{
        const c=$('mview');
        MVs.R = MVs.R && MVs.R.canvas===c ? MVs.R : Rend(c);
        const texs = MVs.R ? await prefetch(MVs.R,g) : null;
        if(MVs.R && bindGeo(MVs.R,g,texs)){
          MVs.R.refGeo = g; MVs.R.refYards = S.ref;
          mSize(); glOrbit(c); showDims(g); syncZoom();
          setPreset(S.proj); startSpin();
          fetch('/api/meta?kind=model&paths='+encodeURIComponent(JSON.stringify([p])))
            .then(r=>r.json()).then(m=>{ mmeta = m[p]; showDims(mgeo); });
        } else {
          // A canvas keeps its first context type for life, so the silhouette
          // needs a NEW element -- the old one is a WebGL canvas for good.
          releaseR();
          wrap.innerHTML = '<canvas id=mview></canvas>';
          showDims(null);
          paint(g, $('mview'), S.proj);
          $('mctl').style.display='none';
        }
      }); };
    if(el && el._geo) show(el._geo);
    else fetch('/api/model/'+encodeURIComponent(p)).then(r=>r.json()).then(g=>{
      if(!g.error && !g.empty) show(g); });
  }
}
// A modal open makes a fresh canvas and a fresh WebGL context. Dropping the
// reference does NOT free the context -- a browser allows only about 16, and
// once they run out Rend() fails on a canvas that already holds one, which is
// how the 2D fallback ended up calling getContext('2d') and getting null.
function releaseR(){
  if(MVs.R && MVs.R.gl){
    const ext = MVs.R.gl.getExtension('WEBGL_lose_context');
    if(ext) ext.loseContext();
  }
  MVs.R = null;
}

function mtile(){
  const v = $('mview'); if(!v||kindOf(mpath)!=='texture') return;
  v.style.backgroundImage = 'url("/tex/'+encodeURIComponent(mpath)+'")';
  if($('mtile').checked){
    // The modal is a wider window on the same wall, so one tile has to keep the
    // grid's yards-per-pixel or the repeat rate would read differently here.
    const px = v.clientWidth*(S.uv/PATCH);
    v.style.backgroundSize = px+'px '+px+'px';
    v.style.backgroundRepeat='repeat';
  } else {
    v.style.backgroundSize='contain';
    v.style.backgroundRepeat='no-repeat';
  }
}
function setFull(on){
  S.full = on;
  localStorage.setItem('full', on?'1':'0');
  document.querySelector('.mbox').classList.toggle('full', on);
  $('mfull').classList.toggle('on', on);
  if(!mpath) return;
  // The canvas backing store and the tile size are both derived from the
  // element's measured width, so both have to be redone AFTER the relayout.
  requestAnimationFrame(()=>{
    if(kindOf(mpath)==='texture') mtile();
    else { mSize(); mDraw(); }
  });
}

function mclose(){ stopSpin(); $('modal').classList.remove('show');
                   mpath=mgeo=mmeta=null; releaseR(); stateToHash();
                   if(S.mode==='history') go(); }
$('mclose').onclick = mclose;
$('mtile').onchange = mtile;
$('mstar').onclick = ()=>{ if(mpath) toggleStar(mpath); };
$('mzoom').oninput = e=>{ MVs.dist = zoomToDist(+e.target.value); mDraw(); };
$('mref').onchange = e=>{ S.ref = +e.target.value;
  localStorage.setItem('ref', S.ref);
  if(MVs.R) MVs.R.refYards = S.ref;
  showDims(mgeo); mDraw(); };
$('mspin').onchange = e=>{ S.spin=e.target.checked;
  localStorage.setItem('spin', S.spin?'1':'0');
  S.spin ? startSpin() : stopSpin(); };
$('mspeed').oninput = e=>{ S.spinSpeed=+e.target.value;
  localStorage.setItem('spinSpeed', S.spinSpeed); speedLabel(); };
$('mrev').onclick = ()=>{ S.spinDir = -S.spinDir;
  localStorage.setItem('spinDir', S.spinDir); speedLabel(); };
$('mreset').onclick = ()=>{ MVs.dist=2.6; syncZoom(); setPreset(S.proj); };
function speedLabel(){
  $('mspeedv').textContent = S.spinSpeed+'\u00B0/s '
    + (S.spinDir>0 ? '\u21BB' : '\u21BA');
}
$('mspin').checked = S.spin; $('mspeed').value = S.spinSpeed; speedLabel();
$('mref').value = S.ref;
$('mfull').onclick = ()=>setFull(!S.full);
$('modal').onclick = e=>{ if(e.target===$('modal')) mclose(); };
let focusIdx = -1;
function focusCard(n){
  const cards = [...$('grid').querySelectorAll('.card:not([hidden])')];
  if(!cards.length) return;
  focusIdx = Math.max(0, Math.min(cards.length-1, n));
  cards.forEach((c,i)=>c.classList.toggle('kb', i===focusIdx));
  cards[focusIdx].scrollIntoView({block:'nearest'});
}
function cardsPerRow(){
  const g = $('grid');
  return Math.max(1, getComputedStyle(g).gridTemplateColumns.split(' ').length);
}
addEventListener('keydown', e=>{
  if(e.key==='Escape' && $('cmp').classList.contains('show'))
    return closeCompare();
  if(e.key==='Escape') return mclose();
  // ignore while a field has focus, or typing "f" in the search box would
  // fullscreen the modal
  if(/^(INPUT|SELECT|TEXTAREA)$/.test(e.target.tagName)) return;
  if(e.key==='f' && $('modal').classList.contains('show')) return setFull(!S.full);
  if($('modal').classList.contains('show')) return;   // modal owns the keyboard
  if($('cmp').classList.contains('show')) return;
  const step = {ArrowRight:1, ArrowLeft:-1,
                ArrowDown:cardsPerRow(), ArrowUp:-cardsPerRow()}[e.key];
  if(step){ e.preventDefault(); return focusCard(focusIdx<0 ? 0 : focusIdx+step); }
  const cards = [...$('grid').querySelectorAll('.card:not([hidden])')];
  const cur = cards[focusIdx];
  if(!cur) return;
  if(e.key===' '){ e.preventDefault(); lastStar=cur.dataset.path;
                   toggleStar(cur.dataset.path); }
  if(e.key==='Enter'){ e.preventDefault(); open_(cur.dataset.path, cur); }
});

// ------------------------------------------------------------------ export

function block(kind){
  const list=[...S.star].filter(p=>kindOf(p)===kind);
  if(!list.length) return '';
  if(kind==='texture')
    return '# materials:\n'+list.map(p=>'    '
      + p.split('\\').pop().replace(/\.blp$/,'')
      +": {texture: '"+p+"', uv_scale_yd: "+S.uv+'}').join('\n');
  return '# dressing.families:\n      family_name:\n        height_yd: 5.0\n'
       + '        models:\n'
       + list.map(p=>"          - '"+p+"'").join('\n');
}
function yaml(){
  const out=[block('texture'),block('model')].filter(Boolean);
  return out.length?out.join('\n\n'):'# nothing starred';
}
