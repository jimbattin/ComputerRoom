// `bun run dev`: Bun's built-in bundler serves index.html with hot reload.
// `bun run preview`: serves the `bun run build` output in dist/ as static files.
// Both bind the IPv4 loopback only and are opened as http://localhost:<port>, a secure context,
// which Web MIDI SysEx requires. Not hostname 'localhost': Bun then binds ::1 or 127.0.0.1,
// whichever is free, so a second server would start on the other address instead of failing.
// With one fixed address a port already in use makes Bun.serve() throw and the server exit.
import { join, sep } from 'node:path';
import index from './index.html';

const preview = process.argv.includes('--dist');
const dist = join(import.meta.dir, 'dist');
const notFound = () => new Response('Not found', { status: 404 });

const server = preview
  ? Bun.serve({
      hostname: '127.0.0.1',
      port: 4174,
      async fetch(req) {
        let path: string;
        try {
          path = decodeURIComponent(new URL(req.url).pathname);
        } catch {
          return notFound();
        }
        // join() normalizes `..`, including a decoded %2F; anything resolving outside dist/ is refused.
        const target = join(dist, path.endsWith('/') ? `${path}index.html` : path);
        if (!target.startsWith(dist + sep)) return notFound();
        const file = Bun.file(target);
        return (await file.exists()) ? new Response(file) : notFound();
      },
    })
  : Bun.serve({
      hostname: '127.0.0.1',
      port: 5174,
      routes: { '/': index },
      development: { hmr: true, console: true },
    });

console.log(`${preview ? 'Previewing dist/ at' : 'Editor dev server at'} http://localhost:${server.port}/`);
