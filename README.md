# A port of DOOM for the EMF camp TiDAL badge (ESP32S3, ST7789)

Yet another DOOM port, this one specifically to be loaded on the TiDAL badge
as a standard _python_ module and integrated to the application manager.

## WARNING! DANGER WILL ROBINSON! CAVEAT EXECUTOR!

This is _experimental_ code, not a polished application, as such it has sharp
edges that may cause you problems - take note and use caution:

 * It's big, it hogs lots of flash storage space (~3MB) and writing to a device
   is excrutiatingly slow (thanks to `pyboard.py` assuming a real serial port).
   If you know how to speed up `pyboard.py` or if a different tool/technique is
   faster to copy over to flash, please make it so!
 * Running this _will overwrite your unused OTA partition_ with the `doom.bin`
   file. If you don't know what this means, you probably need to read some of
   the [documentation about the TiDAL badge](https://badge.emfcamp.org/wiki/TiDAL).
   There was a plan to back up the partition first, but it doesn't fit on the
   flash storage along with the WAD..
 * Installer logic is non-existant. You will want to tinker with the lines
   in the `Makefile` after an initial install to speed things up. If you think
   this could work better (file size/hashes to avoid copying?), please make it so :)

## Why?

Seriously... ok: [https://knowyourmeme.com/memes/it-runs-doom](https://knowyourmeme.com/memes/it-runs-doom)

## How?

A much better question:

 * The 'hard bit' of running game logic, then rendering a 3D frame from the
   level/texture data in the WAD and the player position, is handled by a gently
   massaged^ version of the idTech 1 engine, as supplied by the excellent
   [doomgeneric](https://github.com/ozkl/doomgeneric), it's a submodule
   of this project.
 * The 'other bits' of handling file I/O to the WAD, reading player inputs
   and blitting the frame is written in python using the TiDAL badge API. This
   gets us integration with the application manager and deployability via the
   [hatchery](https://2022.badge.emfcamp.org/) so everyone can play...
 * Sound you say? The badge does have Bluetooth(LE), so it should be possible
   to hook up some AirPods, maybe when the graphics are working? It may even be
   possible to hack up some PWM audio from a GPIO pin..

^  I've made about 4000LoC changes, to remove declared data and static variables
   since micropython's `.mpy` format doesn't support either. This turned out to
   be a huge waste of effort, as I now build a separate application binary that
   _does_ support such things, and load it myself. At some point I'll back out
   the enourmous pile of changes, and leave just the bug fixes and the scaler to
   240x150 that I added. Maybe.

## Tooling

 * The Xtensa ESP32S3 targetted version of GCC from Espressif cross-tools:
   https://github.com/espressif/crosstool-NG/releases, specifically the
   esp32s3-elf-gcc11_2_0-esp-2022r1-&lt;your host technology here&gt;.tar.xz
 * Micropython repo (it's a submodule of this project) for final assembly
   of compiled object code into `doomloader.mpy` loadable python module
   and providing the uPython dynamic module API.
 * Python3 plus `python3-pyelftools` package (on my Debian system)
 * A copy of an appropriate WAD file (search for DOOM1.WAD for demo game) if
   you want to mess with an original (uncompressed) file, otherwise the
   DOOM1.WAD supplied here is the standard shareware offerring with assets
   compressed through gzip.

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
% make install
<..or..>
% micropython/tools/pyboard.py --no-soft-reset -f mkdir /apps/Doom
% micropython/tools/pyboard.py --no-soft-reset -f cp __init__.py :/apps/Doom/
% micropython/tools/pyboard.py --no-soft-reset -f cp doomloader.mpy :/apps/Doom/
% micropython/tools/pyboard.py --no-soft-reset -f cp build/doom.bin :/apps/Doom/
% micropython/tools/pyboard.py --no-soft-reset -f cp DOOM1.WAD :/apps/Doom/
```

Debugging is by resetting the badge, then connecting your favourite serial
terminal program (I like `minicom`) to the TiDAL badge REPL, before selecting
`Doom` app on the badge screen...

## What's all the other stuff?

`phlashboot` is a test program to ensure I can build/install/run a very simple
program on a bare metal device, and/or as an application in an OTA slot.

`memstuff` is my investigation of the workings of ESP32S3 memory mapping/caching
to make very sure I understand it, and see how it's used by micropython, so I can
do vile things without causing harm...

 * `romread` and supporting bits is a `.mpy` module for poking about in memory..
 * `binsegments` is a trivial display program for ESP-IDF format `.bin` files that
   are loaded from bootstrap or an OTA partition.
 * other files are notes and logs (especially useful: uPython.map)

## How's it going?

After a month of effort on and off, it seems to work..

There is now a [progress blog](https://github.com/phlash/doom-tidal/discussions/2)
