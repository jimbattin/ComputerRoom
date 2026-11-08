BUN ?= bun
PORT ?= 5174
STAMP = node_modules/.installed

$(STAMP): package.json bun.lock
	$(BUN) install --frozen-lockfile
	touch $@

serve: $(STAMP)
	PORT=$(PORT) $(BUN) run server.ts

html: dist/editor.html

# presets.toml is inlined (src/io/files.ts imports it as text).
dist/editor.html: $(STAMP) build.ts $(shell find src -type f) ../presets.toml
	$(BUN) run build.ts

test: $(STAMP)
	$(BUN) test

check: $(STAMP)
	$(BUN) x tsc --noEmit
	$(BUN) x biome check .

fmt: $(STAMP)
	$(BUN) x biome check --write .

clean:
	rm -rf dist

.PHONY: serve html test check fmt clean
