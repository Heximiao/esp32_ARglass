'use strict';
const $ = id => document.getElementById(id);
const canvas = $('canvas'), context = canvas.getContext('2d', {alpha: false, willReadFrequently: true});
const video = $('video');
let socket = null, connectionPromise = null, media = null, sending = false, running = false;
let generation = 0, sequence = 0, lastCapture = 0, lastAckAt = 0;
const pending = new Set();
let config = {width: 256, height: 172, format: 1, fps: 30, quality: 70, fill: false, showFps: true, accepting: false};
function message(text) { $('status').textContent = text; }
function applyState(state) {
  if (state.type === 'error') { message(state.message); return; }
  if (state.ack >= 0) { pending.delete(state.ack); lastAckAt = performance.now(); }
  Object.assign(config, state);
  if (canvas.width !== config.width || canvas.height !== config.height) { canvas.width=config.width; canvas.height=config.height; }
  $('fpsActual').textContent = Number(state.actualFps || 0).toFixed(1);
  $('dropped').textContent = state.dropped || 0;
  $('format').value = String(config.format); $('fps').value = String(config.fps);
  if (![50,70,90].includes(config.quality)) config.quality=70;
  $('quality').value=String(config.quality); $('quality').disabled=config.format===2;
  $('fit').value=config.fill?'1':'0'; $('showFps').value=config.showFps?'1':'0';
  if (running && !config.accepting) { stop(); message('眼镜已退出投屏，请在眼镜上重新选择“开始投屏”。'); }
}
function sendCommand(command) {
  if (socket?.readyState !== WebSocket.OPEN) { message('请先连接设备。'); return false; }
  socket.send(JSON.stringify(command)); return true;
}
function connect() {
  if (socket?.readyState === WebSocket.OPEN) return Promise.resolve();
  if (connectionPromise) return connectionPromise;
  const pin=$('pin').value.trim();
  if (!/^\d{6}$/.test(pin)) return Promise.reject(new Error('请输入眼镜上显示的六位配对码。'));
  connectionPromise=new Promise((resolve,reject)=>{
    const ws=new WebSocket(`${location.protocol==='https:'?'wss':'ws'}://${location.host}/ws?pin=${encodeURIComponent(pin)}`);
    socket=ws;
    const timeout=setTimeout(()=>{ws.close();reject(new Error('连接超时，请检查证书、网络和配对码。'));},10000);
    ws.onopen=()=>{ws.send(JSON.stringify({type:'state'}));};
    ws.onmessage=event=>{try{const state=JSON.parse(event.data);applyState(state);if(state.type==='state'||state.type==='ack'){clearTimeout(timeout);$('connection').textContent='已连接';resolve();}}catch(error){message(`设备响应异常：${error.message}`);}};
    ws.onerror=()=>{clearTimeout(timeout);reject(new Error('无法连接：请检查配对码、HTTPS 信任和是否已有其他发送端。'));};
    ws.onclose=()=>{clearTimeout(timeout);if(socket===ws){socket=null;stop(false);$('connection').textContent='已断开';message('连接已断开。请检查眼镜网络状态后重新连接。');} reject(new Error('连接已关闭。'));};
  }).finally(()=>{connectionPromise=null;});
  return connectionPromise;
}
function stop(notify=true) {
  generation++; running=false; sending=false; pending.clear();
  if (media) { media.getTracks().forEach(track=>track.stop());media=null; }
  video.srcObject=null; $('start').disabled=false; $('stop').disabled=true; $('saveWifi').disabled=false;
  if (notify && socket?.readyState===WebSocket.OPEN) sendCommand({type:'stop'});
}
function drawSource() {
  const w=canvas.width,h=canvas.height,sw=video.videoWidth,sh=video.videoHeight;
  if (!sw||!sh) return false;
  context.fillStyle='#000';context.fillRect(0,0,w,h);
  const scale=config.fill?Math.max(w/sw,h/sh):Math.min(w/sw,h/sh);
  const dw=sw*scale,dh=sh*scale;
  context.drawImage(video,(w-dw)/2,(h-dh)/2,dw,dh);return true;
}
function packet(payload, format, id) {
  const result=new Uint8Array(20+payload.byteLength),view=new DataView(result.buffer);
  result.set([65,82,83,49,format,0]);view.setUint16(6,canvas.width,true);view.setUint16(8,canvas.height,true);
  view.setUint16(10,20,true);view.setUint32(12,id,true);view.setUint32(16,payload.byteLength,true);
  result.set(new Uint8Array(payload),20);return result;
}
function rgb565(rgba) {
  const output=new Uint8Array(rgba.length/2);
  for(let i=0,j=0;i<rgba.length;i+=4,j+=2){const value=((rgba[i]&248)<<8)|((rgba[i+1]&252)<<3)|(rgba[i+2]>>3);output[j]=value&255;output[j+1]=value>>8;}
  return output.buffer;
}
async function pump(now) {
  if (!running) return;
  requestAnimationFrame(pump);
  if (pending.size && now-lastAckAt>5000) {stop();message('设备超过 5 秒未响应，已停止采集，避免积压。');return;}
  const interval=1000/(config.fps||50);
  if(sending || pending.size>=2 || socket?.readyState!==WebSocket.OPEN || socket.bufferedAmount>196608 || now-lastCapture<interval)return;
  if(!drawSource())return;
  sending=true;lastCapture=now;
  const version=generation,format=Number(config.format);
  try {
    let payload;
    if(format===1){const blob=await new Promise(resolve=>canvas.toBlob(resolve,'image/jpeg',config.quality/100));if(!blob)throw new Error('浏览器 JPEG 编码失败');payload=await blob.arrayBuffer();}
    else payload=rgb565(context.getImageData(0,0,canvas.width,canvas.height).data);
    if(version!==generation || !running || socket?.readyState!==WebSocket.OPEN)return;
    if(payload.byteLength>98304)throw new Error('画面数据过大，请降低 JPEG 质量');
    const id=sequence++ & 0x7fffffff;
    if(!pending.size)lastAckAt=performance.now();
    pending.add(id);socket.send(packet(payload,format,id));
  } catch(error){stop();message(error.message);}
  finally{if(version===generation)sending=false;}
}
$('connect').onclick=()=>connect().catch(error=>message(error.message));
$('start').onclick=async()=>{
  if(!window.isSecureContext || !navigator.mediaDevices?.getDisplayMedia){$('secureWarning').classList.remove('hidden');return;}
  if(!/^\d{6}$/.test($('pin').value.trim())){message('请输入眼镜上显示的六位配对码。');return;}
  $('start').disabled=true;
  try {
    // Capture must be requested inside the click activation, before network awaits.
    media=await navigator.mediaDevices.getDisplayMedia({video:{frameRate:{ideal:60,max:60}},audio:false});
    media.getVideoTracks()[0].addEventListener('ended',()=>{stop();message('已停止共享。');},{once:true});
    await connect();
    if(!media)throw new Error('共享已取消');
    if(!config.accepting)throw new Error('请先在眼镜“投屏”菜单选择“开始投屏”。');
    video.srcObject=media;await video.play();
    pending.clear();lastCapture=0;lastAckAt=performance.now();running=true;generation++;
    $('stop').disabled=false;$('saveWifi').disabled=true;message('正在投屏。格式和帧率可在此页面或眼镜上调整。');
    requestAnimationFrame(pump);
  }catch(error){stop();message(error.name==='NotAllowedError'?'已取消授权，或浏览器未允许屏幕捕获。':error.message);}
};
$('stop').onclick=()=>{stop();message('已停止共享。');};
for(const id of ['format','fps','quality','fit','showFps'])$(id).onchange=()=>sendCommand({type:'config',format:Number($('format').value),fps:Number($('fps').value),quality:Number($('quality').value),fill:$('fit').value==='1',showFps:$('showFps').value==='1'});
$('saveWifi').onclick=()=>{if(running)return;const password=$('password').value;if(sendCommand({type:'wifi',ssid:$('ssid').value,password})){ $('password').value='';message('连接请求已发送。等待约 20 秒；若连接断开，请根据眼镜上显示的地址重新打开页面。');}};
setInterval(()=>{if(socket?.readyState===WebSocket.OPEN&&!running)sendCommand({type:'state'});},1000);
window.addEventListener('pagehide',()=>{stop(false);socket?.close();});
if(!window.isSecureContext || !navigator.mediaDevices?.getDisplayMedia)$('secureWarning').classList.remove('hidden');
fetch('/api/state').then(response=>response.json()).then(applyState).catch(()=>message('设备暂时无响应，请稍后刷新。'));
