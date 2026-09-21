# A-R production audit / implementation checkpoint

## Git checkpoint

- Archived adaptive: `5aae0d2b389bb9811bed6e21b754cee629f4ecfc` at `prototype/reveal-adaptive-sampling`.
- Clean baseline: `devel_blur_text` at `b7b0b712`; tip-only adaptive removed; clean UI tree.
- Existing adaptor +13-line dependency-checker change preserved; no remote writes.
- Regenerated existing CMake build after removing the adaptive shader; baseline foundation build passed.
- Frozen CURRENT library: `current/`. Reference quality/feasibility directories remain untouched.

## Architecture

HIGH: Source -> H -> V -> Output. Compatible pages share Source/H, retain V.
CURRENT PERFORMANCE: full Source -> quarter-X/full-Y H -> quarter-XY V -> Output + sharp Source.
A-R PERFORMANCE: full Source -> full-X/quarter-Y Pref -> quarter-XY H -> quarter-XY V -> Output + sharp Source.

Each page completes Source/Pref/H/V before the next page; Pref/H can share only identical page dimensions and format. Source/V are retained separately. Existing source/output draw batching, 64-entry bindings, mixed-format ordering and image proxy capture stay intact. Decorations remain outside the foreground filter chain. Prepared CPU payload and async revision checks need no new fields. Detached construction precedes publication and borrowing; existing cancellation/restore guards must cover every new allocation.

## Sampling derivation

- Actual Y ratio r = H / ceil(H/4); actual X output = ceil(W/4).
- Prefilter output row j integrates the source pixel cells over [j*r,(j+1)*r]. Weight for cell k is overlap([k,k+1], footprint)/r. Three adjacent-cell pairs cover at most five cells. Each pair is one LINEAR fetch at k+0.5 + secondWeight/pairWeight; X remains 1:1.
- H retains the existing full-radius Gaussian and source-pixel X offsets. Its input Y clamp follows Pref height. Its coverage band must include Pref's footprint, not just original nonzero rows.
- V positive pairs = max(2, ceil((normalizedRadius/2)/r)); radius = 2*pairs. The minimum avoids single-element uniform-array reflection. Actual Gaussian bell widths come from the existing coefficient generator, via a numeric-only internal helper (no UBO exposure or kernel change).
- V strength = sqrt(max(0, sigmaOriginal^2 * strength^2 - preVariance)) / (sigmaLow*r). The aligned four-cell footprint has variance 1.25, matching the PoC. For odd heights, preVariance is the mean discrete central variance of the actual overlap weights across output rows (not a newly guessed Gaussian sigma). Phase-dependent footprints cannot be represented by one exact convolution; parity/quality must gate this generalization.
- V offset uses low input height; batch line-local offsets multiply by r. Original progress/start guards remain before Gaussian evaluation. Completed output still selects full sharp Source through unchanged Late Smooth thresholds 2/8.
- Output averages V at Y +/- 0.5/actualVHeight; batch reads clamp to the padded line rectangle. Sharp Source read and alpha/RGBA premultiplication stay unchanged.

## Implementation plan

Keep public API and HIGH shader variants unchanged. Extend private PERFORMANCE path with an optional Pref actor/FBO/task, numeric V calibration, and cached reduced-V shader variant. Reuse page-sized prefilter geometry, existing H/V draw geometry, source/output ownership and lifecycle. Add initial-allocation topology/odd-size/boundary/resource tests before native validation. No adaptive, idle gating or reconstruction tuning.
