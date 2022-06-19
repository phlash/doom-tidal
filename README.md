# A port of DOOM for the EMF camp TiDAL badge (ESP32S3, ST7789)

Yet another DOOM port, this one specifically to be loaded on the TiDAL badge
as a standard _python_ module and integrated to the application manager.

## Why?

Seriously... ok: [https://knowyourmeme.com/memes/it-runs-doom](https://knowyourmeme.com/memes/it-runs-doom)

## How?

A much better question:

 * The 'hard bit' of running game logic, then rendering a 3D frame from the
   BSP data in the WAD and the player position, is handled by a gently
   massaged version of the idTech 1 engine, as supplied by the excellent
   [doomgeneric](https://github.com/ozkl/doomgeneric), it's a submodule
   of this project.
 * The 'other bits' of handling file I/O to the WAD, reading player inputs
   and blitting the frame is written in python using the TiDAL badge API. This
   gets us integration with the application manager and deployability via the
   [hatchery](https://2022.badge.emfcamp.org/) so everyone can play...
 * Sound you say? The badge does have Bluetooth(LE), so it should be possible
   to hook up some AirPods, maybe when the graphics are working?

## Tooling

 * The Xtensa ESP32S3 targetted version of GCC from Espressif cross-tools:
   https://github.com/espressif/crosstool-NG/releases, specifically the
   esp32s3-elf-gcc11_2_0-esp-2022r1-&lt;your host technology here&gt;.tar.xz
 * Micropython repo (it's a submodule of this project) for final assembly
   of compiled object code into `doomgeneric.mpy` loadable python module
 * Python3 plus `python3-pyelftools` (on my Debian system)
 * A copy of an appropriate WAD file (search for DOOM1.WAD for demo game)

## Build 'n Debug

First, ensure you have the Xtensa GCC cross-compiler for you host system
installed (unpacked) somewhere easy to refer to by environment variable.

Second, clone this project, and yank those submodules (but __not__ recursive!):
```bash
% cd <project dir>
% git submodule init --update
```

Thirdly, check you can access the TiDAL badge python REPL via serial port
as this is how we'll be loading the test app (with the `pyboard.py` program
from the micropython project).

Building should now work via GNU make, provided it's told where to find the
Xtensa GCC:
```bash
% make COMPILER=<path to xtensa top level>
..or..
% export COMPILER=<path to xtensa top level>
% make
```

Installing the built module and wrapper python looks like this:
```bash
% micropython/tools/pyboard.py --no-soft-reset -f mkdir /apps/doom
% micropython/tools/pyboard.py --no-soft-reset -f cp wrapper/__init__.py :/apps/doom/
% micropython/tools/pyboard.py --no-soft-reset -f cp build/doomgeneric.mpy :/apps/doom/
% micropython/tools/pyboard.py --no-soft-reset -f cp <path to .WAD file> :/apps/doom/game.wad
```

Debugging is by resetting the badge, then connecting your favourite serial
terminal program (I like `minicom`) to the TiDAL badge REPL, before selecting
`doom` app on the badge screen...

