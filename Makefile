.PHONY: nob build site serve clean deploy

nob: nob.c nob.h
	mkdir -p build && \
	cc -Wall -Wextra -o build/nob nob.c

build : nob src/main.c
	./build/nob

site: build
	rm -rf build/dist && \
	set -a && \
	if [ -f .env ]; then . ./.env; fi && \
	set +a && \
	./build/blog-builder

serve: site
	wrangler pages dev build/dist

deploy: site
	wrangler pages deploy build/dist --project-name blog

clean:
	rm -rf build
