// state.js
// Shared state object, saved lists, and the storage they live in.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

const S = { kind:'texture', mode:'browse', cat:'', q:'', page:0, uv:4,
            proj:'angled', sort:'path', role:'', theme:'', all:false,
            fsort:'count', dims:localStorage.getItem('dims')==='1',
            nf:{hmin:'',hmax:'',col:'',res:'',alpha:''},
            cols:localStorage.getItem('cols')||'auto',
            ccols:localStorage.getItem('ccols')||'auto',
            bg:localStorage.getItem('bg')||'#101014',
            full:localStorage.getItem('full')==='1',
            ref:+(localStorage.getItem('ref')||0),
            spin:localStorage.getItem('spin')==='1',
            spinSpeed:+(localStorage.getItem('spinSpeed')||40),
            spinDir:+(localStorage.getItem('spinDir')||1),
            lists:loadLists(), list:localStorage.getItem('list')||'Favorites',
            star:new Set(),
            hist:JSON.parse(localStorage.getItem('hist')||'[]') };
const $ = i => document.getElementById(i);

// Named lists. `star` is the ACTIVE list held as a Set -- one working copy, so
// every existing membership test stays a Set lookup.
function loadLists(){
  let L=null;
  try { L = JSON.parse(localStorage.getItem('lists')||'null'); } catch(e){}
  if(!L || typeof L!=='object' || !Object.keys(L).length){
    // pre-dates named lists: the single starred set becomes the default list
    let old=[]; try { old = JSON.parse(localStorage.getItem('star')||'[]'); } catch(e){}
    L = {Favorites: old};
  }
  return L;
}
const CELL = 220, PATCH = 32;   // card width px, yards of wall it stands for
const HIST_MAX = 50;
const PAGE_SIZE = 200;

// Kind comes from the PATH, not the tab: favorites and history mix both, and a
// card has to render as whatever it actually is.
const kindOf = p => p.endsWith('.blp') ? 'texture' : 'model';

function saveStar(){
  S.lists[S.list] = [...S.star];
  localStorage.setItem('lists', JSON.stringify(S.lists));
  localStorage.setItem('list', S.list);
  $('nstar').textContent = S.star.size;
  syncControls();
}

function renderLists(){
  const sel=$('lsel');
  sel.innerHTML = Object.keys(S.lists).sort()
    .map(n=>'<option value="'+n.replace(/"/g,'&quot;')+'">'+n
            +' ('+S.lists[n].length+')</option>').join('');
  sel.value = S.list;
}

function setList(name){
  if(!(name in S.lists)) return;
  S.list = name;
  S.star = new Set(S.lists[name]);
  saveStar(); renderLists();
  // Choosing a list SHOWS it. Without this, picking one while browsing changed
  // only where the star writes -- the grid never moved, so the control looked
  // broken. setMode already re-runs go(), so this is not a second query.
  if(S.mode !== 'favorites') setMode('favorites'); else go();
}
function pushHist(p){
  S.hist = [p, ...S.hist.filter(x=>x!==p)].slice(0, HIST_MAX);
  localStorage.setItem('hist', JSON.stringify(S.hist));
}
