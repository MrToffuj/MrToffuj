const http = require('http'), fs = require('fs'), path = require('path');
const { chromium } = require('playwright');
const root = __dirname;
const srv = http.createServer((q, r) => {
  const p = path.join(root, decodeURIComponent(q.url.split('?')[0]));
  if (!fs.existsSync(p)) { r.writeHead(404); return r.end(); }
  const ext = path.extname(p);
  r.writeHead(200, {'Content-Type': ext === '.js' ? 'text/javascript' : ext === '.json' ? 'application/json' : 'text/html'});
  fs.createReadStream(p).pipe(r);
}).listen(8765, async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await browser.newPage({ viewport: { width: 1600, height: 900 } });
  page.on('console', m => console.log('console:', m.text()));
  page.on('pageerror', e => console.log('error:', e.message));
  for (const [data, view, out] of JSON.parse(process.argv[2])) {
    await page.goto(`http://localhost:8765/index.html?data=${data}&view=${view}`);
    await page.waitForFunction(() => document.title === 'done', null, { timeout: 60000 });
    await page.screenshot({ path: path.join(root, out) });
    console.log('saved', out);
  }
  await browser.close(); srv.close();
});
