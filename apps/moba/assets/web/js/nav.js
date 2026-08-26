// nav.js
// Sidebar data: the folder drill-down and the role/theme facets.
//
// Plain script, not an ES module: all files share one global scope and
// ui.js runs last. The load order is fixed in index.html.

async function tree(){
  $('tree').className = S.mode==='browse' ? '' : 'off';
  const f = await (await fetch('/api/folders?kind='+S.kind+'&all='+(S.all?1:0)
    + '&at='+encodeURIComponent(S.cat)
    + '&role='+encodeURIComponent(S.role)
    + '&theme='+encodeURIComponent(S.theme) + nfQuery())).json();
  const box = $('tree');
  box.innerHTML = '';
  const goTo = at => { S.cat = at; S.page = 0; drawer(false); go(); };

  const crumb = document.createElement('div');
  crumb.className = 'crumb';
  const segs = S.cat ? S.cat.split('\\') : [];
  const hop = (label, at, last) => {
    const el = document.createElement(last ? 'b' : 'a');
    el.textContent = label;                     // never innerHTML: these are
    if(!last) el.onclick = ()=>goTo(at);        // archive names, not markup
    crumb.append(el);
    if(!last) crumb.append(document.createTextNode(' \u203A '));
  };
  hop('all', '', !segs.length);
  segs.forEach((sg,i)=>hop(sg, segs.slice(0,i+1).join('\\'), i===segs.length-1));
  box.append(crumb);

  const sum = document.createElement('div');
  sum.className = 'sum';
  sum.textContent = f.total.toLocaleString() + ' in this folder'
    + (f.here ? ' \u00B7 ' + f.here + ' directly here' : '');
  box.append(sum);

  if(!f.children.length){
    const e = document.createElement('div');
    e.className = 'none'; e.textContent = 'no subfolders';
    box.append(e);
    return;
  }
  for(const [name, n, hasKids] of f.children){
    const e = document.createElement('div');
    e.className = 'f';
    const nm = document.createElement('span'); nm.textContent = name;
    const ct = document.createElement('i');
    ct.textContent = n.toLocaleString() + (hasKids ? ' \u203A' : '');
    e.append(nm, ct);
    e.title = name + ' \u2014 ' + n + ' assets';
    e.onclick = ()=>goTo(S.cat ? S.cat + '\\' + name : name);
    box.append(e);
  }
}

async function facets(){
  const f = S.mode==='browse'
    ? await (await fetch('/api/facets?kind='+S.kind+'&all='+(S.all?1:0)
        + '&role='+encodeURIComponent(S.role)
        + '&theme='+encodeURIComponent(S.theme) + nfQuery())).json()
    : await postFiltered('/api/facets');
  if(!f || f.error){ toast('filter failed: '+((f&&f.error)||'no response')); return; }
  for(const [id,key,label] of [['frole','roles','role'],['ftheme','themes','theme']]){
    const sel = $(id), keep = S[label];
    // The server returns them by count; A-Z is a re-sort of the same list.
    const src = f[key] || [];
    const list = S.fsort==='name'
      ? [...src].sort((a,b)=>a[0].localeCompare(b[0])) : src;
    sel.innerHTML = '<option value="">any '+label+'</option>'
      + list.map(([k,n])=>'<option value="'+k+'">'+k+' ('+n+')</option>').join('');
    // A facet can vanish when the kind or the tree switch changes: keep the
    // selection only if it still exists, or the UI would claim a filter the
    // query is not applying.
    sel.value = src.some(([k])=>k===keep) ? keep : '';
    S[label] = sel.value;
  }
}

// navigator.clipboard is undefined over plain http, which is exactly how this
// is reached from another machine -- only localhost counts as a secure context.
// The execCommand path is the deprecated one, and the only one that works there.
