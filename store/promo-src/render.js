// Rendert Store-Grafiken (Banner, Promo-Bilder, Icons) aus den Emulator-Screenshots.
// Aufruf: node store/promo-src/render.js   (benoetigt playwright + chromium)
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright');

const ROOT = path.resolve(__dirname, '..');
const shot = (name) => 'data:image/png;base64,' +
  fs.readFileSync(path.join(ROOT, 'screenshots', 'emery', name)).toString('base64');

const css = `
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Inter', sans-serif; color: #fff; overflow: hidden; }
  .bg { position: absolute; inset: 0;
        background: radial-gradient(ellipse at 70% 40%, #1d3f86 0%, #0c1a3d 55%, #070f24 100%); }
  .tiles { position: absolute; inset: 0; opacity: .10;
           background-image: linear-gradient(#55aaff 2px, transparent 2px), linear-gradient(90deg, #55aaff 2px, transparent 2px);
           background-size: 48px 48px; mask-image: radial-gradient(ellipse at 70% 50%, #000 0%, transparent 75%); }
  .watch { position: absolute; }
  .strap { position: absolute; left: 50%; transform: translateX(-50%); width: 62%; background: linear-gradient(90deg,#1b1f27,#2b313c 50%,#1b1f27); }
  .strap.top { border-radius: 18px 18px 0 0; }
  .strap.bot { border-radius: 0 0 18px 18px; }
  .body { position: absolute; inset: 0; border-radius: 15%/13%;
          background: linear-gradient(145deg, #4a515e 0%, #262b34 45%, #15181e 100%);
          box-shadow: 0 30px 60px rgba(0,0,0,.55), inset 0 2px 3px rgba(255,255,255,.25), inset 0 -3px 6px rgba(0,0,0,.6); }
  .bezel { position: absolute; background: #050608; border-radius: 6%/5%;
           box-shadow: inset 0 0 0 2px #000, 0 0 0 2px rgba(255,255,255,.06); }
  .screen { position: absolute; image-rendering: pixelated; border-radius: 2px; }
  .glare { position: absolute; inset: 0; border-radius: 15%/13%;
           background: linear-gradient(130deg, rgba(255,255,255,.10) 0%, rgba(255,255,255,0) 40%); pointer-events: none; }
  .btn { position: absolute; width: 3.2%; background: linear-gradient(90deg,#5b6370,#2a2f38); border-radius: 4px; }
  .txt { z-index: 2; }
  h1 { font-weight: 900; letter-spacing: -0.02em; line-height: 1.02; }
  .accent { color: #ffe95a; }
  .sub { color: #b9cdf5; font-weight: 500; }
  .pill { display: inline-block; padding: 8px 16px; border-radius: 999px; background: rgba(85,170,255,.16);
          color: #cfe3ff; font-weight: 600; border: 1px solid rgba(85,170,255,.35); }
`;

// Pebble Time 2: Display 200x228, Gehaeuse grob proportional nachgebildet
function watch(img, scale, x, y, rot = 0) {
  const sw = 200 * scale, sh = 228 * scale;
  const padX = 34 * scale, padTop = 44 * scale, padBot = 52 * scale;
  const bw = sw + 2 * padX, bh = sh + padTop + padBot;
  const strapH = 120 * scale, strapTop = 34 * scale;
  return `
  <div class="watch" style="left:${x}px; top:${y}px; width:${bw}px; height:${bh}px; transform: rotate(${rot}deg);">
    <div class="strap top" style="top:${-strapTop}px; height:${strapTop + 20}px;"></div>
    <div class="strap bot" style="bottom:${-strapH}px; height:${strapH + 20}px;"></div>
    <div class="body"></div>
    <div class="btn" style="left:-1.6%; top:30%; height:16%;"></div>
    <div class="btn" style="right:-1.6%; top:20%; height:14%;"></div>
    <div class="btn" style="right:-1.6%; top:40%; height:18%;"></div>
    <div class="btn" style="right:-1.6%; top:64%; height:14%;"></div>
    <div class="bezel" style="left:${padX - 10 * scale}px; top:${padTop - 10 * scale}px; width:${sw + 20 * scale}px; height:${sh + 20 * scale}px;"></div>
    <img class="screen" src="${img}" style="left:${padX}px; top:${padTop}px; width:${sw}px; height:${sh}px;">
    <div class="glare"></div>
  </div>`;
}

function mineSvg(size, tile) {
  const bg = tile ? `<rect x="2" y="2" width="96" height="96" rx="18" fill="#0055aa"/>
    <rect x="2" y="2" width="92" height="92" rx="16" fill="#aaffff"/>
    <rect x="7" y="7" width="86" height="86" rx="13" fill="#55aaff"/>` : '';
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 100 100">
    ${bg}
    <g stroke="#000" stroke-width="6" stroke-linecap="round">
      <line x1="16" y1="54" x2="76" y2="54"/><line x1="46" y1="24" x2="46" y2="84"/>
      <line x1="25" y1="33" x2="67" y2="75"/><line x1="25" y1="75" x2="67" y2="33"/>
    </g>
    <circle cx="46" cy="54" r="20" fill="#000"/>
    <rect x="34" y="42" width="8" height="8" rx="1" fill="#fff"/>
    <line x1="82" y1="12" x2="82" y2="42" stroke="#000" stroke-width="5" stroke-linecap="round"/>
    <path d="M80 12 L60 20 L80 28 Z" fill="#ff3030"/>
  </svg>`;
}

const page = (w, h, inner) => `<!doctype html><html><head><meta charset="utf-8"><style>${css}</style></head>
  <body style="width:${w}px;height:${h}px;position:relative"><div class="bg"></div><div class="tiles"></div>${inner}</body></html>`;

const jobs = [
  // Banner / Header (720x320)
  { out: 'banner_720x320.png', w: 720, h: 320, html: page(720, 320, `
      <div style="position:absolute; left:40px; top:58px; width:390px;">
        <div style="display:flex; align-items:center; gap:14px; margin-bottom:16px;">
          ${mineSvg(56, true)}<span class="pill" style="font-size:14px">for Pebble Time 2</span>
        </div>
        <h1 style="font-size:56px;">Mine<span class="accent">sweeper</span></h1>
        <p class="sub" style="font-size:19px; margin-top:12px;">The classic puzzle on your wrist.<br>Two buttons. No touch. Pure skill.</p>
      </div>
      ${watch(shot('1_gameplay.png'), 1.0, 452, 34, 6)}`) },

  // Promo 1: Gameplay
  { out: 'promo_1_gameplay.png', w: 1080, h: 1080, html: page(1080, 1080, `
      <div class="txt" style="position:absolute; left:80px; top:90px; width:920px;">
        <span class="pill" style="font-size:24px">Minesweeper for Pebble Time 2</span>
        <h1 style="font-size:92px; margin-top:28px;">Sweep mines<br>with <span class="accent">two buttons.</span></h1>
        <p class="sub" style="font-size:32px; margin-top:22px;">UP moves across, DOWN moves down.<br>SELECT opens, hold SELECT to flag.</p>
      </div>
      ${watch(shot('1_gameplay.png'), 1.55, 600, 520, -4)}`) },

  // Promo 2: Animationen
  { out: 'promo_2_animations.png', w: 1080, h: 1080, html: page(1080, 1080, `
      <div class="txt" style="position:absolute; left:80px; top:90px; width:920px;">
        <h1 style="font-size:92px;">Big <span style="color:#ff6a5a">BOOM.</span><br>Bigger <span class="accent">trophies.</span></h1>
        <p class="sub" style="font-size:32px; margin-top:22px;">Explosion, confetti and a trophy animation<br>every time you set a new record.</p>
      </div>
      ${watch(shot('2_boom.png'), 1.25, 70, 520, -6)}
      ${watch(shot('4_new_record.png'), 1.25, 560, 470, 5)}`) },

  // Promo 3: Schwierigkeit & Highscores
  { out: 'promo_3_highscores.png', w: 1080, h: 1080, html: page(1080, 1080, `
      <div class="txt" style="position:absolute; left:80px; top:90px; width:920px;">
        <h1 style="font-size:92px;">3 levels.<br><span class="accent">Beat your best.</span></h1>
        <p class="sub" style="font-size:32px; margin-top:22px;">Easy, Medium, Hard - with a top-5 time list<br>for every difficulty.</p>
      </div>
      ${watch(shot('6_menu.png'), 1.25, 70, 500, -5)}
      ${watch(shot('5_highscores.png'), 1.25, 560, 520, 5)}`) },

  // Store-Icons
  { out: 'icon_144x144.png', w: 144, h: 144, transparent: true,
    html: `<!doctype html><html><body style="margin:0;background:transparent">${mineSvg(144, true)}</body></html>` },
  { out: 'icon_48x48.png', w: 48, h: 48, transparent: true,
    html: `<!doctype html><html><body style="margin:0;background:transparent">${mineSvg(48, true)}</body></html>` },
];

(async () => {
  const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || undefined });
  const outDir = path.join(ROOT, 'graphics');
  fs.mkdirSync(outDir, { recursive: true });
  for (const j of jobs) {
    const p = await browser.newPage({ viewport: { width: j.w, height: j.h } });
    await p.setContent(j.html, { waitUntil: 'load' });
    await p.evaluate(() => document.fonts.ready);
    await p.screenshot({ path: path.join(outDir, j.out), omitBackground: !!j.transparent });
    await p.close();
    console.log('rendered', j.out);
  }
  await browser.close();
})();
