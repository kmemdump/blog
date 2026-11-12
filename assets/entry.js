(function () {
  function escapeHtml(s) {
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  function renderInline(text) {
    // code spans first
    var code = [];
    text = text.replace(/`([^`]+)`/g, function (_, c) {
      var i = code.length;
      code.push('<code>' + escapeHtml(c) + '</code>');
      return '\x00C' + i + '\x00';
    });
    // links [text](url)
    text = text.replace(/\[([^\]]+)\]\(([^)]+)\)/g, function (_, t, u) {
      var safe = u.replace(/"/g, '&quot;');
      return '<a href="' + safe + '">' + renderInlinePlain(t) + '</a>';
    });
    // bold ** or __
    text = text.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    text = text.replace(/__([^_]+)__/g, '<strong>$1</strong>');
    // italic * or _
    text = text.replace(/\*([^*\n]+)\*/g, '<em>$1</em>');
    text = text.replace(/_([^_\n]+)_/g, '<em>$1</em>');
    // escape remaining
    text = escapeHtml(text);
    // restore code spans (already escaped inside)
    text = text.replace(/\x00C(\d+)\x00/g, function (_, i) { return code[Number(i)]; });
    // restore links/strong/em tags we injected (they were escaped) - easier approach: do escaping carefully
    return text;
  }

  function renderInlinePlain(t) { return t; }

  function inlineFormat(text) {
    // protect code spans
    var codes = [];
    text = text.replace(/`([^`]+)`/g, function (_, c) {
      codes.push('<code>' + escapeHtml(c) + '</code>');
      return '@@CODE' + (codes.length - 1) + '@@';
    });
    // escape html
    var out = escapeHtml(text);
    // links
    out = out.replace(/\[([^\]]+)\]\(([^)]+)\)/g, function (_, t, u) {
      // t,u already escaped
      var url = u;
      return '<a href="' + url + '">' + t + '</a>';
    });
    // bold and italic
    out = out.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    out = out.replace(/__([^_]+)__/g, '<strong>$1</strong>');
    out = out.replace(/(^|[\s(])\*([^*\n]+)\*/g, '$1<em>$2</em>');
    out = out.replace(/(^|[\s(])_([^_\n]+)_/g, '$1<em>$2</em>');
    // restore code
    out = out.replace(/@@CODE(\d+)@@/g, function (_, i) { return codes[Number(i)]; });
    return out;
  }

  function renderMarkdown(md) {
    var lines = md.replace(/\r\n?/g, '\n').split('\n');
    var html = [];
    var i = 0;
    function isBlank(l) { return /^\s*$/.test(l); }
    function isUl(l) { return /^\s*[-*+]\s+/.test(l); }
    function isOl(l) { return /^\s*\d+[.)]\s+/.test(l); }
    function isHeading(l) { return /^\s{0,3}#{1,6}\s+/.test(l); }
    function isFence(l) { return /^\s*```/.test(l); }
    function isBlockquote(l) { return /^\s*>\s?/.test(l); }

    while (i < lines.length) {
      var line = lines[i];
      if (isBlank(line)) { i++; continue; }
      if (isFence(line)) {
        var lang = line.replace(/^\s*```/, '').trim();
        i++;
        var codeLines = [];
        while (i < lines.length && !isFence(lines[i])) { codeLines.push(lines[i]); i++; }
        if (i < lines.length) i++;
        html.push('<pre><code>' + escapeHtml(codeLines.join('\n')) + '</code></pre>');
        continue;
      }
      if (isHeading(line)) {
        var m = line.match(/^\s{0,3}(#{1,6})\s+(.*)$/);
        var level = m[1].length;
        var text = m[2].replace(/\s+#*\s*$/, '');
        html.push('<h' + level + '>' + inlineFormat(text) + '</h' + level + '>');
        i++; continue;
      }
      if (isUl(line)) {
        html.push('<ul>');
        while (i < lines.length && isUl(lines[i])) {
          var item = lines[i].replace(/^\s*[-*+]\s+/, '');
          html.push('<li>' + inlineFormat(item) + '</li>');
          i++;
        }
        html.push('</ul>');
        continue;
      }
      if (isOl(line)) {
        html.push('<ol>');
        while (i < lines.length && isOl(lines[i])) {
          var item = lines[i].replace(/^\s*\d+[.)]\s+/, '');
          html.push('<li>' + inlineFormat(item) + '</li>');
          i++;
        }
        html.push('</ol>');
        continue;
      }
      if (isBlockquote(line)) {
        var q = [];
        while (i < lines.length && isBlockquote(lines[i])) {
          q.push(lines[i].replace(/^\s*>\s?/, ''));
          i++;
        }
        html.push('<blockquote>' + renderMarkdown(q.join('\n')) + '</blockquote>');
        continue;
      }
      // paragraph: gather consecutive non-blank, non-special lines
      var para = [];
      while (i < lines.length && !isBlank(lines[i]) && !isHeading(lines[i]) && !isUl(lines[i]) && !isOl(lines[i]) && !isFence(lines[i]) && !isBlockquote(lines[i])) {
        para.push(lines[i]);
        i++;
      }
      if (para.length) {
        html.push('<p>' + inlineFormat(para.join('\n').replace(/\n/g, ' ')) + '</p>');
      } else {
        // fallback
        html.push('<p>' + inlineFormat(line) + '</p>');
        i++;
      }
    }
    return html.join('\n');
  }

  var md = window.__ENTRY_MARKDOWN__ || '';
  var el = document.getElementById('content');
  if (el) {
    el.innerHTML = renderMarkdown(md);
  }
})();
