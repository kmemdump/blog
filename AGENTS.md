## Progress (blog-v2)

- 2026-11-12: Initialized submodule `notes` from `git@github.com:kmemdump/notes`
  (source entries in `notes/writing/essays`).
- Implemented C site generator `src/main.c` (built via `nob.h`/`nob.c`)
  producing `index.html` and `/archive/{YYYY}/{MM}/{DD}/{title-slug}/index.html`
  for each essay markdown file.
- Entry pages show two ISO dates, both from the submodule's git history:
  Created (first, above) and Published. Created is the oldest commit that added
  the file, following renames (`git -C notes log --follow --diff-filter=A
  --format=%aI -- writing/essays/<file>`, last line) and sets the
  `{YYYY}/{MM}/{DD}` in the entry URL. Published is the file's last edit (`git
  -C notes log -1 --format=%aI -- writing/essays/<file>`). The index still shows
  and sorts by the published date.
- Tags: deterministic intersection of normalized tokens from markdown content
  and `assets/tags` (lowercased `[A-Za-z0-9]+` tokens, deduped and sorted).
- Entry pages render markdown with `zero-md` v3, loaded from the jsDelivr CDN
  (approved by the user): `<zero-md
  src="/archive/{YYYY}/{MM}/{DD}/{slug}/index.md">`. The generator writes each
  essay's markdown as `index.md` next to its `index.html`. The custom renderer
  `assets/entry.js` was removed. Page styling is in `assets/style.css` (80ch
  content width, responsive); zero-md renders in a shadow DOM with its own
  GitHub theme, so page CSS does not reach the article body.
- HTML is produced with the vendored `src/tinytemplate.{c,h}`
  (https://github.com/cozis/tinytemplate), compiled into `build/blog-builder` by
  `nob.c`. Page markup lives in `templates/index.html` and
  `templates/entry.html`; `src/main.c` only supplies values. tinytemplate quirks:
  it does not HTML-escape, so `main.c` escapes every value first; `{{name }}`
  with a space before `}}` fails to compile; `{% if x %}` is true only for
  non-zero numbers, never strings; it has no whitespace trimming, so loop
  markers sit inline with the markup.
- `Makefile` targets: `nob` (builds `build/nob`), `build/blog-builder`, `site`
  (wipes and regenerates `build/dist`), `serve` (`site` + `node
  scripts/serve.js`, serves `build/dist` on port 8080 by default, override with
  `PORT`), `clean` (removes `build/`).
- Site output goes to `build/dist` (`index.html`, `archive/`, and a copy of
  `assets/` without `tags`). `build/` is gitignored; generated files are not
  committed. The previously committed root `index.html` and `archive/` were
  deleted from the worktree.
- Open: `.github/workflows/static.yml` still uploads the repo root with no build
  step, so it no longer deploys a site until it builds and uploads `build/dist`.

