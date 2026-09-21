# Scope and measurement boundaries

- External native DALi app, production libraries unchanged.
- CURRENT and unchanged A-R installer only; no HIGH/B1/A/Y-half comparison.
- Korean24, 12 original corpus lines, 1920x1080 standalone Label at (60,60);
  same 1920x1080 window, black background, MSAA=4.
- Unit LINE, WHOLE_TEXT, Fade0, Stagger .25, BlurRadius48,
  BlurDurationRatio1, public PERFORMANCE. Fade0 excludes HEAD's adaptive recipe.
- No HUD in the benchmark so Output timing contains only the Label composition.
- Ordinary text scene first settles 2.5 seconds; Reveal setup then 1.2 seconds,
  unchanged A-R installer if selected, then another 2.5 seconds.
- Setup CPU logged over the last 3.7-second preparation/settling interval.
  It includes repeated forced render frames, so it is NOT pure creation time.
- A-R installer call has separate wall/process-CPU measurements; these do not
  include deferred GL compile/allocation or imply production first-frame cost.

## GPU

- One excluded warm-up process per CURRENT/A-R, then three independent processes
  each, interleaved CURRENT/AR/CURRENT/AR/CURRENT/AR.
- Each process measures fixed p=.20 for about 3 seconds then one 8-second
  0->1->0 linear animation (4 seconds each direction). No progress matrix.
- Public Animation/constraints drive the native renderer. Progress ranges logged.
- EXT_disjoint_timer_query surrounds each actual GL draw call.
- Classification by shader uniforms and source-texture producer; native task
  dimensions and draw counts are logged.
- Measurement starts/stops on swap frame boundaries. Stop drains all measured
  pending queries asynchronously before printing; no glFinish/readback.
- Report sums stage draw times divided by the same measured frame count.
- Source + optional Prefilter + H + V + Output included.
- Clear, upload, MSAA resolve, present, inter-pass waits and CPU are excluded.
  Thus this is total DRAW GPU time, NOT total frame time or predicted target FPS.
- GPU hook overhead is not used to judge CPU/RSS.

## CPU / RSS

- Three independent animation runs per candidate, same interleaved order,
  without LD_PRELOAD, GPU queries, native readback or detailed profiler.
- Process CPU uses CLOCK_PROCESS_CPUTIME_ID during the ~8-second animation.
- Report CPU ms / elapsed wall second, plus per-second bins.
  All app/update/render/driver threads in this process are included.
- VmRSS/VmHWM from /proc/self/status at ordinary/prepared/end-animation
  checkpoints. Prepared checkpoint follows 2.5 seconds of settling.
- RSS includes user-space driver/allocator/shader resources; NOT GPU VRAM.
- Native GPU hook separately confirms active FBO dimensions and whether the old
  production H remains alive after external substitution.
- Logical payload is width*height*bytes-per-pixel for active color attachments.
  Persistent old H, original text/metadata, window MSAA, driver alignment and
  allocator overhead are excluded from that logical topology table.
- No leak/stress/teardown-cycle test; no claim of leak absence.

## Conditional RGBA

- Only after primary GPU gain is clearly >=20–30%, one CURRENT/A-R pair using the
  preserved Korean32 red/green/blue gradient fixture.
- Only 8-second animation total draw GPU sanity; no RGBA CPU/RSS matrix.

No production build/change, quality tuning, target deployment, full regression,
sanitizer, commit or push. Previous reports and diagnostic files are untouched.
