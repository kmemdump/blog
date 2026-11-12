.PHONY: nob build site serve clean

nob: nob.c nob.h
	mkdir -p build
	cc -Wall -Wextra -o build/nob nob.c

build/blog-builder: nob src/main.c
	./build/nob

site: build/blog-builder
	rm -rf build/dist
	./build/blog-builder

serve: site
	node scripts/serve.js

clean:
	rm -rf build
