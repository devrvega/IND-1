#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""docs/img/ind1.gif: the README's animation, from the simulator. Foundry plays through the quest (the
halls, the campfire, the stair, the Warden, the battle), then the song editor and the drums editor, a frame
every 100 ms, each LCD pixel drawn as 2 x 2.

  tools/readme_gif.py        (builds the simulator first; needs Pillow)"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

SRC = Path(__file__).resolve().parents[1]
STEP_MS = 100
# what happens, and for how long: (commands before, frames)
PLAN = [
    (["until section 1 30000", "wait 1500"], 16),                 # a verse: the halls
    (["tap REC", "until section 4 6000", "wait 1200"], 12),       # the breakdown: the campfire
    (["tap REC", "until section 2 6000", "wait 1500"], 12),       # the build: the stair
    (["until section 5 12000"], 16),                              # the drop: the Warden
    (["tap EDIT", "until section 3 30000", "wait 1000"], 12),     # a chorus: the battle
    (["tap HOME"], 10),                                           # the song editor
    (["tap SEQ"], 12),                                            # the drums editor
]


def main():
    subprocess.run(["sh", str(SRC / "host" / "build_host.sh")], check=True, capture_output=True)
    with tempfile.TemporaryDirectory() as tmp:
        lines, n = ["wait 400"], 0
        for cmds, frames in PLAN:
            lines += cmds
            for _ in range(frames):
                lines += [f"wait {STEP_MS}", f"shot f{n:03d}.png"]
                n += 1
        script = Path(tmp) / "gif.ind"
        script.write_text("\n".join(lines) + "\n")
        subprocess.run([str(SRC / "build" / "host" / "ind_host"), str(script), tmp], check=True, capture_output=True)
        frames = [Image.open(Path(tmp) / f"f{i:03d}.png").convert("RGB").resize((480, 480), Image.NEAREST)
                  for i in range(n)]
    pal = frames[len(frames) // 3].quantize(colors=64, method=Image.Quantize.MEDIANCUT)
    out = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    dest = SRC / "docs" / "img" / "ind1.gif"
    out[0].save(dest, save_all=True, append_images=out[1:], duration=STEP_MS, loop=0, optimize=True)
    print(f"{dest}: {n} frames, {dest.stat().st_size // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
