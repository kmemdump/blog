const fs = require('fs');
const path = require('path');

const root = __dirname;
const essaysDir = path.join(root, 'essays');
const assetsDir = path.join(root, 'assets');

const siteTitle = "Krishen's Blog";
const siteTagline = 'Essays on software, work, and craft.';

function escapeHtml(value) {
  return value
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function titleFromSlug(slug) {
  return slug
    .split('-')
    .filter(Boolean)
    .map((word) => word.charAt(0).toUpperCase() + word.slice(1))
    .join(' ');
}

function paragraphsFromMarkdown(markdown) {
  return markdown
    .trim()
    .split(/\n\s*\n/)
    .map((block) => block.replace(/\s*\n\s*/g, ' ').trim())
    .filter(Boolean)
    .map((block) => `        <p>${escapeHtml(block)}</p>`)
    .join('\n');
}

function pageShell({ title, description, stylesheetPath, homePath, body }) {
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="${escapeHtml(description)}">
  <title>${escapeHtml(title)} | ${escapeHtml(siteTitle)}</title>
  <link rel="stylesheet" href="${stylesheetPath}">
</head>
<body>
  <header class="site-header">
    <div class="container header-inner">
      <a class="site-title" href="${homePath}">${escapeHtml(siteTitle)}</a>
      <nav class="site-nav" aria-label="Primary navigation">
        <a class="nav-link" href="${homePath}">Home</a>
      </nav>
    </div>
  </header>
  <main class="container">
${body}
  </main>
  <footer class="site-footer">
    <div class="container">
      <a class="footer-link" href="${homePath}">Back to home</a>
    </div>
  </footer>
</body>
</html>
`;
}

function buildEssay(fileName) {
  const slug = fileName.replace(/\.md$/, '');
  const title = titleFromSlug(slug);
  const markdown = fs.readFileSync(path.join(essaysDir, fileName), 'utf8');
  const description = `${title} — an essay from ${siteTitle}.`;
  const body = `    <article class="essay">
      <header class="essay-header">
        <p class="eyebrow">Essay</p>
        <h1>${escapeHtml(title)}</h1>
      </header>
${paragraphsFromMarkdown(markdown)}
    </article>`;

  return {
    slug,
    title,
    html: pageShell({
      title,
      description,
      stylesheetPath: '../assets/style.css',
      homePath: '../index.html',
      body,
    }),
  };
}

function buildIndex(essays) {
  const items = essays
    .map((essay) => `        <li class="essay-item">
          <a class="essay-link" href="essays/${essay.slug}.html">
            <span class="essay-link-title">${escapeHtml(essay.title)}</span>
            <span class="essay-link-note">Read essay</span>
          </a>
        </li>`)
    .join('\n');

  const body = `    <section class="intro">
      <p class="eyebrow">Writing</p>
      <h1>${escapeHtml(siteTitle)}</h1>
      <p class="tagline">${escapeHtml(siteTagline)}</p>
    </section>
    <section aria-labelledby="essays-heading">
      <h2 id="essays-heading" class="section-heading">Essays</h2>
      <ul class="essay-list">
${items}
      </ul>
    </section>`;

  return pageShell({
    title: 'Home',
    description: siteTagline,
    stylesheetPath: 'assets/style.css',
    homePath: 'index.html',
    body,
  });
}

fs.mkdirSync(assetsDir, { recursive: true });

const essayFiles = fs.readdirSync(essaysDir)
  .filter((fileName) => fileName.endsWith('.md'))
  .sort((a, b) => a.localeCompare(b));

const essays = essayFiles.map(buildEssay);

for (const essay of essays) {
  fs.writeFileSync(path.join(essaysDir, `${essay.slug}.html`), essay.html);
}

fs.writeFileSync(path.join(root, 'index.html'), buildIndex(essays));

console.log(`Built ${essays.length} essay pages and index.html`);
