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
.github/workflows/static.yml  GitHub Actions workflow that deploys to Pages
CNAME                         custom domain for GitHub Pages
essays/                       source .md essays and generated .html pages
assets/style.css              all CSS for the site
build.js                      static site generator
index.html                    generated homepage
README.md                     this file
INSTRUCTIONS.md               repository rules for contributors/LLMs
```

## Deployment

The site deploys to GitHub Pages with the Actions workflow in `.github/workflows/static.yml`, not with a Jekyll build.

On every push to `main` (or when the workflow is run manually from the Actions tab), the workflow:

1. Checks out the repository.
2. Configures GitHub Pages.
3. Uploads the repository root as the Pages artifact.
4. Deploys that artifact to the `github-pages` environment.

Because the deploy uploads the repository as-is, the generated HTML must be committed before merging. Run `node build.js`, commit the markdown and generated files together, and let the workflow deploy after the pull request merges.

The `CNAME` file contains `blog.kmemdump.com`, so the site is served at:

```text
https://blog.kmemdump.com
```

The repository is not rebuilt on the Pages runner; it only publishes what is committed to `main`.
