# Target / visual comparison

This is a standalone diagnostic, not a production-library switch. It requires
the matching Text::Reveal implementation (baseline UI commit 46d0ea18).
It only installs a private H/V shader after the ordinary public Reveal pipeline
has been created. Source, Output, tasks, textures and their owners are unchanged.

## Host visual comparison

```bash
bash /home/bowonryuubuntu/tizen/reveal-adaptive-study.8A0C8e/run-compare.sh a8
bash /home/bowonryuubuntu/tizen/reveal-adaptive-study.8A0C8e/run-compare.sh rgba
```

Left: production exact24. Right: adaptive candidate. The same Animation drives
both Labels over 8 seconds; the application exits after the comparison.

Videos: `compare-a8-ready.mp4`, `compare-rgba-ready.mp4`. These are visual aids,
not lossless pixel measurements. Lossless GL readbacks provide the quality metrics.

## Target build

Copy `visual.cpp`, `adaptive.inc`, `fhd-corpus.h`, and `run-target.sh` together.
Build `visual.cpp` using the same target compiler/SDK/link flags used for existing
DALi UI sample applications. Do not copy the host ELF executable to the TV.
There are no Linux profiling, preload, Python or filesystem APIs in this viewer.

For a native SDK with matching pkg-config files, the host-equivalent command is:

```bash
g++ -std=c++17 -O2 visual.cpp \
  $(pkg-config --cflags --libs dali2-ui-foundation dali2-adaptor) -o visual
```

For a cross SDK, replace g++ and pkg-config with that SDK's configured tools.
No repository CMake condition or installed production library needs to change.
Target compilation and FPS have not been tested here.

## Target execution

Use the same normal backend, MSAA and library environment for both runs:

```bash
bash run-target.sh exact a8
bash run-target.sh adaptive a8
bash run-target.sh exact rgba
bash run-target.sh adaptive rgba
```

Each process creates one 1920x1080 Label, font 24, fixed Korean corpus,
PIXEL / WHOLE_TEXT / Fade=1 / radius48 / BlurTime=1 / PERFORMANCE,
UI scale and render scale 1. It plays 20 one-second Linear reverse loops.
The PLAY log marks animation start. Ignore startup and final idle FPS samples.
Repeat in alternating order, with the same foreground apps and window state.

Use the existing target FPS capture method if DALI_FPS_TRACKING output is not
available in its build. Record whole-animation FPS **and** middle-region drops:
the candidate still uses exact24 at progress 0.2..0.6, so it is not expected to
remove that region's worst-case blur cost.

Do not turn this into a generic app setting: other radii, UI/render scales,
BlurDurationRatio, PER_LINE, async replacement and ImageSpan are outside this PoC.
The factory rejects a non-24-pair original shader or a mismatched public recipe.

Proceed with production design only after target FPS and visual review show a
worthwhile improvement. If the target gain is negligible, keep production exact.
