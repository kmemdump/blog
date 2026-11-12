## INSTRUCTIONS FOR LLMS

- Never change the INSTRUCTIONS.md file or its contents
- Never commit directly to the main branch of the repository
- Never write files in the default folder, create a worktree instead
- Allowed languages; javascript, html, css
- Do not install package managers without permission from a human
- Do not install external packages or dependencies to the application without
permission from a human
- Always create a pull request for changes and await a human to merge said 
changes
- Place all style class definitions into a separate css file and link the
respective html to it
- Place all javascript function definition into separate javascript files and 
link the respective html to it

## Progress (blog-v2)

- 2026-11-12: Initialized submodule `notes` from `git@github.com:kmemdump/notes` (source entries in `notes/writing/essays`).
- Implemented C site generator `src/main.c` (built via `nob.h`/`nob.c`) producing `index.html` and `/archive/{YYYY}/{MM}/{DD}/{title-slug}/index.html` for each essay markdown file.
- Published date/URL date derived from each file's last git edit in the submodule (`git -C notes log -1 --format=%aI -- writing/essays/<file>`), shown in ISO format on entry pages.
- Tags: deterministic intersection of normalized tokens from markdown content and `assets/tags` (lowercased `[A-Za-z0-9]+` tokens, deduped and sorted).
- Entry pages embed markdown and render client-side with `assets/entry.js`; styling in `assets/style.css` with 80ch content width and responsive layout.
- `Makefile` targets: `nob` (builds `build/nob`), `build/blog-builder`, `site` (wipes and regenerates `build/dist`), `serve` (`site` + `node scripts/serve.js`, serves `build/dist` on port 8080 by default, override with `PORT`), `clean` (removes `build/`).
- Site output goes to `build/dist` (`index.html`, `archive/`, and a copy of `assets/` without `tags`). `build/` is gitignored; generated files are not committed. The previously committed root `index.html` and `archive/` were deleted from the worktree.
- Open: `.github/workflows/static.yml` still uploads the repo root with no build step, so it no longer deploys a site until it builds and uploads `build/dist`.

