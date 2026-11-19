'use strict';
const fs=require('fs'),vm=require('vm'),crypto=require('crypto'),path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../firmware/components/wifi/index.html'),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];new vm.Script(script);
const sha=vm.runInNewContext(script.slice(script.indexOf('function sha256(buffer)'),script.indexOf("$('ota').onclick"))+';sha256');
for(const text of ['', 'abc', 'Hunter '.repeat(100000)]){const b=Buffer.from(text),actual=sha(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength)),expected=crypto.createHash('sha256').update(b).digest('hex');if(actual!==expected)throw Error('SHA256 known-vector mismatch');}
console.log('Phone JavaScript syntax and OTA SHA256 vectors: PASS');
