// Bundler plugin for the dev server (bunfig.toml [serve.static]). Bun's dev-server bundler
// ignores `with { type: 'text' }` on a .toml import and inlines the parsed table instead, while
// the editor needs presets.toml byte-for-byte (the bank model and its FNV-1a hash work on the
// text). `bun build` and `bun test` honour the attribute and do not use this plugin.
import type { BunPlugin } from 'bun';

export default {
  name: 'toml-as-text',
  setup(build) {
    build.onLoad({ filter: /\.toml$/ }, async ({ path }) => ({
      contents: await Bun.file(path).text(),
      loader: 'text',
    }));
  },
} satisfies BunPlugin;
