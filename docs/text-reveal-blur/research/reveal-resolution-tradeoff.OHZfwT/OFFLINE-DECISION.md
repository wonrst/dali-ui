# Offline gate — STOP

Verdict: DIRECT RASTER TWO-STAGE APPROACH STOP.

A (third-resolution 7x7) and B (half-resolution 5x5) both fail the requested
offline feasibility gate. No GPU viewer or runtime candidate will be built.

The comparison integrates CURRENT and the unchanged independent quarter9
over EACH CANDIDATE'S OWN alias band, using exact ceil-sized targets and full
interior output reconstruction. We do not compare unequal-band RMS ratios.

Strong24 s=.65:

- A band: CURRENT .030849, quarter9 .108385, A .200143.
- B band: CURRENT .032551, quarter9 .096701, B .202492.
- Best reconstructed leakage across all starts: A .173308, B .152597.

Strong24 s=.50:

- A band: CURRENT .000308, quarter9 .105659, A .156270.
- B band: CURRENT .000226, quarter9 .100330, B .219564.
- Best reconstructed leakage across all starts: A .147330, B .149138.

These are theoretical signal amplitudes, not brightness or GPU performance.
CURRENT's tiny s=.50 tail is phase/lattice dependent; huge ratios to that
near-zero denominator are NOT used as the verdict. Absolute gaps, s=.65,
and worsening against quarter9 establish the lack of practical closure.

384 bounded deterministic solves all converged and were feasible. Primary
minima and width/fidelity tradeoffs were examined independently of the selected
weighted score. This is not a proof of global optimum or impossibility for all
direct filters. It is the requested practical STOP for these architectures.

No further quarter/third/half direct-raster tuning, adaptive kernel, GPU quality
capture, runtime H removal, benchmark, target work, or production modification.
