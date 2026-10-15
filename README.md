# IND-1 for the M-VAVE FM-1

IND-1 turns the M-VAVE FM-1 into a self-playing industrial machine, after the late-80s and 90s records of
KMFDM, Nine Inch Nails and Ministry. It starts the moment it's switched on and arranges its own songs
(intros, verses, builds, choruses, breakdowns, drops) out of six layers: distorted drums through a
crusher, a Pro-1-style sequenced bass, two synthesized guitars, Xpander / Prophet VS-style FM stabs,
metal percussion, and a bed of drone and static. You steer it: a key moves the whole machine to its root
on the next bar; buttons throw in fills, breakdowns, drops, mutes and new riffs.

![IND-1's screen: a verse](docs/img/screen-verse.png) ![A breakdown](docs/img/screen-breakdown.png) ![The Drums page](docs/img/screen-drums.png)

- **Read the manual**: [docs/MANUAL.md](docs/MANUAL.md)
- **Build it, test it, install it**: [BUILDING.md](BUILDING.md)

Installing it is at your own risk, though it's hard to get stuck: the web installer can put M-VAVE's
own firmware back, and an FM-1 that fails to start twice comes up in a safe mode the installer can reach.

## In short

| | |
|---|---|
| Keys | The root: the machine moves to the key's note on the next bar |
| PLAY · REC · EDIT | Run / stop · breakdown, build, drop · the next section |
| OCT− · OCT+ | A fill · mutate (new riffs and lines) |
| EDIT + a page button | Mute / unmute that layer |
| HOME · SEQ · LFO · ENV · ARP · SEL · FX · GLO | The pages: Machine, Drums, Bass, Guitar, Synth, Metal, Space, Setup; KNOB 1–4 set their values |
| SELECT | The previous or next page; with HOME held, the tempo |
| ALGORITHM · PRESETS | The mode · the piece (eight of them) |
| SAVE | Saves; held, puts the piece back to its recipe |

Hold any button for a second and a card says what it does.

## Building and testing

The toolchain is AMB-1's and X0X's (see [BUILDING.md](BUILDING.md)):

```
tests/run_tests.sh      # the instrument, the platform pieces, the app in a simulator
./build.sh              # build/ind.fwsc
python3 tools/fm1_install.py build/ind.fwsc     # install it over USB
```

`build/host/ind_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.ind`), with audio and screenshots coming out; `tests/demo.ind` records about a minute
of each piece. `web/emu/build.sh` builds the same code for the browser (the emulator).

## Credits

IND-1 is built on [AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal, a generative
ambient firmware for the FM-1: its engine's frame, screen, simulator and emulator. The reverb is Jon
Dattorro's plate ("Effect Design, Part 1", JAES 1997), by way of FoMni-1 and AMB-1; the Euclidean rhythms
are Godfried Toussaint's reading of Bjorklund's algorithm. The platform (hardware layer, USB, update
loader, storage, installer) is [Felucca](https://github.com/hugelton/Felucca)'s by Leo Kuroshita
(Hügelton Instruments), by way of [X0X](https://github.com/charlesvestal/fm1-x0x), FoMni and AMB-1. The
idea of running FM-1 firmware in the browser comes from [Groove OS](https://www.groove-os.com/).
GPL-3.0-only; see [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. IND-1 is not affiliated with M-VAVE, KMFDM, Nine Inch
Nails, Ministry, Charles Vestal or Hügelton Instruments.
