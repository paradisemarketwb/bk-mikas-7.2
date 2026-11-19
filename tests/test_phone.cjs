'use strict';
const assert=require('node:assert/strict'),http=require('http'),fs=require('fs'),path=require('path'),crypto=require('crypto');
let pw;try{pw=require('playwright')}catch{pw=require('../web/node_modules/playwright')}
const root=path.resolve(__dirname,'..');let profile=JSON.parse(fs.readFileSync(path.join(root,'build/tests/default-profile.json'),'utf8')),saved=0,uploaded=false;
const auth='Basic '+Buffer.from('hunter:simulation-only').toString('base64');
const server=http.createServer(async(req,res)=>{
 if(req.url==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');return res.end(fs.readFileSync(path.join(root,'firmware/components/wifi/index.html')))}
 if(req.headers.authorization!==auth){res.statusCode=401;return res.end('Authentication required')}
 if(req.method==='POST'&&req.headers['x-bk-intent']!=='configure'){res.statusCode=403;return res.end('Intent required')}
 const chunks=[];for await(const c of req)chunks.push(c);const body=Buffer.concat(chunks);res.setHeader('Content-Type','application/json');
 if(req.url==='/api/config'){if(req.method==='POST'){profile=JSON.parse(body);saved++;}return res.end(JSON.stringify(req.method==='POST'?{ok:true}:profile))}
 if(req.url==='/api/telemetry')return res.end(JSON.stringify({connected:true,clients:1,version:'SIMULATION',metrics:[{state:1,value:800},{state:1,value:88},{state:1,value:13.9}],faults_valid:true,faults:[{code:53,description:'СИМУЛЯЦИЯ: ДПКВ'}]}));
 if(req.url==='/api/history')return res.end(JSON.stringify({observed_engine_seconds:17,points:[[1000,800,88,13.9],[2000,null,null,null],[3000,900,89,13.8]],events:[]}));
 if(req.url==='/api/ota'){assert.equal(req.headers['x-bk-confirm'],'UPDATE');assert.equal(req.headers['x-bk-sha256'],crypto.createHash('sha256').update(body).digest('hex'));uploaded=true;return res.end('{"ok":true}')}
 res.statusCode=404;res.end('{}');
});
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const browser=await pw.chromium.launch({headless:true,executablePath:process.env.BK_CHROMIUM||undefined,args:['--no-sandbox']});
 try{const page=await browser.newPage({viewport:{width:390,height:844}});await page.goto('http://127.0.0.1:'+server.address().port);await page.fill('#password','simulation-only');await page.click('#enter');await page.waitForSelector('#app:not(.hidden)');await page.waitForFunction(()=>document.getElementById('metrics').children.length===3);
  fs.mkdirSync(path.join(root,'docs/images/web'),{recursive:true});await page.evaluate(()=>document.getElementById('message').textContent='СИМУЛЯЦИЯ: тестовые данные без подключения к автомобилю');await page.screenshot({path:path.join(root,'docs/images/web/phone-telemetry.png'),fullPage:true});
  await page.click('[data-page="editor"]');await page.fill('#screen-title','МОЙ ПРИБОР');await page.locator('#screen-title').press('Tab');await page.selectOption('#theme','1');await page.locator('#brightness').fill('62');await page.dispatchEvent('#brightness','input');await page.click('#add-widget');await page.click('#save');await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Профиль сохранён'));
  assert.equal(saved,1);assert.equal(profile.dark,true);assert.equal(profile.brightness,62);assert.equal(profile.layout.screens[0].title,'МОЙ ПРИБОР');assert.equal(profile.layout.screens[0].widgets.length,5);
  const download=page.waitForEvent('download');await page.click('#export');const d=await download;const exported=JSON.parse(fs.readFileSync(await d.path(),'utf8'));assert.equal(exported.brightness,62);
  await page.evaluate(()=>document.getElementById('message').textContent='СИМУЛЯЦИЯ: редактирование профиля прибора');await page.screenshot({path:path.join(root,'docs/images/web/phone-editor.png'),fullPage:true});
  await page.click('[data-page="update"]');await page.setInputFiles('#image',{name:'bk_mikas_r2.bin',mimeType:'application/octet-stream',buffer:Buffer.alloc(1024,0xe9)});await page.check('#confirm-update');await page.click('#ota');await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Образ принят'));assert(uploaded);
  console.log('Phone UI login, live telemetry, layout edit/save/export, confirmed OTA and SHA256: PASS');
 }finally{await browser.close();await new Promise(resolve=>server.close(resolve))}
})().catch(e=>{console.error(e);process.exitCode=1;server.close()});
