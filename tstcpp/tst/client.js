var WebSocket = require('faye-websocket');
console.log("next create client");

function openone(id){

var ws        = new WebSocket.Client('ws://localhost:7681/mmm'+id);
console.log("next open");

ws.on('open', function(event) {
  console.log('open');
  //ws.send('Hello, world!');
});

ws.on('message', function(event) {
  console.log('message', event.data);
});

ws.on('close', function(event) {
  console.log('close', event.code, event.reason);
  ws = null;
});

}

function open(cnt){
    for(var i=0;i<cnt;i++){
        console.log(".....opening"+i+".....");
        openone(i);
    }
}

open(1);
