var WebSocket = require('faye-websocket');
console.log("next create client");
var ws        = new WebSocket.Client('ws://localhost:8999/mmm');
console.log("next open");

ws.on('open', function(event) {
  console.log('open');
  ws.send('Hello, world!');
});

ws.on('message', function(event) {
  console.log('message', event.data);
});

ws.on('close', function(event) {
  console.log('close', event.code, event.reason);
  ws = null;
});
