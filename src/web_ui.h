#pragma once
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>K-line Bridge</title>
<style>
 body{font:14px system-ui;margin:0;background:#111;color:#eee}
 header{background:#1c1c1c;padding:8px 12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
 button{background:#2a6;color:#fff;border:0;padding:6px 10px;border-radius:4px}
 button.sec{background:#345}
 input{background:#222;color:#eee;border:1px solid #444;border-radius:4px;padding:4px}
 .tabs button{background:#333}.tabs button.on{background:#2a6}
 table{border-collapse:collapse;width:100%}td,th{border:1px solid #333;padding:3px 6px;text-align:left}
 .mono{font-family:ui-monospace,monospace}
 .hist{color:#8cf}
 section{padding:10px}
</style></head><body>
<header>
 <b>K-line Bridge</b>
 <span id="st">?</span>
 <input id="baud" value="10400" size="6">
 <button onclick="conn()">Ligar</button>
 <button class="sec" onclick="post('/disconnect').then(status)">Desligar</button>
 <button class="sec" onclick="sniff()">Sniff 3s</button>
</header>
<div class="tabs" style="padding:6px 10px">
 <button id="tScan" class="on" onclick="tab('scan')">Scanner</button>
 <button id="tWatch" onclick="tab('watch')">Painel</button>
</div>

<section id="scan">
 <div>
  Gama:
  <input id="s0" value="C000" size="5"> - <input id="s1" value="C800" size="5">
  <button onclick="baseline()">Retrato</button>
  <span class="hist" id="hist"></span>
 </div>
 <div style="margin:8px 0">
  Filtro:
  <button class="sec" onclick="filt('changed')">mudou</button>
  <button class="sec" onclick="filt('unchanged')">nao mudou</button>
  <button class="sec" onclick="filt('up')">subiu</button>
  <button class="sec" onclick="filt('down')">desceu</button>
  = <input id="fv" size="3"><button class="sec" onclick="filt('eq',fvv())">=</button>
  <button class="sec" onclick="filt('ne',fvv())">!=</button>
  <button onclick="undo()">&larr; recuar</button>
 </div>
 <table><thead><tr><th>addr</th><th>valor</th><th></th></tr></thead><tbody id="cand"></tbody></table>
</section>

<section id="watch" style="display:none">
 <table><thead><tr><th>nome</th><th>addr</th><th>largura</th><th>valor</th></tr></thead>
 <tbody id="wt"></tbody></table>
 <button onclick="pollWatch()">Atualizar</button>
</section>

<script>
let base=null, cur=null, stack=[], cands=null;
const WATCH=[["rpm","F96A",2],["embraiagem","C2A1",1],["SET","C2A8",1],
             ["RESET","C2AA",1],["teto antilag","E772",1],["modo","E786",1]];

async function j(u,m){const r=await fetch(u,{method:m||'GET'});return r.json();}
function post(u){return fetch(u,{method:'POST'});}
function fvv(){return parseInt(document.getElementById('fv').value||'0',10);}

async function status(){const d=await j('/status');
 document.getElementById('st').textContent =
  d.connected?('ligado KW '+d.kw1+'/'+d.kw2+' '+(d.part_number||'')):'desligado';}
async function conn(){const b=document.getElementById('baud').value;
 document.getElementById('st').textContent='a ligar...';
 await j('/connect?baud='+b,'POST');status();}

function tab(x){for(const t of ['scan','watch']){
 document.getElementById(t).style.display=(t===x)?'':'none';
 document.getElementById('t'+t[0].toUpperCase()+t.slice(1)).classList.toggle('on',t===x);}}

async function snap(){const s0=document.getElementById('s0').value,s1=document.getElementById('s1').value;
 const d=await j('/snapshot?start='+s0+'&end='+s1);
 const start=parseInt(s0,16);const m=new Map();
 d.values.forEach((v,i)=>m.set(start+i,v));return m;}

async function baseline(){document.getElementById('hist').textContent='a ler retrato...';
 base=await snap();cands=[...base.keys()];stack=[];
 render();updateHist([base.size]);}

async function filt(kind,val){if(!base){alert('faz Retrato primeiro');return;}
 cur=await snap();stack.push(cands.slice());
 cands=applyFilter(new Map(cands.map(a=>[a,base.get(a)])),
                   new Map(cands.map(a=>[a,cur.get(a)])),kind,val);
 base=cur;render();pushHist();}

function undo(){if(stack.length){cands=stack.pop();render();pushHist();}}

let histArr=[];
function updateHist(a){histArr=a;drawHist();}
function pushHist(){histArr.push(cands.length);drawHist();}
function drawHist(){document.getElementById('hist').textContent=histArr.join(' -> ');}

function render(){const tb=document.getElementById('cand');tb.innerHTML='';
 for(const a of cands.slice(0,400)){const tr=document.createElement('tr');
  const h=a.toString(16).toUpperCase().padStart(4,'0');
  tr.innerHTML='<td class=mono>'+h+'</td><td>'+(base.get(a))+
   '</td><td><button class=sec onclick="toWatch(\''+h+'\')">painel</button></td>';
  tb.appendChild(tr);}}

function toWatch(h){WATCH.push([h,h,1]);tab('watch');renderWatch();}
function renderWatch(){const tb=document.getElementById('wt');tb.innerHTML='';
 for(const [n,a,w] of WATCH){const tr=document.createElement('tr');
  tr.innerHTML='<td>'+n+'</td><td class=mono>'+a+'</td><td>'+(w===2?'u16':'u8')+
   '</td><td id="v_'+a+'">-</td>';tb.appendChild(tr);}}
async function pollWatch(){for(const [n,a,w] of WATCH){
  const d=await j('/read?addr='+a+'&len='+w);
  let v=d.bytes[0];if(w===2)v=d.bytes[0]|(d.bytes[1]<<8);
  const el=document.getElementById('v_'+a);if(el)el.textContent=v;}}

async function sniff(){const d=await j('/sniff?ms=3000');
 alert(d.bytes.map(b=>b.toString(16).padStart(2,'0')).join(' ')||'(nada)');}

// --- pure narrowing logic ---
function applyFilter(prev,cur,kind,val){const out=[];
 for(const [a,c] of cur){const p=prev.get(a);let keep=false;
  switch(kind){case 'changed':keep=p!==c;break;case 'unchanged':keep=p===c;break;
   case 'up':keep=c>p;break;case 'down':keep=c<p;break;
   case 'eq':keep=c===val;break;case 'ne':keep=c!==val;break;}
  if(keep)out.push(a);}return out;}
function selftest(){const prev=new Map([[1,10],[2,20],[3,30]]);
 const cur=new Map([[1,10],[2,25],[3,20]]);
 console.assert(JSON.stringify(applyFilter(prev,cur,'changed'))==='[2,3]','changed');
 console.assert(JSON.stringify(applyFilter(prev,cur,'unchanged'))==='[1]','unchanged');
 console.assert(JSON.stringify(applyFilter(prev,cur,'up'))==='[2]','up');
 console.assert(JSON.stringify(applyFilter(prev,cur,'down'))==='[3]','down');
 console.log('selftest done');}
if(location.hash==='#selftest')selftest();

renderWatch();status();
</script></body></html>)HTML";
