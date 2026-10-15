#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the IND-1 site (a static folder: GitHub Pages or any web server serves it as it is):

  index.html                  what IND-1 is, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        IND-1 in the browser (build/emu, from web/emu/build.sh)
  install/index.html          the web installer (web/ind_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  firmware/ind-VERSION.fwsc   the package the installer writes; also the download
  source/                     the complete source (GPL-3.0: whoever gets the firmware gets the source),
                              as a tarball of this tree; and IND_REPO, if set, the repository's address

  [IND_REPO=https://...] tools/make_pages.py build/ind-0.1.fwsc 0.1 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_5XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = os.environ.get("IND_REPO", "")

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>IND-1</title>
<meta name="description" content="A self-playing industrial machine for the M-VAVE FM-1, after KMFDM, Nine Inch Nails and Ministry: distorted drums, a sequenced bass, guitars, FM stabs, metal and noise, in songs it arranges itself.">
<meta name="theme-color" content="#121214">
<style>
/* a factory at night, as IND-1's screen: near-black, steel greys, hazard orange */
:root { --paper: #121214; --sheet: #222226; --ink: #e8e4d8; --muted: #9a978e; --hazard: #ff8a18; --rule: #3a3a40;
        color-scheme: dark; }
body { background: var(--paper); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 48px 64px;
       font: 400 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; }
main { max-width: 42rem; margin: 0 auto; }
h1 { font-size: 3rem; font-weight: 700; line-height: 1; letter-spacing: .06em; margin: 0 0 .3rem; }
.sub { text-transform: uppercase; letter-spacing: .08em; font-size: .9rem; margin: 0 0 1.2rem; padding-bottom: .8rem;
       border-bottom: 4px solid var(--hazard); }
h2 { font-size: 1.1rem; font-weight: 400; text-transform: uppercase; letter-spacing: .06em; margin: 2.4rem 0 .6rem;
     padding-top: .6rem; border-top: 1px solid var(--rule); color: var(--hazard); }
.lede { font-size: 1.15rem; margin: 0 0 1.6rem; max-width: 36rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 0 0 1.8rem; }
.shots img { width: 100%; height: auto; image-rendering: pixelated; border: 1px solid var(--rule); }
.ways { display: grid; gap: 10px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--sheet); border: 1px solid var(--rule); padding: 12px 16px; }
.ways a:first-child { background: var(--hazard); color: var(--paper); }
.ways a:first-child span { color: var(--paper); }
.ways a:hover, .ways a:focus-visible { border-color: var(--hazard); outline: none; }
.ways strong { font-weight: 600; font-size: 1.15rem; text-transform: uppercase; letter-spacing: .05em; display: block; }
.ways span { color: var(--muted); font-size: .95rem; }
.ways em.v { font-style: normal; text-transform: none; letter-spacing: 0; }
ul.layers { padding-left: 1.2rem; margin: 0 0 1rem; }
ul.layers li { margin: 0 0 .4rem; }
ul.layers li::marker { color: var(--hazard); }
p.small { color: var(--muted); font-size: .9rem; }
a { color: var(--ink); text-decoration-color: var(--hazard); text-decoration-thickness: 2px; text-underline-offset: 3px; }
code { font-size: .9em; }
</style>
</head>
<body>
<main>
<h1>IND-1</h1>
<p class="sub">An industrial machine for the M-VAVE FM-1</p>
<p class="lede">A self-playing industrial machine, after the records of KMFDM, Nine Inch Nails and Ministry. It
starts the moment it's switched on and arranges its own songs: intros, verses, builds, choruses, breakdowns and
drops. You steer it: press a key and the whole machine moves to that root on the next bar; throw in fills,
breakdowns and drops, mute layers, mutate the riffs.</p>
<div class="shots">
  <img src="img/screen-verse.png" width="240" height="240" alt="IND-1's screen: the piece Foundry in a verse, the bar's grid of drums, metal, bass, guitar and synth">
  <img src="img/screen-breakdown.png" width="240" height="240" alt="IND-1's screen: a breakdown">
  <img src="img/screen-drums.png" width="240" height="240" alt="IND-1's screen: the Drums page">
</div>
<nav class="ways" aria-label="Get IND-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Getting started in a few minutes, then everything else.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="source/"><strong>Source</strong><span>GPL-3.0. Built on Felucca by Hügelton Instruments, by way of X0X and AMB-1.</span></a>
</nav>

<h2>The machine</h2>
<ul class="layers">
  <li><b>Drums.</b> A distorted kick, a snare or a clap through a gated plate, hats, toms for the fills; six grooves from EBM's four on the floor to thrash, through a crusher for the sampler's crunch.</li>
  <li><b>Bass.</b> A Pro-1-style mono synth: a saw and a square through a squelching low-pass, driven, in sixteenths, gallops and octaves.</li>
  <li><b>Guitar.</b> Two synthesized guitars, left and right: palm-muted chugs and open power chords through a high-gain amp and a cabinet.</li>
  <li><b>Synth.</b> FM and saw stabs and lines in the spirit of the Xpander and the Prophet VS.</li>
  <li><b>Metal and noise.</b> Inharmonic clangs on Euclidean patterns; a drone or static under the intros and breakdowns, risers into the drops.</li>
</ul>
<p>Eight pieces, from four-on-the-floor EBM to thrash and half-time grind, each remembering how you left it.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
M-VAVE and FM-1 are trademarks of their owners. IND-1 is not affiliated with M-VAVE, KMFDM, Nine Inch Nails or Ministry.</p>
</main>
</body>
</html>
"""

SOURCE = """<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>IND-1 source</title>
<style>body { background: #121214; color: #e8e4d8; font: 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; margin: 48px 16px; }
main { max-width: 42rem; margin: 0 auto; } a { color: #e8e4d8; text-decoration-color: #ff8a18; text-decoration-thickness: 2px; }</style></head>
<body><main><h1>IND-1 source</h1>
<p>IND-1 is free software under the GNU General Public License, version 3 only. The complete source of v__VERSION__:
<a href="__TAR__">__TAR__</a>.__REPOLINE__</p>
<p><a href="../">Back to IND-1</a></p></main></body></html>
"""


def source_tarball(dest, version):
    """the tree as it is (no build output, no caches): what the GPL asks to give with the firmware"""
    name = f"ind-1-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}-src"
    skip = {"build", "__pycache__", ".git", ".DS_Store", ".serena"}

    def keep(ti):
        return None if set(Path(ti.name).parts) & skip else ti
    with tarfile.open(dest / f"{name}.tar.gz", "w:gz") as t:
        t.add(SRC, arcname=name, filter=keep)
    return f"{name}.tar.gz"


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_5\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"ind-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware", "source"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "ind_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"ind_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name),
                                    encoding="utf-8")
    # the source, for the GPL
    tar = source_tarball(out / "source", version)
    repo = f' The repository: <a href="{html.escape(REPO)}">{html.escape(REPO)}</a>.' if REPO else ""
    (out / "source" / "index.html").write_text(SOURCE.replace("__VERSION__", html.escape(version)).replace("__TAR__", tar)
                                               .replace("__REPOLINE__", repo), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "ind.wasm").exists():
        raise SystemExit("no build/emu/ind.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, emu/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
