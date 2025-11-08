/*
   ESP32 Chat Pro – 3-User Stable Edition
   --------------------------------------
   • Max 3 simultaneous users
   • Auto-reconnects if Wi-Fi or socket drops
   • Keep-alive pings every 15 s
   • Username + room password
   • Stores last 30 messages in memory
   • LED heartbeat (GPIO 2)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

WebServer server(80);
WebSocketsServer webSocket(81);

// ===== CONFIG =====
const char* ssid         = "ESP32_ChatPro";
const char* apPass       = "";          // Wi-Fi password (blank = open)
const char* chatPassword = "1234";      // Room password ("" = none)
const int   MAX_USERS    = 3;
const int   MAX_HISTORY  = 30;
// ===================

struct Message { String text; };
Message history[MAX_HISTORY];
int historyCount = 0;
int connectedClients = 0;

String escapeHTML(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  return s;
}
void addToHistory(const String &msg) {
  if (historyCount < MAX_HISTORY) history[historyCount++].text = msg;
  else {
    for (int i = 1; i < MAX_HISTORY; i++) history[i - 1] = history[i];
    history[MAX_HISTORY - 1].text = msg;
  }
}
String buildHistoryHTML() {
  String h;
  for (int i = 0; i < historyCount; i++)
    h += "<div class='msg'>" + history[i].text + "</div>";
  return h;
}

// ===== HTML PAGE =====
String pageHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Chat Pro</title>
<style>
body{font-family:sans-serif;background:#f4f4f9;margin:0;padding:0}
#container{max-width:700px;margin:20px auto;background:white;padding:10px;border-radius:10px;box-shadow:0 0 5px rgba(0,0,0,.1)}
#chat{border:1px solid #ccc;height:60vh;overflow-y:auto;padding:8px;border-radius:5px;background:#fff;}
input,button{padding:10px;font-size:16px;}
#loginBox{text-align:center;padding:50px;}
.msg{margin:4px 0;padding:5px;background:#e9e9ff;border-radius:5px;}
.sys{color:#777;font-size:0.9em;}
.me{background:#d1ffd1;}
</style>
</head>
<body>
<div id='container'>
<h2>ESP32 Chat Pro</h2>
<div id='loginBox'>
  <input id='user' placeholder='Enter username'><br><br>
  <input id='pass' placeholder='Room password (if any)' type='password'><br><br>
  <button onclick='enterChat()'>Enter Chat</button>
</div>
<div id='chatBox' style='display:none'>
  <div id='chat'></div><br>
  <input id='msg' placeholder='Type message...' onkeydown="if(event.key==='Enter')sendMsg();">
  <button onclick='sendMsg()'>Send</button>
</div>
</div>

<script>
var ws; var username="";
function enterChat(){
  username=document.getElementById('user').value.trim();
  var p=document.getElementById('pass').value.trim();
  if(username===""){alert('Enter a username');return;}
  fetch('/auth?user='+encodeURIComponent(username)+'&pass='+encodeURIComponent(p))
  .then(r=>r.text()).then(t=>{
    if(t!=='OK'){alert(t);}else{startChat();}
  });
}
function startChat(){
  document.getElementById('loginBox').style.display='none';
  document.getElementById('chatBox').style.display='block';
  connectSocket();
}
function connectSocket(){
  ws=new WebSocket("ws://"+window.location.host+":81/");
  ws.onopen=function(){console.log("WebSocket connected");};
  ws.onmessage=function(e){
    if(e.data==="*PING") return;
    var chat=document.getElementById('chat');
    var div=document.createElement('div');
    div.innerHTML=e.data;
    if(e.data.startsWith('*'))div.className='sys';
    else if(e.data.startsWith(username+':'))div.className='me msg';
    else div.className='msg';
    chat.appendChild(div);
    chat.scrollTop=chat.scrollHeight;
  };
  ws.onclose=function(){
    console.log("Socket closed — reconnecting...");
    setTimeout(connectSocket,2000);
  };
  ws.onerror=function(){ws.close();};
}
function sendMsg(){
  var i=document.getElementById('msg');
  if(i.value.trim()!==""){ws.send(username+': '+i.value);i.value="";}
}
</script>
</body>
</html>
)rawliteral";
// ====================

// ===== HTTP handlers =====
void handleRoot(){ server.send(200,"text/html",pageHTML); }
void handleAuth(){
  String pass=server.arg("pass");
  if(String(chatPassword).length() && pass!=chatPassword)
       server.send(200,"text/plain","Wrong password");
  else server.send(200,"text/plain","OK");
}

// ===== WebSocket logic =====
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  switch(type){
    case WStype_CONNECTED:{
      IPAddress ip=webSocket.remoteIP(num);
      if(connectedClients>=MAX_USERS){
        Serial.printf("[DENIED] %s — room full\n",ip.toString().c_str());
        webSocket.sendTXT(num,"* Room full (max 3 users)");
        delay(500);
        webSocket.disconnect(num);
        return;
      }
      connectedClients++;
      Serial.printf("[%u] Connected from %s (Total:%d)\n",num,ip.toString().c_str(),connectedClients);
      webSocket.sendTXT(num,"<div class='sys'>Welcome to ESP32 Chat Pro!</div>"+buildHistoryHTML());
      webSocket.broadcastTXT("* A user joined the chat");
      addToHistory("<div class='sys'>A user joined</div>");
      break;
    }
    case WStype_DISCONNECTED:{
      connectedClients = max(0, connectedClients - 1);
      Serial.printf("[%u] Disconnected (Total:%d)\n",num,connectedClients);
      webSocket.broadcastTXT("* A user left the chat");
      addToHistory("<div class='sys'>A user left</div>");
      break;
    }
    case WStype_TEXT:{
      String msg=String((char*)payload);
      msg=escapeHTML(msg);
      Serial.println(msg);
      webSocket.broadcastTXT(msg);
      addToHistory("<div class='msg'>"+msg+"</div>");
      break;
    }
  }
}

void setup(){
  Serial.begin(115200);
  pinMode(2,OUTPUT);
  digitalWrite(2,HIGH);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, apPass);
  Serial.println("ESP32 Chat Pro (3-User) running at:");
  Serial.println(WiFi.softAPIP());

  server.on("/",handleRoot);
  server.on("/auth",handleAuth);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

void loop(){
  server.handleClient();
  webSocket.loop();

  static unsigned long lastPing=0;
  if(millis()-lastPing>15000){
    webSocket.broadcastTXT("*PING");
    lastPing=millis();
  }

  // heartbeat blink
  static unsigned long t=0;
  if(millis()-t>1000){
    t=millis();
    digitalWrite(2,!digitalRead(2));
  }
}
