// ui.js
// Every control handler, and the boot sequence. Loaded LAST.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

// ------------------------------------------------------------------- wiring

function setKind(k){
  S.kind=k; S.cat=''; S.page=0; S.role=''; S.theme='';
  $('kt').classList.toggle('on',k==='texture');
  $('km').classList.toggle('on',k==='model');
  chrome(); go();
}
function setMode(m){
  S.mode=m; S.page=0;
  for(const [id,v] of [['vb','browse'],['vf','favorites'],['vh','history']])
    $(id).classList.toggle('on', m===v);
  chrome(); go();
}
// Anything that cannot do something is disabled, not merely ignored on click.
// Paging past the last page used to "work" and walked to 3/1, 4/1, ...
function syncControls(){
  $('prev').disabled = S.mode!=='browse' || S.page<=0;
  $('next').disabled = S.mode!=='browse' || S.page>=(S.pages||1)-1;
  $('clr').disabled  = S.star.size===0;
  $('cmpbtn').disabled = !['texture','model'].some(
    k => [...S.star].filter(p=>kindOf(p)===k).length >= 2);
  $('exp').disabled  = S.star.size===0;
}

function chrome(){
  // Favorites and history mix kinds, so both controls stay live there.
  const mixed = S.mode!=='browse';
  // disabled on the CONTROLS, not just opacity on the wrapper: a faded select
  // still takes clicks and still opens.
  // Folders describe the archive, so they stay off for a list. Role, theme and
  // the numeric filters now run against the list itself and stay live.
  $('facets').toggleAttribute('disabled', false);
  $('nums').toggleAttribute('disabled', false);
  $('fscope').disabled = mixed;
  for(const id of ['frole','ftheme','fsort','xrole','xtheme'])
    $(id).disabled = false;
  nfWrite();
  for(const id of ['frole','ftheme','fscope','fsort','xrole','xtheme'])
    $(id).disabled = mixed;
  $('uvwrap').style.display   = (mixed||S.kind==='texture')?'':'none';
  $('projwrap').style.display = (mixed||S.kind==='model')?'':'none';
  $('sortwrap').style.display = (mixed||S.kind==='model')?'':'none';

}
$('kt').onclick=()=>setKind('texture');
$('km').onclick=()=>setKind('model');
$('vb').onclick=()=>setMode('browse');
$('vf').onclick=()=>setMode('favorites');
$('vh').onclick=()=>setMode('history');
let t, rz; $('q').oninput=e=>{ clearTimeout(t);
  t=setTimeout(()=>{ S.q=e.target.value; S.page=0; go(); },200); };
$('uv').oninput=e=>{ S.uv=parseFloat(e.target.value); $('uvv').textContent=S.uv;
  document.querySelectorAll('.card .view').forEach(v=>{
    if(v.tagName!=='CANVAS') sizeSwatch(v); });
  mtile(); };
function setPreset(k){ const p=PRESET[k]; if(!p) return;
  MVs.yaw=p[0]; MVs.pitch=p[1]; mDraw(); }
$('proj').onchange=e=>{ S.proj=e.target.value;
  redrawThumbs();                    // NOT paint(): that is the silhouette
  if(MVs.R) setPreset(S.proj);
  else if(mgeo) paint(mgeo,$('mview'),S.proj); };
$('sort').onchange=e=>{ S.sort=e.target.value; sortGrid(); };
function setBg(c){
  S.bg = c;
  localStorage.setItem('bg', c);
  $('bg').value = c;
  for(const b of document.querySelectorAll('.sw'))
    b.classList.toggle('on', b.dataset.c.toLowerCase() === c.toLowerCase());
  applyBg();
}
$('bg').oninput = e=>setBg(e.target.value);
for(const b of document.querySelectorAll('.sw')){
  b.style.background = b.dataset.c;
  b.onclick = ()=>setBg(b.dataset.c);
}
$('cols').onchange=e=>{ S.cols=e.target.value;
  localStorage.setItem('cols',S.cols); applyCols(); resized(); };
addEventListener('resize', ()=>{ clearTimeout(rz);
  rz=setTimeout(resized, 200); });
$('frole').onchange=e=>{ S.role=e.target.value; S.cat=''; S.page=0; go(); };
$('ftheme').onchange=e=>{ S.theme=e.target.value; S.cat=''; S.page=0; go(); };
$('xrole').onclick=()=>{ S.role=''; S.cat=''; S.page=0; go(); };
$('xtheme').onclick=()=>{ S.theme=''; S.cat=''; S.page=0; go(); };
$('fscope').onchange=e=>{ S.all=e.target.value==='all'; S.cat=''; S.page=0; go(); };
$('fsort').onchange=e=>{ S.fsort=e.target.value; go(); };
$('prev').onclick=()=>{ if(S.page>0){ S.page--; go(); } };
$('next').onclick=()=>{ if(S.page<(S.pages||1)-1){ S.page++; go(); } };
$('exp').onclick=()=>{ $('yaml').textContent=yaml(); $('out').classList.toggle('show'); };
const NF_IDS = {hmin:'fhmin',hmax:'fhmax',
                col:'fcol',res:'fres',alpha:'falpha'};
// Visibility only. Kept apart from nfWrite because typing must update the clear
// buttons WITHOUT writing values back into the field being typed in, which can
// move the caret.
function nfClears(){
  $('fclear').style.display  = nfActive() ? '' : 'none';
  $('fhclear').style.display = (S.nf.hmin || S.nf.hmax) ? '' : 'none';
}
let nfT;
function nfRead(now){
  for(const [k,id] of Object.entries(NF_IDS)) S.nf[k] = $(id).value;
  nfClears();
  S.page = 0;
  clearTimeout(nfT);
  if(now) go(); else nfT = setTimeout(go, 300);
}
function nfWrite(){
  for(const [k,id] of Object.entries(NF_IDS)) $(id).value = S.nf[k] ?? '';
  $('nmodel').style.display = S.kind==='model' ? '' : 'none';
  $('ntex').style.display   = S.kind==='model' ? 'none' : '';
  nfClears();
}
for(const id of Object.values(NF_IDS)){
  const el = $(id);
  // a number field fires change only on blur, so the clear button would not
  // appear until you clicked away; a select can commit immediately
  if(el.tagName === 'INPUT') el.oninput = ()=>nfRead(false);
  else el.onchange = ()=>nfRead(true);
}
$('fhclear').onclick = ()=>{
  S.nf.hmin = S.nf.hmax = '';
  nfWrite(); S.page=0; go();
};
$('fclear').onclick = ()=>{
  for(const k of Object.keys(S.nf)) S.nf[k]='';
  nfWrite(); S.page=0; go();
};
$('lsel').onchange = e=>setList(e.target.value);
$('lnew').onclick = ()=>{
  const n=(prompt('Name for the new list:')||'').trim();
  if(!n) return;
  if(n in S.lists) return setList(n);
  S.lists[n]=[]; setList(n);
};
$('lren').onclick = ()=>{
  const n=(prompt('Rename "'+S.list+'" to:', S.list)||'').trim();
  if(!n || n===S.list) return;
  if(n in S.lists && !confirm('"'+n+'" exists. Merge into it?')) return;
  const merged=[...new Set([...(S.lists[n]||[]), ...S.lists[S.list]])];
  delete S.lists[S.list];
  S.lists[n]=merged; setList(n);
};
$('ldel').onclick = ()=>{
  const names=Object.keys(S.lists);
  if(names.length<2) return alert('That is the only list.');
  if(!confirm('Delete "'+S.list+'" and its '+S.lists[S.list].length+' items?')) return;
  delete S.lists[S.list];
  setList(Object.keys(S.lists).sort()[0]);
};
$('starall').onclick = ()=>{
  for(const el of $('grid').querySelectorAll('.card:not([hidden])'))
    if(!S.star.has(el.dataset.path)) toggleStar(el.dataset.path);
};
$('clr').onclick=()=>{
  if(!S.star.size) return;
  if(!confirm('Remove all '+S.star.size+' items from "'+S.list+'"? '
              +'This cannot be undone.')) return;
  S.star.clear(); saveStar(); go(); };
function drawer(on){
  document.querySelector('aside').classList.toggle('open', on);
  $('scrim').classList.toggle('on', on);
}
$('vbtn').onclick = e=>{ e.stopPropagation(); $('vpop').classList.toggle('show'); };
addEventListener('click', e=>{
  if(!e.target.closest('#vpop') && !e.target.closest('#vbtn'))
    $('vpop').classList.remove('show');
});
$('menu').onclick = ()=>drawer(!document.querySelector('aside')
                                  .classList.contains('open'));
$('scrim').onclick = ()=>drawer(false);
$('clink').onclick = ()=>copyAnd(location.href, 'link');
addEventListener('hashchange', ()=>{
  const a = readHash();
  applyControls(); chrome();
  go().then(()=>{
    if(a) open_(a, null);
    else if($('modal').classList.contains('show')) mclose();
  });
});

$('cols').value = S.cols; applyCols();
setBg(S.bg);
if(!(S.list in S.lists)) S.list = Object.keys(S.lists).sort()[0];
S.star = new Set(S.lists[S.list]);
const wantAsset = readHash();
renderLists(); applyControls(); saveStar(); chrome();
go().then(()=>{ if(wantAsset) open_(wantAsset, null); });
