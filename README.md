# Blog

A simple static blog for the essays in `essays/`. It is plain HTML, CSS, and a small Node.js build script. There are no runtime dependencies and nothing to install.

## How the site is built

The source essays live in `essays/*.md`. The HTML pages are generated from those markdown files by `build.js`:

1. `build.js` reads every `essays/*.md` file.
2. It converts each markdown file into a standalone HTML page at `essays/<slug>.html`.
3. It generates `index.html` with links to every generated essay page.
4. All styles live in `assets/style.css`; generated pages link to it.
5. Essay body text is HTML-escaped before being written into the pages.

To rebuild the site after editing an essay:

```sh
node build.js
```

No package manager is required. Only Node.js is used.

## Typical edit/build flow

```sh
git switch -c feature/my-change
# edit essays/<name>.md or build.js
node build.js
git add build.js index.html essays assets README.md
git commit -m "Update site"
git push -u origin feature/my-change
```

Then open a pull request and wait for human review/merge. Do not commit directly to `main`.

## Adding a new essay

1. Add a new markdown file such as `essays/my-new-essay.md`.
2. Use a lowercase, hyphenated filename; the filename becomes the page slug and the title is generated from it.
3. Run `node build.js`.
4. Commit both the markdown source and the generated HTML files.

## Repository layout

```text
essays/                 source .md essays and generated .html pages
assets/style.css        all CSS for the site
build.js                static site generator
index.html              generated homepage
README.md               this file
INSTRUCTIONS.md         repository rules for contributors/LLMs
```

## Deployment

Deploy via GitHub Pages from the `main` branch root after the pull request is merged. See the PR description or project notes for the exact Pages settings.
