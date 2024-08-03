const { sayHello } = require("./log.ts");

interface Socket {
  write: (data: string) => void;
  read: (cb: (chunk: string) => void) => void;
  close: () => void;
}

//println(`$HOME is: ${env('HOME')}`);
sayHello("From another module!!!!");

function getPathFromRequest(chunk: string) {
  const line = chunk.split('\r\n')[0];
  if (!line) {
    return "Invalid";
  }

  const path = line.split(' ')[1];
  if (!path) {
    return "Invalid";
  }

  return path;
}

const net = bind("tcp");

net.tcpListen((socket: Socket) => {
  println("Client connected");

  socket.read((chunk) => {
    const path = getPathFromRequest(chunk);
    println(`Received data`);
    const response = `<h1>Path: ${path}</h1>`;
    socket.write(`HTTP/1.1 200 OK\r\nContent-Length: ${response.length}\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n`);
    socket.write(response);
    socket.close();
  });
}, 8000);
