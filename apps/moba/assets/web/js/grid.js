// grid.js
// The card grid: building cards, paging, metadata, starring.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

function fill(card){
  const p = card.dataset.path;
  if(kindOf(p)==='texture'){
    const v = card.querySelector('.view');
    v.style.backgroundImage = 'url("/tex/'+encodeURIComponent(p)+'")';
    sizeSwatch(v);
  } else {
    enqueue(card);
  }
}

function sizeSwatch(v){
  // Measured, not CELL: a card is only 220px wide at "auto", and the whole
  // point of the tiling is that a card spans PATCH yards of wall whatever its
  // width. Falls back to the constant if it is asked before layout exists,
  // which would otherwise size every tile to 0 and show nothing.
  const px = (v.clientWidth || CELL)*(S.uv/PATCH);
  v.style.backgroundSize = px+'px '+px+'px';
}

function applyBg(){
  document.documentElement.style.setProperty('--viewbg', S.bg);
  // The clear colour is baked into each render, so thumbnails and the modal
  // both have to be drawn again -- a CSS variable alone only repaints the
  // element BEHIND the canvas, which the opaque clear then covers.
  redrawThumbs();
  mDraw();
}

function applyCols(){
  // A fixed column count has to be capped to what the viewport can actually
  // show: 8 columns on a phone is eight 45px slivers. min(CELL,100%) keeps the
  // auto track from overflowing a container narrower than one card.
  const w = $('grid').clientWidth || innerWidth;
  const fit = Math.max(1, Math.floor(w/120));
  const n = S.cols==='auto' ? 0 : Math.min(+S.cols, fit);
  $('grid').style.gridTemplateColumns = n
    ? 'repeat('+n+',1fr)'
    : 'repeat(auto-fill,minmax(min('+CELL+'px,100%),1fr))';
}

// Width drives both the tile size and the thumbnail's backing store, so a
// column change has to redo both -- after layout, hence the rAF.
function resized(){
  applyCols();
  requestAnimationFrame(()=>{
    document.querySelectorAll('.card .view').forEach(v=>{
      if(v.tagName!=='CANVAS') sizeSwatch(v); });
    redrawThumbs();
  });
}

function paint(g, c, proj){
  if(!g||!c) return;
  const w=c.width=c.clientWidth*2, h=c.height=c.clientHeight*2;
  const ax = {front:[1,2],side:[0,2],top:[0,1]}[proj];
  // null when the canvas already holds a context of another type; that is a
  // caller error, but it must not take the page down with it
  const x = c.getContext('2d');
  if(!x) return;
  x.clearRect(0,0,w,h);
  // ONE fill PER TRIANGLE. Batching them into a single path fills the union
  // once under nonzero winding, and overlap -- which is the only depth cue a
  // flat silhouette has -- disappears entirely.
  x.fillStyle='rgba(220,228,245,0.13)';
  const s = Math.min(w,h)/1024, ox=(w-1024*s)/2, oy=(h-1024*s)/2;
  const V=g.verts, T=g.tris;
  for(let i=0;i<T.length;i+=3){
    x.beginPath();
    for(let k=0;k<3;k++){
      const b=T[i+k]*3;
      const px=ox+V[b+ax[0]]*s, py=h-oy-V[b+ax[1]]*s;
      k?x.lineTo(px,py):x.moveTo(px,py);
    }
    x.fill();
  }
}
const draw = card => paint(card._geo, card.querySelector('canvas'), S.proj);

// Starring changes ONE card's state, so it updates that card in place. Calling
// go() here rebuilt the whole grid and reset scrollTop, which threw away your
// position on the page -- from the modal it did so in every mode.
function toggleStar(p){
  const on = !S.star.has(p);
  on ? S.star.add(p) : S.star.delete(p);
  saveStar();
  for(const el of $('grid').querySelectorAll('.card')){
    if(el.dataset.path !== p) continue;
    el.classList.toggle('star', on);
    // Hidden rather than removed: in the favorites view an unstarred card no
    // longer belongs, but re-starring it straight afterwards has to bring it
    // back, and a removed node cannot come back without a rebuild.
    if(S.mode === 'favorites') el.hidden = !on;
  }
  if(mpath === p) $('mstar').className = on ? 'on starred' : '';
  renderLists();
  if(S.mode === 'favorites'){
    $('pg').textContent = S.star.size + ' in "'+S.list+'"';
    const left = $('grid').querySelector('.card:not([hidden])');
    if(!left && !$('grid').querySelector('.empty'))
      $('grid').insertAdjacentHTML('beforeend',
        '<div class=empty>nothing yet</div>');
    else if(left) $('grid').querySelector('.empty')?.remove();
  }
}

function card(p){
  const leaf = p.split('\\').pop().replace(/\.(blp|m2)$/,'');
  const el = document.createElement('div');
  el.className='card'+(S.star.has(p)?' star':'');
  el.dataset.path=p;
  el.innerHTML = (kindOf(p)==='texture'?'<div class=view></div>'
                                       :'<canvas class=view></canvas>')
    + (S.mode==='browse'?'':'<div class=tag>'+kindOf(p)+'</div>')
    + '<div class=pin>★</div>'
    + '<div class=meta><b>'+leaf+'</b>'
    + '<span class=path title="click to copy this path">'+p+'</span></div>';
  el.querySelector('.pin').onclick = ev=>{
    ev.stopPropagation();
    // shift extends from the last one you starred, so building a candidate
    // list is two clicks rather than one per asset
    if(ev.shiftKey && lastStar){
      const cards = [...$('grid').querySelectorAll('.card')];
      const a = cards.findIndex(c=>c.dataset.path===lastStar);
      const b = cards.indexOf(el);
      if(a>=0 && b>=0){
        const on = !S.star.has(p);
        for(let i=Math.min(a,b); i<=Math.max(a,b); i++){
          const q = cards[i].dataset.path;
          if(S.star.has(q) !== on) toggleStar(q);
        }
        return;
      }
    }
    lastStar = p;
    toggleStar(p);
  };
  el.querySelector('.path').onclick = ev=>{
    ev.stopPropagation();          // the card itself opens the modal
    copyAnd(p, 'path');
  };
  el.onclick = ()=>open_(p, el);
  return el;
}

async function go(){
  // facets() first: it can clear a selection that no longer exists, and tree()
  // filters folders by those same values -- the other order builds the sidebar
  // from a value that is about to be discarded.
  await facets();
  await tree();
  const grid = $('grid'); grid.innerHTML=''; grid.scrollTop=0;
  let paths, total;
  if(S.mode==='browse'){
    const u = '/api/query?kind='+S.kind+'&cat='+encodeURIComponent(S.cat)
            + '&q='+encodeURIComponent(S.q)+'&page='+S.page
            + '&role='+encodeURIComponent(S.role)
            + '&theme='+encodeURIComponent(S.theme)+'&all='+(S.all?1:0)
            + nfQuery();
    const r = await (await fetch(u)).json();
    paths = r.paths; total = r.total;
    S.pages = Math.max(1, Math.ceil(total/r.page_size));
    $('pg').textContent = total+' · page '+(S.page+1)+'/'+S.pages;
  } else {
    const r = await postFiltered('/api/query', {page:S.page});
    paths = r.paths || []; total = r.total || 0;
    S.pages = Math.max(1, Math.ceil(total/(r.page_size||PAGE_SIZE)));
    $('pg').textContent = total
      + (S.mode==='favorites' ? ' in "'+S.list+'"' : ' viewed')
      + (S.pages>1 ? '  \u00B7 page '+(S.page+1)+'/'+S.pages : '');
  }
  syncControls();
  if(!paths.length){
    grid.innerHTML = '<div class=empty>'
      + (S.mode==='browse'?'nothing here':'nothing yet') + '</div>';
    stateToHash();
    return;
  }
  for(const p of paths){ const el=card(p); grid.append(el); io.observe(el); }
  stateToHash();
  for(const k of ['model','texture']){
    const sub = paths.filter(p=>kindOf(p)===k);
    if(sub.length) meta(sub, k);
  }
}

async function meta(paths, kind){
  const m = await (await fetch('/api/meta?kind='+kind+'&paths='+
              encodeURIComponent(JSON.stringify(paths)))).json();
  for(const el of $('grid').querySelectorAll('.card')){
    const d = m[el.dataset.path]; if(!d) continue;
    el._meta = d;
    const line = document.createElement('div');
    if(kind==='model'){
      // bound_tris 0 is the one fact that decides whether a model can ever be
      // collision on the map: vmap4extractor skips a model without them.
      line.innerHTML = 'h '+(d.height??'?')+' \u00B7 a '+(d.aspect??'?')+' \u00B7 '
        + (d.bound_tris ? d.bound_tris+' col'
                        : '<b class=nocol>no collision</b>');
    } else {
      line.textContent = (d.w ? d.w+'\u00D7'+d.h : '?')
        + (d.alpha ? ' \u00B7 cut-out' : ' \u00B7 opaque');
    }
    el.querySelector('.meta').append(line);
  }
  sortGrid();
}

function sortGrid(){
  if(S.sort==='path') return;
  const key = {height:c=>-(c._meta?.height??-1), aspect:c=>-(c._meta?.aspect??-1),
               tris:c=>-(c._meta?.bound_tris??-1)}[S.sort];
  const g=$('grid');
  [...g.children].sort((a,b)=>key(a)-key(b)).forEach(c=>g.append(c));
}
