# Building IND-1

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `ind.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `ind.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.1` makes a release build; the package is `build/ind-0.1.fwsc`.

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `EN_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `EN_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

The build also generates `build/gen/` (the font). It fails if a soft-double routine is linked
(a `double` crept in) and checks the image, RAM and pool sizes.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the maths library against libm; the instrument (`tests/host/ind_test.c`:
Euclidean patterns, scales and degrees, section lengths, the render's levels, the arranger through a song,
the root on the bar, the breakdown / build / drop, fills, mutes, mutate, every layer alone, every setting at
its extremes, every piece); flash storage; Felucca's update-path tests against `build/ind.fwsc`; and the
whole app in the simulator (`tests/scenarios/*.ind`), with its screenshots and audio in `build/scenarios/`.

`host/build_host.sh` builds the simulator alone; `build/host/ind_host SCRIPT OUTDIR` runs one
script (the command list is at the top of `host/ind_host.c`).

- `build/host/ind_host tests/demo.ind build/demo`: about a minute of each piece, as WAV files (not part
  of the tests: nine minutes of audio).
- `tools/manual_shots.sh`: the manual's screenshots (`tests/manual.ind`) into `docs/img`, and the panel
  drawing (`tools/gen_manual_figures.py`).

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/ind.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update
mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB),
`bash tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Publishing a release

1. `./build.sh --release X.Y` (the identity, FM-1_5XXYYZZ, is what the installer checks; AMB-1's is
   FM-1_7..., X0X's FM-1_9...).
2. `web/emu/build.sh` (needs Emscripten) for the browser emulator.
3. `[IND_REPO=https://...] python3 tools/make_pages.py build/ind-X.Y.fwsc X.Y OUT_DIR`: the site (landing
   page, manual, browser emulator, web installer, firmware, and the complete source as a tarball, as the
   GPL asks) in a static folder, to put on GitHub Pages or any web server. The web installer needs it
   served over HTTPS (Web MIDI). The manual's source is `docs/MANUAL.md` (`tools/manual_html.py` turns it
   into the page).
