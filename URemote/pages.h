const char page1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Universal Remote</title>
<style>
div,table{border-radius: 3px;box-shadow: 2px 2px 12px #000000;
background: rgb(94,94,94);
background: linear-gradient(0deg, rgba(94,94,94,1) 0%, rgba(160,160,160,1) 100%);
background-clip: padding-box;}
input{border-radius: 5px;box-shadow: 2px 2px 12px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(160,160,160,1) 0%, rgba(239,239,239,1) 100%);
background-clip: padding-box;}
body{width:470px;display:block;margin-left:auto;margin-right:auto;font-family: Arial, Helvetica, sans-serif;}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript">
a=document.all
idx=0
led=0
dys=[['Sun'],['Mon'],['Tue'],['Wed'],['Thu'],['Fri'],['Sat']]
protos=[
    ['UNKNOWN'],
    ['PULSE_WIDTH'],
    ['PULSE_DISTANCE'],
    ['APPLE'],
    ['DENON'],
    ['JVC'],
    ['LG'],
    ['LG2'],
    ['NEC'],
    ['NEC2'], /* NEC with full frame as repeat */
    ['ONKYO'],
    ['PANASONIC'],
    ['KASEIKYO'],
    ['KASEIKYO_DENON'],
    ['KASEIKYO_SHARP'],
    ['KASEIKYO_JVC'],
    ['KASEIKYO_MITSUBISHI'],
    ['RC5'],
    ['RC6'],
    ['SAMSUNG'],
    ['SAMSUNG48'],
    ['SAMSUNG_LG'],
    ['SHARP'],
    ['SONY'],
]
$(document).ready(function(){
  openSocket()
  for(i=0;i<protos.length;i++)
    document.getElementById("r"+i).innerHTML=protos[i]
  a.proto.innerHTML=protos[0]
})

function openSocket(){
 ws=new WebSocket("ws://"+window.location.host+"/ws")
// ws=new WebSocket("ws://192.168.31.81/ws")
 ws.onopen=function(evt){}
 ws.onclose=function(evt){alert("Connection closed.");}
 ws.onmessage=function(evt){
 console.log(evt.data)
  d=JSON.parse(evt.data)
  if(d.cmd=='state'){
   d2=new Date(d.t*1000)
   a.time.innerHTML=dys[d2.getDay()]+' '+d2.toLocaleTimeString()
  }
  else if(d.cmd=='setup')
  {
   a.TZ.value=d.tz
   a.SLEEP.value=d.st
  }
  else if(d.cmd=='print')
  {
    a.OUT.value += d.text + '\n'
    a.OUT.scrollTop = a.OUT.scrollHeight 
  }
  else if(d.cmd=='decode')
  {
    a.OUT.value += 'Protocol: '+protos[d.proto]+' Addr:'+d.addr.toString(16)+' Cmd:'+d.code.toString(16)+' Raw:'+d.raw.toString(16)+' Bits:'+d.bits+'\n'
    a.OUT.scrollTop = a.OUT.scrollHeight 
  }
  else if(d.cmd=='send')
  {
    a.OUT.value += 'Transmitted: '+protos[d.proto]+' Addr:'+d.addr.toString(16)+' Cmd:'+d.code.toString(16)+' Reps:'+d.rep+'\n'
    a.OUT.scrollTop = a.OUT.scrollHeight 
  }
  else if(d.cmd=='alert'){alert(d.text)}
 }
}

function setVar(varName, value)
{
 ws.send('{"'+varName+'":'+value+'}')
}

function setProto(b)
{
 a.proto.innerHTML=protos[b]
 a.OUT.value += 'Setting to ' + protos[b] + '\n'
 a.OUT.scrollTop = a.OUT.scrollHeight 
 setVar('PROTO', b)
}

function sendCode()
{
 addr=parseInt(a.ADDR.value,16)
 code=parseInt(a.CODE.value,16)
 ws.send('{"ADDR":'+addr+',"CODE":'+code+',"REP":'+a.REP.value+'}')
}

function setSleep(val)
{
 ws.send('{"ST":'+val+'}')
}

function setTZ(val)
{
 ws.send('{"TZ":'+val+'}')
}

function rxCode()
{
 ws.send('{"RX":0}')
}

function sendRst()
{
 ws.send('{"restart":0}')
}

</script>
<style type="text/css">
input{
 border-radius: 5px;
 margin-bottom: 5px;
 box-shadow: 2px 2px 12px #000000;
 background-clip: padding-box;
}
.range{
  overflow: hidden;
}
.dropdown {
  position: relative;
  display: inline-block;
}
.dropbtn {
  background-color: #50a0ff;
  padding: 1px;
  font-size: 12px;
  border-radius: 5px;
  margin-bottom: 5px;
  box-shadow: 2px 2px 12px #000000;
}
.btn {
  background-color: #50a0ff;
  padding: 1px;
  font-size: 12px;
  min-width: 50px;
  border: none;
}
.dropdown-content {
  display: none;
  position: absolute;
  background-color: #919191;
  min-width: 40px;
  min-height: 1px;
  z-index: 1;
}
.dropdown:hover .dropdown-content {display: block;}
.dropdown:hover .dropbtn {background-color: #3e8e41;}
.dropdown:hover .btn {  background-color: #3efe41;}
</style>
</head>
<body bgcolor="silver">
<table width=450>
<tr>
<td colspan=2><div id="time">Sun 1 10:00:00 AM</div></td>
<td></td>
<td><input type=button value="Restart" onClick="sendRst();"></td>
</tr>
<tr>
<td>
<div class="dropdown">
  <button class="dropbtn">Protocol</button>
  <div class="dropdown-content">
  <button class="btn" id="r0" onclick="setProto(0)"></button>
  <button class="btn" id="r1" onclick="setProto(1)"></button>
  <button class="btn" id="r2" onclick="setProto(2)"></button>
  <button class="btn" id="r3" onclick="setProto(3)"></button>
  <button class="btn" id="r4" onclick="setProto(4)"></button>
  <button class="btn" id="r5" onclick="setProto(5)"></button>
  <button class="btn" id="r6" onclick="setProto(6)"></button>
  <button class="btn" id="r7" onclick="setProto(7)"></button>
  <button class="btn" id="r8" onclick="setProto(8)"></button>
  <button class="btn" id="r9" onclick="setProto(9)"></button>
  <button class="btn" id="r10" onclick="setProto(10)"></button>
  <button class="btn" id="r11" onclick="setProto(11)"></button>
  <button class="btn" id="r12" onclick="setProto(12)"></button>
  <button class="btn" id="r13" onclick="setProto(13)"></button>
  <button class="btn" id="r14" onclick="setProto(14)"></button>
  <button class="btn" id="r15" onclick="setProto(15)"></button>
  <button class="btn" id="r16" onclick="setProto(16)"></button>
  <button class="btn" id="r17" onclick="setProto(17)"></button>
  <button class="btn" id="r18" onclick="setProto(18)"></button>
  <button class="btn" id="r19" onclick="setProto(19)"></button>
  <button class="btn" id="r20" onclick="setProto(20)"></button>
  <button class="btn" id="r21" onclick="setProto(21)"></button>
  <button class="btn" id="r22" onclick="setProto(22)"></button>
  <button class="btn" id="r23" onclick="setProto(23)"></button>
  </div>
</div>
</td>
<td><div id="proto"></div></td>
<td> Sleep:<input name="SLEEP" type=text value="1500" size="4" onChange="setSleep(this.value);"></td>
<td>
 TZ:<input name="TZ" type=text value="0" size="1" onChange="setTZ(this.value);">
</td>
</tr>
<tr>
<td><input name="RX" type=button value="Decode" onClick="rxCode();"></td>
<td>
Addr:<input name="ADDR" type=text size=1 value='0'>
 Code:<input name="CODE" type=text size=1 value='0'>
</td>
<td>
 Reps:<input name="REP" type=text size=1 value='0'>
</td>
<td><input name="SEND" type=button value="Send" onClick="sendCode();"></td>
</tr>
</table>
<table>
<tr>
<td>
<textarea readonly id="OUT" rows="20" cols="58">
</textarea>
</td>
</tr>
</table>
</body>
</html>
)rawliteral";

const uint8_t favicon[] PROGMEM = {
  0x1F, 0x8B, 0x08, 0x08, 0x70, 0xC9, 0xE2, 0x59, 0x04, 0x00, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 
  0x6E, 0x2E, 0x69, 0x63, 0x6F, 0x00, 0xD5, 0x94, 0x31, 0x4B, 0xC3, 0x50, 0x14, 0x85, 0x4F, 0x6B, 
  0xC0, 0x52, 0x0A, 0x86, 0x22, 0x9D, 0xA4, 0x74, 0xC8, 0xE0, 0x28, 0x46, 0xC4, 0x41, 0xB0, 0x53, 
  0x7F, 0x87, 0x64, 0x72, 0x14, 0x71, 0xD7, 0xB5, 0x38, 0x38, 0xF9, 0x03, 0xFC, 0x05, 0x1D, 0xB3, 
  0x0A, 0x9D, 0x9D, 0xA4, 0x74, 0x15, 0x44, 0xC4, 0x4D, 0x07, 0x07, 0x89, 0xFA, 0x3C, 0x97, 0x9C, 
  0xE8, 0x1B, 0xDA, 0x92, 0x16, 0x3A, 0xF4, 0x86, 0x8F, 0x77, 0x73, 0xEF, 0x39, 0xEF, 0xBD, 0xBC, 
  0x90, 0x00, 0x15, 0x5E, 0x61, 0x68, 0x63, 0x07, 0x27, 0x01, 0xD0, 0x02, 0xB0, 0x4D, 0x58, 0x62, 
  0x25, 0xAF, 0x5B, 0x74, 0x03, 0xAC, 0x54, 0xC4, 0x71, 0xDC, 0x35, 0xB0, 0x40, 0xD0, 0xD7, 0x24, 
  0x99, 0x68, 0x62, 0xFE, 0xA8, 0xD2, 0x77, 0x6B, 0x58, 0x8E, 0x92, 0x41, 0xFD, 0x21, 0x79, 0x22, 
  0x89, 0x7C, 0x55, 0xCB, 0xC9, 0xB3, 0xF5, 0x4A, 0xF8, 0xF7, 0xC9, 0x27, 0x71, 0xE4, 0x55, 0x38, 
  0xD5, 0x0E, 0x66, 0xF8, 0x22, 0x72, 0x43, 0xDA, 0x64, 0x8F, 0xA4, 0xE4, 0x43, 0xA4, 0xAA, 0xB5, 
  0xA5, 0x89, 0x26, 0xF8, 0x13, 0x6F, 0xCD, 0x63, 0x96, 0x6A, 0x5E, 0xBB, 0x66, 0x35, 0x6F, 0x2F, 
  0x89, 0xE7, 0xAB, 0x93, 0x1E, 0xD3, 0x80, 0x63, 0x9F, 0x7C, 0x9B, 0x46, 0xEB, 0xDE, 0x1B, 0xCA, 
  0x9D, 0x7A, 0x7D, 0x69, 0x7B, 0xF2, 0x9E, 0xAB, 0x37, 0x20, 0x21, 0xD9, 0xB5, 0x33, 0x2F, 0xD6, 
  0x2A, 0xF6, 0xA4, 0xDA, 0x8E, 0x34, 0x03, 0xAB, 0xCB, 0xBB, 0x45, 0x46, 0xBA, 0x7F, 0x21, 0xA7, 
  0x64, 0x53, 0x7B, 0x6B, 0x18, 0xCA, 0x5B, 0xE4, 0xCC, 0x9B, 0xF7, 0xC1, 0xBC, 0x85, 0x4E, 0xE7, 
  0x92, 0x15, 0xFB, 0xD4, 0x9C, 0xA9, 0x18, 0x79, 0xCF, 0x95, 0x49, 0xDB, 0x98, 0xF2, 0x0E, 0xAE, 
  0xC8, 0xF8, 0x4F, 0xFF, 0x3F, 0xDF, 0x58, 0xBD, 0x08, 0x25, 0x42, 0x67, 0xD3, 0x11, 0x75, 0x2C, 
  0x29, 0x9C, 0xCB, 0xF9, 0xB9, 0x00, 0xBE, 0x8E, 0xF2, 0xF1, 0xFD, 0x1A, 0x78, 0xDB, 0x00, 0xEE, 
  0xD6, 0x80, 0xE1, 0x90, 0xFF, 0x90, 0x40, 0x1F, 0x04, 0xBF, 0xC4, 0xCB, 0x0A, 0xF0, 0xB8, 0x6E, 
  0xDA, 0xDC, 0xF7, 0x0B, 0xE9, 0xA4, 0xB1, 0xC3, 0x7E, 0x04, 0x00, 0x00, 
};
