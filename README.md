<p align="center">
  <a href="https://michaldziuba03.github.io/pand/"><img src="https://github.com/user-attachments/assets/40c000fa-26b2-425d-98d0-ad68d3026b0e" alt="Logo" height=170></a>
</p>

<h1 align="center">PandJS</h1>

<div align="center">
  <a href="https://github.com/pandland/pand/tree/main/docs">Documentation</a>
  <span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
  <a href="https://pandland.github.io/pand">Website</a>
  <span>&nbsp;&nbsp;•&nbsp;&nbsp;</span>
  <a href="https://github.com/pandland/pand/issues/new">Issues</a>
  <br />
  <br />
</div>

> ⚠️ Early stage of development.

My own JavaScript runtime - currently, just randomly messing around with v8 engine in C++. Very unstable as I learn how things work together.

### TODO:

- [x] Try to bind SWC (written in Rust) into my C++ codebase
- [x] Integrate my async IO library for runtime's event loop: [luxio](https://github.com/michaldziuba03/luxio) (renamed to `pandio`, runtime will use updated version soon).
- [x] Create timers
- [x] Clunky TCP support
- [x] Basic support for ES6 imports
- [ ] Improve TCP module
- [ ] Improve memory management and fix potential leaks
- [ ] File system module

> Current state example:

```js
import { 
  assert, 
  assertThrows,
  assertStrictEqual  
} from 'std:assert';
import { tcpListen } from 'std:net';

function willThrow() {
  throw new Error("Some error");
}

Runtime.argv.forEach(item => {
  console.log(`Arg: ${item}`);
});

assert(2 + 2 == 4);
assertThrows(willThrow);
assertStrictEqual(2, 2);

console.log(`Platform is ${Runtime.platform} and pid is: ${Runtime.pid}`);
console.log(`Cwd is: ${Runtime.cwd()} and runtime version is: ${Runtime.version}`);
console.log(`Dirname: ${import.meta.dirname}`)
console.log(`Filename: ${import.meta.filename}`);
console.log(`Url: ${import.meta.url}`);

tcpListen((socket) => {
  console.log("Client connected");

  setTimeout(() => {
    socket.close();
  }, 5 * 1000);

  socket.read((chunk) => {
    console.log(`Received data`);
    const response = `<h1>${chunk}</h1>`; // echo HTTP request
    socket.write(`HTTP/1.1 200 OK\r\nContent-Length: ${response.length}\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n`);
    socket.write(response);
  });
}, 8000);
```

## License

Distributed under the MIT License. See `LICENSE` for more information.
