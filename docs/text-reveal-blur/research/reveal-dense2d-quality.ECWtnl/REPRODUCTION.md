# PC quality comparison

```bash
# Default CURRENT | DENSE9, Soft16, looping 8s forward/reverse
bash /home/bowonryuubuntu/tizen/reveal-dense2d-quality.ECWtnl/run-viewer.sh

# Start directly at Strong24
RADIUS=24 bash /home/bowonryuubuntu/tizen/reveal-dense2d-quality.ECWtnl/run-viewer.sh
```

`1/2/3`: HIGH / CURRENT / CANDIDATE single view.
`4/5`: CURRENT|CANDIDATE / HIGH|CANDIDATE split view.
`S/D`: Soft16 / Strong24.
`Q/W/E/R`: p=.20/.50/.75/.90 static.
`Space`: 8s 0→1→0 loop. `Esc`: quit.

The split view clips each full-size Label to its left 960px. It does not shrink
the font or layout. Default window is 1920×1080; content is the same dense
Korean24 corpus used in previous experiments.

The script uses **private** existing optimized baseline libraries under
`../reveal-flush-poc.2JyKvS/lib/production` and `lib/common`.
It does not use the original tree's Source-only build, the rejected flush
candidate Adaptor, or any GBS/target library. It does not install anything.

## Rebuild this viewer only

```bash
cd /home/bowonryuubuntu/tizen/reveal-dense2d-quality.ECWtnl
python3 extract-kernel.py
PYTHONPATH=deps python3 kernel.py
bash build.sh
```

`deps` contains only the local pinned NumPy wheel for offline analysis. System
Pillow handles the captures. The viewer needs neither dependency at runtime.

## Existing evidence

- `captures/r{16,24}-{high,current,dense}/step*.png`: 48 native captures.
- steps 0/1/2/3: forward p=.20/.50/.75/.90.
- steps 4/5/6/7: reverse p=.90/.75/.50/.20.
- `slow-r16.mkv`, `slow-r24.mkv`: CURRENT left / CANDIDATE right.
- `comparisons/*-3x.png`: HIGH / CURRENT / CANDIDATE top-to-bottom.
- `METRICS-*.json`: text coverage energy/centroid/moments/bounds and reverse check.
- `KERNEL.json`, `PRODUCTION_KERNEL.json`: actual production arithmetic and
  derived candidate constants.
- `THEORY.json`: texture-expression arithmetic, logical payload, phase/frequency
  model. Not CPU/GPU performance results.

All logs newly printed by this viewer use `RYU - `.
The screenshot hook reused from the prior diagnostic only requests captures;
no `RYU_FLUSH_STATS` output or timer-query performance measurement is enabled.

The quality harness intentionally keeps the H resources alive. Do not benchmark
it as an actual two-stage implementation. Quality failed before that phase.
