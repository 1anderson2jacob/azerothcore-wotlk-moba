// util.js
// Clipboard, toasts, URL state, and the filter query helpers.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

function copyText(t){
  if(navigator.clipboard && isSecureContext)
    return navigator.clipboard.writeText(t).then(()=>true,()=>false);
  const ta=document.createElement('textarea');
  ta.value=t; ta.style.cssText='position:fixed;top:-1000px;opacity:0';
  document.body.append(ta); ta.select();
  let ok=false;
  try { ok=document.execCommand('copy'); } catch(e){}
  ta.remove();
  return Promise.resolve(ok);
}

let toastT=null;
function toast(msg){
  const el=$('toast');
  el.textContent=msg; el.classList.add('show');
  clearTimeout(toastT); toastT=setTimeout(()=>el.classList.remove('show'),1400);
}
function copyAnd(t, what){
  copyText(t).then(ok=>toast(ok ? what+' copied' : 'copy failed'));
}

// ---------------------------------------------------------------- shareable url
// The hash, not the query string: it never reaches the server, so a link costs
// no round trip and cannot 404. Written with replaceState so filtering does not
// bury the back button under one entry per keystroke.
function stateToHash(){
  const p=new URLSearchParams();
  if(S.kind!=='texture') p.set('kind',S.kind);
  if(S.mode!=='browse') p.set('view', S.mode==='favorites'?'list':'history');
  if(S.cat) p.set('cat',S.cat);
  if(S.q) p.set('q',S.q);
  if(S.role) p.set('role',S.role);
  if(S.theme) p.set('theme',S.theme);
  if(S.all) p.set('all','1');
  if(S.page) p.set('page',S.page);
  if(S.uv!==4) p.set('uv',S.uv);
  for(const k of nfKeys()) if(S.nf[k]!=='' && S.nf[k]!=null) p.set(k, S.nf[k]);
  if(mpath) p.set('asset',mpath);
  const h=p.toString();
  history.replaceState(null,'', h ? '#'+h : location.pathname+location.search);
}

// null: no hash at all, leave the defaults alone. '' : a hash with no asset.
function readHash(){
  const p=new URLSearchParams(location.hash.replace(/^#/,''));
  if(![...p.keys()].length) return null;
  S.kind = p.get('kind')==='model' ? 'model' : 'texture';
  const v = p.get('view');
  S.mode = v==='list' ? 'favorites' : (v==='history' ? 'history' : 'browse');
  S.cat = p.get('cat')||'';   S.q = p.get('q')||'';
  S.role = p.get('role')||''; S.theme = p.get('theme')||'';
  S.all = p.get('all')==='1';
  S.page = Math.max(0, parseInt(p.get('page')||'0',10)||0);
  const uv = parseFloat(p.get('uv'));
  if(uv>0) S.uv = uv;
  for(const k of Object.keys(S.nf)) S.nf[k] = p.get(k) ?? '';
  return p.get('asset')||'';
}

function applyControls(){
  $('kt').classList.toggle('on', S.kind==='texture');
  $('km').classList.toggle('on', S.kind==='model');
  for(const [id,v] of [['vb','browse'],['vf','favorites'],['vh','history']])
    $(id).classList.toggle('on', S.mode===v);
  $('q').value = S.q;
  $('fscope').value = S.all ? 'all' : 'map';
  $('fsort').value = S.fsort;
  $('uv').value = S.uv; $('uvv').textContent = S.uv;
  nfWrite();
}

// Only the fields that apply to the current kind are sent -- a height filter
// left over from the models tab would otherwise silently empty the texture grid.
const NF_MODEL = ['hmin','hmax','col'];
const NF_TEX   = ['res','alpha'];
const nfKeys = () => S.kind==='model' ? NF_MODEL : NF_TEX;
function nfQuery(){
  return nfKeys().map(k => S.nf[k]!=='' && S.nf[k]!=null
    ? '&'+k+'='+encodeURIComponent(S.nf[k]) : '').join('');
}
function nfActive(){ return nfKeys().some(k => S.nf[k]!=='' && S.nf[k]!=null); }

let lastStar = null;

function nfBody(){
  const o = {};
  for(const k of nfKeys()) if(S.nf[k]!=='' && S.nf[k]!=null) o[k] = S.nf[k];
  return o;
}
function listPaths(){
  return S.mode==='favorites' ? [...S.star] : S.hist;
}
async function postFiltered(route, extra){
  const r = await fetch(route, {method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify(Object.assign({
      paths:listPaths(), kind:S.kind, q:S.q,
      role:S.role, theme:S.theme}, nfBody(), extra||{}))});
  return r.json();
}

const seen = new WeakSet();
const io = new IntersectionObserver(es=>{
  for(const e of es){
    if(!e.isIntersecting || seen.has(e.target)) continue;
    seen.add(e.target); fill(e.target);
  }
},{rootMargin:'300px'});
