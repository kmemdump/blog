const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const root = __dirname;
const essaysDir = path.join(root, 'essays');
const essaysSrcDir = path.join(root, 'essays-src');
const assetsDir = path.join(root, 'assets');

const siteTitle = "Krishen Mohan's Blog";
const siteTagline = 'My brain dump. Satirical if inflammatory. Enjoy';

// CNAME is the single source of truth for the deployment domain (https://blog.kmemdump.com).
const cnameDomain = fs.readFileSync(path.join(root, 'CNAME'), 'utf8').trim();
const siteUrl = `https://${cnameDomain}`;

function escapeHtml(value) {
    return value
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function formatDate(timestamp) {
    return toDateTime(timestamp);
}

function toDateTime(timestamp) {
    return new Date(timestamp).toISOString().slice(0, 10);
}

function formatGmtDateTime(timestamp) {
    // 2026-11-03 03:38:04 GMT
    return new Date(timestamp).toISOString().slice(0, 19).replace('T', ' ') + ' GMT';
}

function toFullDateTime(timestamp) {
    return new Date(timestamp).toISOString();
}

function titleFromSlug(slug) {
    return slug
        .split('-')
        .filter(Boolean)
        .map((word) => word.charAt(0).toUpperCase() + word.slice(1))
        .join(' ');
}

function getGitCreatedAt(fileName) {
    // essays-src is a symlink to the notes repo, so git history lives there, not in the blog repo.
    const srcDir = fs.realpathSync(essaysSrcDir);
    try {
        const output = execFileSync(
            'git',
            ['log', '--diff-filter=A', '--reverse', '--format=%at', '--', fileName],
            { cwd: srcDir, encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }
        );
        const firstLine = output.split('\n').find((line) => line.trim());
        const seconds = firstLine ? Number(firstLine.trim()) : NaN;

        return Number.isFinite(seconds) ? seconds * 1000 : null;
    } catch {
        return null;
    }
}

function getFileCreatedAt(filePath) {
    const stats = fs.statSync(filePath);

    return stats.birthtimeMs > 0 ? stats.birthtimeMs : stats.mtimeMs;
}

function essayCreatedAt(fileName) {
    return getGitCreatedAt(fileName) ?? getFileCreatedAt(path.join(essaysSrcDir, fileName));
}

function isUnorderedListItem(line) {
    return /^\s*[-*+]\s+/.test(line);
}

function isOrderedListItem(line) {
    return /^\s*\d+[.)]\s+/.test(line);
}

function isListItem(line) {
    return isUnorderedListItem(line) || isOrderedListItem(line);
}

function listItemText(line) {
    return line.replace(/^\s*(?:[-*+]|\d+[.)])\s+/, '').trim();
}

function markdownToHtml(markdown) {
    const blocks = [];
    let paragraphLines = [];
    let listItems = null;
    let listType = null;

    function flushParagraph() {
        if (paragraphLines.length === 0) return;
        const text = paragraphLines.join(' ').replace(/\s+/g, ' ').trim();
        if (text) blocks.push(`        <p>${escapeHtml(text)}</p>`);
        paragraphLines = [];
    }

    function flushList() {
        if (!listItems || listItems.length === 0) return;
        const tag = listType === 'ol' ? 'ol' : 'ul';
        const items = listItems
            .map((item) => `          <li>${escapeHtml(item)}</li>`)
            .join('\n');
        blocks.push(`        <${tag}>\n${items}\n        </${tag}>`);
        listItems = null;
        listType = null;
    }

    for (const rawLine of markdown.replace(/\r\n?/g, '\n').split('\n')) {
        const line = rawLine.replace(/\s+$/, '');

        if (!line.trim()) {
            flushParagraph();
            flushList();
            continue;
        }

        if (isListItem(line)) {
            flushParagraph();
            const type = isOrderedListItem(line) ? 'ol' : 'ul';
            if (listType !== type) {
                flushList();
                listType = type;
                listItems = [];
            }
            listItems.push(listItemText(line).replace(/\s+/g, ' '));
            continue;
        }

        flushList();
        paragraphLines.push(line.trim());
    }

    flushParagraph();
    flushList();
    return blocks.join('\n');
}

function paragraphsFromMarkdown(markdown) {
    return markdownToHtml(markdown);
}

function pageShell({ title, description, stylesheetPath, homePath, body, extraHead = '', extraScripts = '' }) {
    return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="description" content="${escapeHtml(description)}">
  <title>${escapeHtml(title)} | ${escapeHtml(siteTitle)}</title>
  <link rel="stylesheet" href="${stylesheetPath}">${extraHead}
</head>
<body>
  <header class="site-header">
    <div class="container header-inner">
      <a class="site-title" href="${homePath}">${escapeHtml(siteTitle)}</a>
      <nav class="site-nav" aria-label="Primary navigation">
        <a class="nav-link" href="${homePath}">Home</a>
        <a class="nav-link" href="${homePath.replace('index.html', 'contact.html')}">Contact</a>
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
  </footer>${extraScripts}
</body>
</html>
`;
}

function escapeXml(value) {
    return value
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&apos;');
}

function toLastmod(timestamp) {
    if (!timestamp) return '';
    return new Date(timestamp).toISOString().slice(0, 10);
}

function buildSitemap(essays) {
    const newestLastmod = essays.length > 0
        ? Math.max(...essays.map((essay) => essay.createdAt || 0))
        : 0;

    const urls = [
        { loc: `${siteUrl}/`, lastmod: toLastmod(newestLastmod) },
        { loc: `${siteUrl}/contact.html` },
        ...essays.map((essay) => ({
            loc: `${siteUrl}/essays/${essay.slug}.html`,
            lastmod: toLastmod(essay.createdAt),
        })),
    ];

    const entries = urls.map(({ loc, lastmod }) => {
        const lastmodLine = lastmod ? `\n    <lastmod>${lastmod}</lastmod>` : '';
        return `  <url>\n    <loc>${escapeXml(loc)}</loc>${lastmodLine}\n  </url>`;
    }).join('\n');

    return `<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
${entries}
</urlset>
`;
}

function buildEssay(fileName) {
    const filePath = path.join(essaysSrcDir, fileName);
    const slug = fileName.replace(/\.md$/, '');
    const title = titleFromSlug(slug);
    const createdAt = essayCreatedAt(fileName);
    const markdown = fs.readFileSync(filePath, 'utf8');
    const description = `${title} — an essay from ${siteTitle}.`;
    const dateHtml = `        <p class="essay-date"><time datetime="${toFullDateTime(createdAt)}">${escapeHtml(formatGmtDateTime(createdAt))}</time></p>`;
    const body = `    <article class="essay">
      <header class="essay-header">
        <p class="eyebrow">Essay</p>
        <h1>${escapeHtml(title)}</h1>
${dateHtml}
      </header>
${paragraphsFromMarkdown(markdown)}
    </article>`;

    return {
        slug,
        title,
        createdAt,
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
            <span class="essay-link-date"><time datetime="${toDateTime(essay.createdAt)}">${escapeHtml(formatDate(essay.createdAt))}</time></span>
          </a>
        </li>`)
        .join('\n');

    const body = `    <section class="intro">
      <p class="eyebrow">Writing</p>
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

function buildContactPage() {
    const body = `    <section class="contact" aria-labelledby="contact-heading">
      <p class="eyebrow">Contact Me</p>

      <div id="contact-error" class="contact-error" role="alert" hidden></div>

      <form id="contact-form" class="contact-form" novalidate>
        <div class="form-field">
          <label for="name">Name</label>
          <input id="name" name="name" type="text" autocomplete="name" required maxlength="200">
          <p class="field-error" data-error-for="name" hidden></p>
        </div>
        <div class="form-field">
          <label for="email">Email</label>
          <input id="email" name="email" type="email" autocomplete="email" required maxlength="254">
          <p class="field-error" data-error-for="email" hidden></p>
        </div>
        <div class="form-field">
          <label for="subject">Subject</label>
          <input id="subject" name="subject" type="text" autocomplete="off" required maxlength="200">
          <p class="field-error" data-error-for="subject" hidden></p>
        </div>
        <div class="form-field">
          <label for="message">Message</label>
          <textarea id="message" name="message" rows="7" required maxlength="20000"></textarea>
          <p class="field-error" data-error-for="message" hidden></p>
        </div>
        <button class="button" type="submit">Send Email</button>
      </form>
    </section>`;

    return pageShell({
        title: 'Contact',
        description: 'Contact form for Krishen Mohan.',
        stylesheetPath: 'assets/style.css',
        homePath: 'index.html',
        body,
        extraScripts: '\n  <script src="assets/contact.js"></script>',
    });
}

fs.mkdirSync(assetsDir, { recursive: true });
fs.mkdirSync(essaysDir, { recursive: true });

const essayFiles = fs.readdirSync(essaysSrcDir)
    .filter((fileName) => fileName.endsWith('.md'))
    .sort((a, b) => a.localeCompare(b));

const essays = essayFiles.map(buildEssay);

for (const essay of essays) {
    fs.writeFileSync(path.join(essaysDir, `${essay.slug}.html`), essay.html);
}

const newestFirst = [...essays].sort((a, b) => {
    return b.createdAt - a.createdAt || a.slug.localeCompare(b.slug);
});

fs.writeFileSync(path.join(root, 'index.html'), buildIndex(newestFirst));
fs.writeFileSync(path.join(root, 'contact.html'), buildContactPage());
fs.writeFileSync(path.join(root, 'sitemap.xml'), buildSitemap(newestFirst));

console.log(`Built ${essays.length} essay pages, index.html, contact.html and sitemap.xml`);
