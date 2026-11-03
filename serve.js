#!/usr/bin/env node

const http = require('http');
const fs = require('fs');
const path = require('path');

const root = __dirname;
const requestedPort = process.env.PORT || process.argv[2] || 8080;
const port = Number(requestedPort);

if (!Number.isInteger(port) || port < 1 || port > 65535) {
  console.error(`Invalid port: ${requestedPort}`);
  process.exit(1);
}

const contentTypes = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.md': 'text/markdown; charset=utf-8',
  '.txt': 'text/plain; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.webp': 'image/webp',
  '.ico': 'image/x-icon',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
};

function resolveRequestPath(pathname) {
  let decodedPath;

  try {
    decodedPath = decodeURIComponent(pathname);
  } catch {
    return null;
  }

  const segments = decodedPath.split(/[\\/]/);

  if (segments.some((segment) => segment.startsWith('.') && segment.length > 1)) {
    return null;
  }

  const resolved = path.resolve(root, `.${decodedPath}`);
  const rootWithSeparator = root.endsWith(path.sep) ? root : `${root}${path.sep}`;

  if (resolved !== root && !resolved.startsWith(rootWithSeparator)) {
    return null;
  }

  return resolved;
}

function sendText(res, statusCode, message) {
  const body = `${statusCode} ${message}\n`;

  res.writeHead(statusCode, {
    'Content-Type': 'text/plain; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
    'Cache-Control': 'no-store',
  });
  res.end(body);
}

const server = http.createServer((req, res) => {
  if (req.method !== 'GET' && req.method !== 'HEAD') {
    sendText(res, 405, 'Method Not Allowed');
    return;
  }

  let pathname;

  try {
    pathname = new URL(req.url, 'http://localhost').pathname;
  } catch {
    sendText(res, 400, 'Bad Request');
    return;
  }

  let filePath = resolveRequestPath(pathname);

  if (!filePath) {
    sendText(res, 403, 'Forbidden');
    return;
  }

  if (fs.existsSync(filePath) && fs.statSync(filePath).isDirectory()) {
    filePath = path.join(filePath, 'index.html');
  }

  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    sendText(res, 404, 'Not Found');
    return;
  }

  const extension = path.extname(filePath).toLowerCase();
  const contentType = contentTypes[extension] || 'application/octet-stream';
  const stats = fs.statSync(filePath);

  res.writeHead(200, {
    'Content-Type': contentType,
    'Content-Length': stats.size,
    'Cache-Control': 'no-store',
  });

  if (req.method === 'HEAD') {
    res.end();
    return;
  }

  fs.createReadStream(filePath).pipe(res);
});

server.on('error', (error) => {
  console.error(`Could not start server on port ${port}: ${error.message}`);
  process.exit(1);
});

server.listen(port, () => {
  console.log(`Serving ${root} at http://localhost:${port}/`);
  console.log('Press Ctrl+C to stop.');
});
