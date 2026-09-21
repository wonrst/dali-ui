# Tizen 10.1 ARMv7l Reveal attribution build

## Bases

- Core: `ecf444c414543b1e629f53afe1340beda2ee5f9c`
- Adaptor: `f451029c14a7bda7175254ffd3aa4c73bb8252ba`
- UI: `4d74bbf9d4169f808c0e11cc5e806d2e506f1d95`
- Production UI additions, in order: `69030aed`, `3c856fe6`, `d95cc4a9`, `b54bb666`.
- Source-only adds the existing `b54bb666..05087317` diagnostic delta, not a new rendering variant.

All sources are separate detached worktrees. Original development checkouts,
branches, commits and `.gbs.conf` are preserved. No commit or push is performed.
Use `--include-all`: instrumentation and ported changes are not committed.

## Port notes

`text-typesetter-impl.cpp` had two conflicts because the requested UI base
predates unrelated raster-coordinate hardening on devel:

1. Initialize the blur metadata's borrowed pixel pointer and pixel count.
2. Retain the base revision's line-coordinate calculation and skipped-line
   accumulation; add blur's reference-height clipping for cropped metadata.

No unrelated devel hardening commits are transplanted. Both comparison variants
use exactly this same resolution.

The sample RPM file list also includes `text-reveal-perf.example`, which the
sample patch already builds and installs. No sample behavior is changed.

The requested Core/Adaptor bases precede two public-to-devel API moves present
in the requested UI base. Target-copy compatibility adaptations:

- Builder's Extents include uses `public-api/common/extents.h`.
- Three builder/debug files use the existing `Property::EXTENTS` and
  `Property::Value::Get(Extents&)`, not the later devel constant/free getter.
- Text samples include the existing public `Application` header, not its later
  devel location. The application class and sample behavior are unchanged.

These adaptations are identical in both UI variants. Core/Adaptor APIs are not
backported or otherwise changed for compatibility.

## Instrumentation

The source instrumentation is from `../reveal-critical-path.CFOTks/trees/`.
Core also requires `collector.patch`, containing `reveal-attribution.inc`.
The same UI instrumentation is applied to production and Source-only.

Build-copy specs append `-DRYU_REVEAL_ATTRIBUTION` to existing CXXFLAGS;
library specs already enable trace. The sample additionally defines
`-DTRACE_ENABLED`. Optimization flags are otherwise unchanged.

The sample spec explicitly requires Core integration headers for its diagnostic
`trace.h` include and UI integration headers already used by its localization
sample. These are build-time dependencies only.

No GPU synchronization, algorithm, animation timing, or additional renderer
behavior is introduced by instrumentation. GPU duration remains unmeasured.

## GBS

All builds use the user's `tizen_10.1` profile and one isolated build root:

```bash
root=/home/bowonryuubuntu/tizen/reveal-trace-tizen101.prtWoU
cd "$root/core"
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot "$root/gbs-root" --release 1.ryutrace
cd "$root/adaptor"
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot "$root/gbs-root" --release 1.ryutrace
cd "$root/production"
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot "$root/gbs-root" --release 1.ryutrace.production
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot "$root/gbs-root" --release 1.ryutrace \
  --packaging-dir samples/text/packaging --spec com.samsung.dali.text.spec
cd "$root/source-only"
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot "$root/gbs-root" --release 1.ryutrace.sourceonly
```

Builds must run sequentially against this build root. Adaptor and UI consume
the locally built Core/Adaptor development packages. Build success, RPM
contents and actual selected dependencies are checked separately from the
commands above. Target execution is performed by the user, not claimed here.
