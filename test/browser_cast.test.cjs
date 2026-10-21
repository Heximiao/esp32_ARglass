// Run against a local HTTPS/WebSocket peer; no ESP32 or OS trust changes required.
// Requires Playwright, Chrome, and locally generated src/cast/private/TlsMaterial.h.
const assert=require('node:assert/strict');
const fs=require('node:fs');const path=require('node:path');const https=require('node:https');const crypto=require('node:crypto');
const assistantMode=process.env.CAST_ASSISTANT==='1';
const {spawn}=require('node:child_process');
const {chromium}=require(process.env.CAST_PLAYWRIGHT||'playwright');
const root=path.resolve(__dirname,'..');
const tls=fs.readFileSync(path.join(root,'src/cast/private/TlsMaterial.h'),'utf8');
const pem=name=>tls.match(new RegExp(`${name}\\[\\][\\s\\S]*?R"PEM\\(([\\s\\S]*?)\\)PEM"`))[1];
const html=fs.readFileSync(path.join(root,'web/cast/index.html'),'utf8')
  .replace('/*__APP__*/',fs.readFileSync(path.join(root,'web/cast/app.js'),'utf8'));
let state={type:'state',ack:-1,width:256,height:172,format:1,fps:30,quality:70,fill:false,showFps:true,actualFps:27.5,dropped:0,errors:0,accepting:true,streaming:false};
let connections=[],jpeg=0,rgb=0,total=0,controls=[],failures=[];
function send(socket,value){const payload=Buffer.from(JSON.stringify(value));let header;
  if(payload.length<126)header=Buffer.from([0x81,payload.length]);
  else{header=Buffer.alloc(4);header[0]=0x81;header[1]=126;header.writeUInt16BE(payload.length,2);}
  if(!socket.destroyed)socket.write(Buffer.concat([header,payload]));}
const transport=assistantMode?require('node:http'):https;
const server=transport.createServer(assistantMode?{}:{key:pem('CastPrivateKey'),cert:pem('CastCertificate')},(req,res)=>{
  res.setHeader('Connection','close');res.setHeader('Content-Type',req.url==='/api/state'?'application/json':'text/html; charset=utf-8');
  res.end(req.url==='/api/state'?JSON.stringify(state):html);
});
server.on('upgrade',(req,socket)=>{
  assert.equal(req.url,'/ws');
  const accept=crypto.createHash('sha1').update(req.headers['sec-websocket-key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
  connections.push(socket);let buffered=Buffer.alloc(0),fragments=[];
  socket.on('error',()=>{});
  socket.on('data',data=>{buffered=Buffer.concat([buffered,data]);
    try{while(buffered.length>=2){let length=buffered[1]&127,offset=2;const opcode=buffered[0]&15,masked=!!(buffered[1]&128);
      if(length===126){if(buffered.length<4)return;length=buffered.readUInt16BE(2);offset=4;}
      else if(length===127){if(buffered.length<10)return;length=Number(buffered.readBigUInt64BE(2));offset=10;}
      assert(length<200000);if(buffered.length<offset+(masked?4:0)+length)return;
      const final=!!(buffered[0]&0x80);
      const mask=buffered.subarray(offset,offset+4);if(masked)offset+=4;
      let payload=Buffer.from(buffered.subarray(offset,offset+length));buffered=buffered.subarray(offset+length);
      if(masked)for(let i=0;i<payload.length;i++)payload[i]^=mask[i%4];
      if(opcode===8){socket.end();return;}
      if(opcode===1){const command=JSON.parse(payload.toString());controls.push(command);
        if(command.type==='config')for(const key of ['format','fps','quality','fill','showFps'])if(key in command)state[key]=command[key];
        if(command.type==='stop')state.streaming=false;
        send(socket,state);
      }else if(opcode===2 || opcode===0){
        fragments.push(payload);if(!final)continue;payload=Buffer.concat(fragments);fragments=[];
        assert.equal(payload.subarray(0,4).toString(),'ARS1');assert.equal(payload[5],0);
        assert.equal(payload.readUInt16LE(6),256);assert.equal(payload.readUInt16LE(8),172);assert.equal(payload.readUInt16LE(10),20);
        assert.equal(payload.readUInt32LE(16),payload.length-20);
        if(payload[4]===1){jpeg++;assert.equal(payload[20],255);assert.equal(payload[21],216);}
        else{rgb++;assert.equal(payload[4],2);assert.equal(payload.length-20,256*172*2);assert.equal(payload.readUInt16LE(20),0xf800);}
        total++;state.streaming=true;const id=payload.readUInt32LE(12);
        setTimeout(()=>send(socket,{...state,type:'ack',ack:id}),35);
      }
    }}catch(error){failures.push(error.stack);socket.destroy();}
  });
});
let helper=null,helperUrl=null;
async function closeHelper(){
  if(!helper)return;
  const processToClose=helper;helper=null;
  try{
    const html=await (await fetch(helperUrl,{signal:AbortSignal.timeout(3000)})).text();
    const config=JSON.parse(html.match(/window.AR_ASSISTANT=(\{[^;]+\});/)[1]);
    const target=new URL(`/api/shutdown?session=${encodeURIComponent(config.session)}`,helperUrl);
    const response=await fetch(target,{method:'POST',headers:{Origin:target.origin},signal:AbortSignal.timeout(3000)});
    assert(response.ok);
    await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(new Error('Assistant did not exit')),5000);processToClose.once('exit',()=>{clearTimeout(timer);resolve();});if(processToClose.exitCode!==null){clearTimeout(timer);resolve();}});
  }catch(error){
    if(process.platform==='win32')require('node:child_process').spawnSync('taskkill',['/PID',String(processToClose.pid),'/T','/F'],{windowsHide:true,stdio:'ignore'});
    else processToClose.kill();
    throw error;
  }
}
(async()=>{
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  let pageUrl=`https://127.0.0.1:${server.address().port}/`;
  if(assistantMode){
    const directory=path.resolve(root,'../arglass_assistant');
    const executable=process.env.CAST_ASSISTANT_EXE||process.env.CAST_PYTHON||'python';
    const args=process.env.CAST_ASSISTANT_EXE?[]:[path.join(directory,'assistant.py')];
    helper=spawn(executable,[...args,'--no-browser','--device-port',String(server.address().port)],{cwd:directory,env:{...process.env,PYTHONUTF8:'1'},windowsHide:true,stdio:['ignore','pipe','inherit']});
    pageUrl=await new Promise((resolve,reject)=>{
      let output='';const timer=setTimeout(()=>reject(new Error('Assistant did not start')),30000);
      helper.stdout.on('data',chunk=>{output+=chunk;const match=output.match(/http:\/\/127\.0\.0\.1:\d+\//);if(match){clearTimeout(timer);resolve(match[0]);}});
      helper.once('error',error=>{clearTimeout(timer);reject(error);});
      helper.once('exit',code=>{clearTimeout(timer);reject(new Error(`Assistant exited ${code}`));});
    });
    helperUrl=pageUrl;
  }
  const browser=await chromium.launch({headless:true,executablePath:process.env.CAST_CHROME||'C:/Program Files/Google/Chrome/Application/chrome.exe'});
  try{
    // Certificate exceptions apply only to this isolated test context, never the OS.
    const context=await browser.newContext({ignoreHTTPSErrors:true,viewport:{width:1200,height:1000}});
    const page=await context.newPage();let pageErrors=[];page.on('pageerror',error=>pageErrors.push(error.message));
    await page.addInitScript(()=>{
      window.__captureRequests=[];window.__captureTracks=[];
      navigator.mediaDevices.getDisplayMedia=async options=>{
        window.__captureRequests.push(options);
        const source=document.createElement('canvas');source.width=256;source.height=172;
        const draw=()=>{const ctx=source.getContext('2d');ctx.fillStyle='#ff0000';ctx.fillRect(0,0,256,172);};draw();
        const stream=source.captureStream(60);const interval=setInterval(draw,16);
        const track=stream.getVideoTracks()[0],stop=track.stop.bind(track);
        track.stop=()=>{clearInterval(interval);stop();};window.__captureTracks.push(track);return stream;
      };
    });
    await page.goto(pageUrl);
    if(assistantMode)await page.locator('#deviceIp').fill('127.0.0.1');
    assert.equal(await page.locator('#pin').count(),0);
    // Direct Start (without a prior connect) tests transient activation ordering and state handshake.
    await page.locator('#start').click();
    await page.waitForFunction(()=>document.getElementById('stop').disabled===false);
    await new Promise(resolve=>setTimeout(resolve,650));assert(jpeg>=3,`JPEG frames: ${jpeg}`);
    assert.equal(await page.evaluate(()=>window.__captureRequests[0].audio),false);
    assert((await page.evaluate(()=>pending.size))<=2);
    await page.locator('#format').selectOption('2');
    await new Promise(resolve=>setTimeout(resolve,650));assert(rgb>=3,`RGB frames: ${rgb}`);
    await page.locator('#fps').selectOption('5');await new Promise(resolve=>setTimeout(resolve,200));
    const before=total;await new Promise(resolve=>setTimeout(resolve,1200));assert(total-before<=8,`5 FPS cap: ${total-before}`);
    await page.locator('#stop').click();
    assert.equal(await page.evaluate(()=>window.__captureTracks.every(track=>track.readyState==='ended')),true);
    await new Promise(resolve=>setTimeout(resolve,150));const stopped=total;
    await new Promise(resolve=>setTimeout(resolve,300));assert.equal(total,stopped);
    await page.locator('#start').click();await new Promise(resolve=>setTimeout(resolve,400));
    state.accepting=false;for(const socket of connections)send(socket,state);
    await page.waitForFunction(()=>document.getElementById('stop').disabled);
    assert.equal(await page.evaluate(()=>window.__captureTracks.every(track=>track.readyState==='ended')),true);
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
    const out=path.join(root,'.pio/browser-tests');fs.mkdirSync(out,{recursive:true});
    await page.screenshot({path:path.join(out,'desktop.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
    await page.screenshot({path:path.join(out,'narrow.png'),fullPage:true});
    assert.deepEqual(pageErrors,[]);assert.deepEqual(failures,[]);
    console.log(`PASS ${assistantMode?'assistant':'device page'}: direct-start handshake, JPEG ${jpeg}, RGB565 ${rgb}, byte order, 5 FPS pacing, two-frame backpressure, stop/restart, device exit, audio=false, responsive layout.`);
  }finally{await browser.close();await closeHelper();for(const socket of connections)socket.destroy();await new Promise(resolve=>server.close(resolve));}
})().catch(async error=>{try{await closeHelper();}catch(cleanupError){console.error(cleanupError.message);}console.error(error.stack);if(failures.length)console.error(failures.join('\n'));process.exitCode=1;for(const socket of connections)socket.destroy();server.close();});
