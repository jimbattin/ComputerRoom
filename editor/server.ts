// Dev server: Bun's bundler serves src/editor.html with hot reload.
// It binds the IPv4 loopback only and is opened as http://localhost:<port>, a secure context,
// which Web MIDI SysEx requires. Not hostname "localhost": Bun then binds ::1 or 127.0.0.1,
// whichever is free, so a second server would start on the other address instead of failing.
// With one fixed address a port already in use makes Bun.serve() throw and the server exit.
import editor from "./src/editor.html";

const port = Number(process.env.PORT ?? 5174);
const server = Bun.serve({
  hostname: "127.0.0.1",
  port,
  routes: { "/": editor },
  development: { hmr: true, console: true },
});
console.log(`Editor dev server at http://localhost:${server.port}/`);
