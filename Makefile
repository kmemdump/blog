.PHONY: nob build site serve clean

nob: nob.c nob.h
	cc -Wall -Wextra -o nob nob.c

build/blog-builder: nob src/main.c
	mkdir -p build
	./nob

site: build/blog-builder
	./build/blog-builder

serve: site
	node scripts/serve.js

clean:
	rm -rf nob build/blog-builder archive index.html

