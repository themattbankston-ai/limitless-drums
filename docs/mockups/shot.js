const { chromium } = require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:+(process.argv[4]||1680),height:+(process.argv[5]||1050)},deviceScaleFactor:2});
p.on('console',m=>{if(m.text().startsWith('LAYOUT'))console.log(m.text());});
await p.goto('file://'+process.argv[2]);await p.waitForFunction(()=>document.title==='done',null,{timeout:20000});
await p.screenshot({path:process.argv[3], fullPage:true});await b.close();})();
