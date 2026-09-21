# Text::Reveal Blur — Production Cleanup
2026-09-14 · devel_blur_text · HEAD 70fe035551deed33fed02078c9354edfcb06adc2

## A. Production direction

- PERFORMANCE: axis-aware quarter, full-resolution sharp Source, Late Smooth, output batching, exact Gaussian.
- HIGH: existing exact full-resolution Gaussian. No kernel, timing, capture, FBO, D2, refresh-rate, or lifecycle policy change.

## B. Removed experiments

Removed reduced kernel generation and its private shader/cache, KernelPair, BuildReducedKernel, GetReducedShader, pairBudget, private shader macros, tap-budget and output-batching environment parsing, unbatched PERFORMANCE PER_LINE output variant, and two experiment-only UTCs.
The exact renderer factories, fragment shader, and runtime settings header are now identical to HEAD.
Production/test/sample searches found no listed experiment symbols or switches. Unrelated GL_GLEXT_PROTOTYPES build definitions were left alone.

## C. Output batching cleanup

PERFORMANCE PER_LINE always batches compatible, contiguous presentation ranges. One ordinary six-line page has H=1, V=1, Output=1 renderer; Source/H/V tasks remain three per page.
The output retains H/V draw-local strength indices, including nonzero first slots after mixed A8/RGBA/ImageSpan splits. Page, texture, format, 64-line draw, and presentation-order boundaries are preserved.
CPU BlurStrength calculation and constraints remain the timing authority. No GPU timing equation was introduced.

CreateBlurOutput derives batchStrength from quarterSource && batched.
Its shader caches contain only the needed variants: HIGH scalar A8 and batched A8/RGBA, PERFORMANCE scalar and batched A8/RGBA. HIGH scalar RGBA still uses the unchanged plain output factory.
PERFORMANCE WHOLE_TEXT scalar strength is preserved; only the PER_LINE unbatched experiment was removed.

## D. Public API documentation

Simplified class, quality, radius, and duration descriptions. Retained normalized PIXEL progression, final-visible LINE/PER_LINE semantics, atomic/spatial ImageSpan behavior, visible ellipsis participation, authored AUTO getter behavior, ranges/defaults, fallbacks, clipping, cost, and resource retention warnings.
Removed minimum/even kernel rounding, pass topology, fixed-kernel and render-scaling implementation details from public documentation.
ImageSpan edge antialiasing remains a short observable limitation, including at progress 1, without exposing capture implementation details.
Public header non-comment tokens are identical to HEAD; reveal.cpp and API/ABI/defaults are unchanged.

## E. Validation actually performed

- Foundation/components build once, install, and affected UTC build: PASS. No compiler warnings/errors in these build logs.
- Internal UtcDaliTextReveal prefix, exact configuration, once: 119/119 PASS.
- Public UtcDaliLabelTextRevealBlurValueP, once: 1/1 PASS.
- Preserved B library versus cleaned library, existing native capture harness: 76/76 captures byte-identical, including dimensions and all RGBA bytes.
- Capture fixtures: A8/r40/6 lines/stagger .25; RGBA gradient/r40; mixed ImageSpan; 65-line boundary. Progress 0 -> .4 -> 1 -> .4. Source, H, V and window output compared.
- Performance smoke: A8 twice each for CPU and GPU; optional RGBA once each. Runs were sequential, after build/UTC/capture completion.
- git diff --check: PASS.
- Full regression/sanitizer intentionally not rerun at this cleanup stage.

Logs: build.log, install.log, utc-build.log, internal-utc.log, public-utc.log, parity.log, performance.log.
XML results and individual captures/logs are in this directory.

## F. Performance sanity

Fixture: 8 Labels, each 550x300, six Latin lines, PIXEL/PER_LINE, fade0, stagger .25, radius40, blur-time .5, progress .5.
Host/build configuration unchanged: i7-11700 / GTX1650 / NVIDIA595.91.07 / X11 GLES / MSAA4, window680x440, existing foundation compiler flags.
Each run performs first creation plus two warm recreations, then .5s settling and approximately 2s measurement. CPU/GPU runs are separate.
B values reuse the previous three-run logs in ../reveal-perf-opt.upcgEj; they were not remeasured.

| A8 / eight Labels | Prior B | Cleaned |
|---|---:|---:|
| Process CPU ms/s | 267.290 | 267.292 |
| CPU run range, ms/s | 242.853–290.089 | 243.068–291.517 |
| GPU Source ms/frame | .244641 | .241745 |
| GPU H ms/frame | .127050 | .122831 |
| GPU V ms/frame | .117147 | .115893 |
| GPU Output ms/frame | .150477 | .151184 |
| GPU draw sum ms/frame | .639315 | .631654 |
| Output draws/frame | 8 | 8 |
| Additional RenderTasks | 24 | 24 |
| Actors / renderers / geometries | 88 / 80 / 25 | 88 / 80 / 25 |
| Unique FBO color-attachment bytes | 3,038,728 | 3,038,728 |
| All unique texture payload bytes | 10,497,088 | 10,497,088 |

Per Label, Source/H/V dimensions remain 434x666 / 109x666 / 109x167 in A8.
FBO payload is 2.898 MiB for all eight Labels; total texture payload is 10.011 MiB.
These are deduplicated width*height*format payloads, not physical driver VRAM allocations.

Optional RGBA: CPU 289.811 -> 302.941 ms/s; GPU draw sum .723867 -> .700359 ms/frame. One cleaned sample is not a trend; CPU is within the prior three-run range 270.258–327.411.
RGBA FBO bytes remain 12,154,912 and total texture bytes 19,625,560, with the same dimensions at four bytes/pixel.
All smoke runs released their additional RenderTasks back to zero.

CPU is CLOCK_PROCESS_CPUTIME_ID across the process, not an individual update/render thread.
GPU is asynchronous elapsed-query draw time, excluding clear/upload/swap/compositor, not total frame latency. Clock/host load were not fixed; query-window boundaries can partially include an offscreen frame. Dropped/disjoint queries were zero.
No meaningful performance improvement is claimed from cleanup; batching and resource dimensions were preserved, with no large regression observed.

## G. Remaining gates

TARGET VERIFICATION REQUIRED.
Full integration regression and sanitizer belong to the final integration/release gate; this scoped cleanup is not a new full-platform certification.
No NVIDIA-specific workaround or adaptor change was made.

## H. Working tree

HEAD unchanged. Changes unstaged. No commit, amend, staging, reset, restore, checkout, stash, or rebase.
Final UI diff is confined to runtime output batching, its production UTCs, and public reveal.h comments.
Existing adaptor working change and repository-external prior artifacts were preserved.

## I. Verdict

PRODUCTION CLEANUP READY.
