# Reveal blur rebase — 2026-09-17

- Branch: `devel_blur_text`
- New base: `upstream/devel` at `3bbd3985`
- Old HEAD preserved at `backup/devel-blur-text-before-origin-devel-20260917` (`96bf7828`).
- `origin/devel` was older than the previous base. The user confirmed using the latest upstream instead.

## Final stack

1. `69030aed` Add custom Gaussian blur shader factory
2. `3c856fe6` Add blur to text reveal
3. `d95cc4a9` Add samples for testing text reveal blur
4. `b54bb666` Batch text reveal blur sources
5. `38d7611b` Optimize performance reveal blur filtering

All five retain Bowon Ryu's Signed-off-by. No push.

## Compatibility resolutions

- Preserve upstream raster coordinate validation and zero-sized metadata initialization guard.
- Preserve Reveal's metadata band/full-layout clipping; widen clipping bounds before negation/addition.
- Keep the sample blur controls, presets and transitions while retaining devel Application includes.
- Add the moved Application include to the newly introduced performance sample.
- Adapt one UTC padding call to the current float/Insets API without changing its values.
- Fold compatibility changes into the corresponding original commits; no extra fixup commits remain.

## Verification

- Full foundation/components build passed.
- text-effect-demo.example, text-reveal.example, text-reveal-perf.example and blur-strength-animation.example built.
- Fresh internal UTC executable: **183/183 passed**.
  Includes runtime blur, async publication, internal Reveal, gradient/raster safety and async marquee coordinates.
- `git diff upstream/devel --check` passed; UI working tree clean.
- Core and adaptor source trees unchanged and clean.
- No new performance measurements, GUI quality runs or full regression suite.

The first sample build used installed headers without Blur API and therefore failed.
After building UI, an install requested with a temporary prefix used the project's absolute install paths:
`/home/bowonryuubuntu/dali-env/opt`.
The local UI libraries/headers were updated to this branch; core/adaptor were not installed or changed.
All four subsequent sample builds passed against the updated UI installation.

See UTC results (로컬 자료: `utc-summary.tsv`), final range-diff (로컬 자료: `range-diff-final.txt`),
foundation/components build (로컬 자료: `build.log`), final UTC build (로컬 자료: `utc-build-final.log`),
sample build (로컬 자료: `sample-build-final.log`), performance sample build (로컬 자료: `perf-sample-build.log`),
BlurEffect sample build (로컬 자료: `blur-effect-build.log`), and UI install log (로컬 자료: `stage-install.log`).
