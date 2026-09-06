// Runs the in-page Markdown renderer (the <script> inside markdownToRichHtml() in
// src/tree_model.h) under Node with the bundled markdown-it and stubs for the DOM, KaTeX and
// Mermaid, and checks the HTML it produces. Invoked by CTest when `node` is available:
//   node tests/markdown_renderer_test.js
'use strict';
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const header = fs.readFileSync(path.join(root, 'src', 'tree_model.h'), 'utf8').split('\r\n').join('\n');
const start = header.indexOf('<script>\n// Markdown renderer for the web preview');
const end = header.indexOf('</script>\n</body></html>', start);
if (start < 0 || end < 0) {
  console.error('renderer script not found in src/tree_model.h');
  process.exit(1);
}
const script = header.slice(start + '<script>\n'.length, end).replace(/theme: %7/, "theme: 'default'");
const markdownit = require(path.join(root, 'assets', 'web', 'markdown-it.min.js'));

function render(markdown) {
  let html = '';
  global.window = { markdownit };
  global.document = {
    getElementById(id) {
      if (id === 'mycel-src') return { textContent: markdown };
      return { set innerHTML(v) { html = v; } };
    },
    querySelectorAll() { return []; },
  };
  global.katex = { renderToString(body, opts) { return '<math' + (opts.displayMode ? ' display' : '') + '>' + body + '</math>'; } };
  global.mermaid = { initialize() {}, run() {} };
  // eslint-disable-next-line no-eval
  eval(script);
  return html;
}

let failures = 0;
function check(label, markdown, expectations) {
  const html = render(markdown);
  const problems = [];
  for (const e of expectations) {
    if (typeof e === 'string' ? !html.includes(e) : !e.test(html)) {
      problems.push(e.toString());
    }
  }
  if (problems.length) {
    failures++;
    console.error(`${label} failed\n  html: ${JSON.stringify(html)}\n  missing: ${problems.join(' | ')}`);
  }
}
function checkNot(label, markdown, forbidden) {
  const html = render(markdown);
  const hits = forbidden.filter((f) => (typeof f === 'string' ? html.includes(f) : f.test(html)));
  if (hits.length) {
    failures++;
    console.error(`${label} failed\n  html: ${JSON.stringify(html)}\n  unexpected: ${hits.join(' | ')}`);
  }
}

// Line breaks and basic Markdown.
check('newline is a line break', '一行目\n二行目', ['<p>一行目<br>\n二行目</p>']);
check('blank line is a paragraph', 'a\n\nb', ['<p>a</p>', '<p>b</p>']);
check('gfm table', '| h |\n|---|\n| c |', ['<table>', '<th>h</th>', '<td>c</td>']);
check('nested list', '- a\n  - b\n- c', [/<ul>\s*<li>a\s*<ul>\s*<li>b<\/li>/]);
check('strikethrough and code', '~~s~~ `x`', ['<s>s</s>', '<code>x</code>']);
checkNot('raw html is escaped', '<b>x</b>', ['<b>x</b>']);

// Mermaid and math.
check('mermaid fence', '```mermaid\ngraph TD; A-->B\n```', ['<div class="mermaid">graph TD; A--&gt;B\n</div>']);
check('other fence', '```js\ncode();\n```', ['<pre><code class="language-js">code();\n</code></pre>']);
check('inline math', 'x $a_1$ y', ['<math>a_1</math>']);
check('display math', '$$E=mc^2$$', ['<math display>E=mc^2</math>']);
check('math with underscores keeps them', '$a_{i}b_{j}$', ['<math>a_{i}b_{j}</math>']);
check('dollar inside code span is literal', '`$x$`', ['<code>$x$</code>']);
checkNot('dollar inside code span is not math', '`$x$`', ['<math>']);

// GitHub Alerts.
check('alert note', '> [!NOTE]\n> 本文', ['<aside class="markdown-alert markdown-alert-note">', 'markdown-alert-title', 'Note</div>', '<p>本文</p>', '</aside>']);
check('alert with paragraphs, list, emphasis', '> [!WARNING]\n> **重要:** `config.json`\n>\n> 次の段落\n> - 項目',
      ['markdown-alert-warning', 'Warning</div>', '<strong>重要:</strong> <code>config.json</code>', '<p>次の段落</p>', '<li>項目</li>']);
check('alert title only', '> [!TIP]', ['markdown-alert-tip', 'Tip</div>']);
checkNot('alert title only has no empty paragraph', '> [!TIP]', ['<p></p>']);
check('lower-case marker is a plain quote', '> [!note]\n> x', ['<blockquote>', '[!note]']);
checkNot('lower-case marker is not an alert', '> [!note]\n> x', ['markdown-alert']);
check('plain quote joins lines', '> a\n> b', ['<blockquote>', 'a<br>\nb']);

// Aozora ruby.
const ruby = (b, r) => `<ruby>${b}<rp>（</rp><rt>${r}</rt><rp>）</rp></ruby>`;
check('shorthand ruby', '吾輩《わがはい》は猫である', [ruby('吾輩', 'わがはい') + 'は猫である']);
check('shorthand stops at kana', '耳まで火照《ほて》って', ['耳まで' + ruby('火照', 'ほて') + 'って']);
check('repetition mark', '稍々《やや》', [ruby('稍々', 'やや')]);
check('explicit full-width bar', '武州｜青梅《おうめ》の宿', ['武州' + ruby('青梅', 'おうめ') + 'の宿']);
check('explicit half-width bar', '孤立語・|膠着語《こうちゃくご》・屈折語', ['孤立語・' + ruby('膠着語', 'こうちゃくご') + '・屈折語']);
check('nearest bar wins', 'a | b|膠着語《x》 c', ['a | b' + ruby('膠着語', 'x') + ' c']);
check('two rubies', '一応《いちおう》何時《いつ》もの', [ruby('一応', 'いちおう') + ruby('何時', 'いつ') + 'もの']);
check('ruby inside emphasis', '**太字《ふとじ》**', ['<strong>' + ruby('太字', 'ふとじ') + '</strong>']);
check('ruby inside alert', '> [!NOTE]\n> 青梅《おうめ》', ['markdown-alert-note', ruby('青梅', 'おうめ')]);
check('no kanji base stays text', 'かな《よみ》', ['かな《よみ》']);
check('unclosed stays text', '青梅《おうめ', ['青梅《おうめ']);
check('explicit unclosed stays text', '｜青梅《', ['｜青梅《']);
check('inline code keeps notation', '`青梅《おうめ》`', ['<code>青梅《おうめ》</code>']);
check('fence keeps notation', '```text\n青梅《おうめ》\n```', ['青梅《おうめ》\n</code></pre>']);
checkNot('inline code has no ruby', '`青梅《おうめ》`', ['<ruby>']);
check('ruby with html-sensitive reading is escaped', '漢《<b>》', [ruby('漢', '&lt;b&gt;')]);

if (failures) {
  console.error(`${failures} renderer check(s) failed`);
  process.exit(1);
}
console.log('markdown_renderer tests passed');
