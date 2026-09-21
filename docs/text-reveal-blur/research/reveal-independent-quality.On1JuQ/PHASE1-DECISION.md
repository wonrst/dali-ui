# Phase 1 gate: STOP

The bounded independent-strength 9x9 search does not close the primary
Strong24 .65/.50 gap. No adaptive interpolation or GPU viewer will be built.

| Strength | CURRENT peak / reconstructed RMS | fixed9 | independent9 |
|---|---:|---:|---:|
| .896 | .28095 / .07958 | .54408 / .17872 | .46405 / .15676 |
| .65 | .06624 / .02971 | .41400 / .14187 | .29024 / .11927 |
| .50 | .01841 / .00693 | .33705 / .09875 | .22888 / .09302 |

All metrics use 2049 frequencies and 16 phases. At these strengths Late Smooth
blur contribution is 1. These are theoretical signal metrics, not GPU captures.

Independent freedom helps but is insufficient: selected reconstructed RMS is
still about 4x CURRENT at .65 and 13.4x at .5. This is not just weighted-score
selection: across all 32 solutions per case, the best reconstructed RMS is
.11582 at .65 and .07713 at .5, still about 3.9x and 11.1x CURRENT respectively.
These minima come from different tradeoffs and are not a single better kernel.

There were 16 deterministic starts under each of two fixed objective families
per case (320 solves total); 319 converged, all 320 feasible, all selected
solutions converged. One unsuccessful solve hit the iteration bound and was
not selected. CURRENT reference and candidate absolute-offset interpretation
also passed 144 independent sine gather/reconstruction checks, including odd
dimensions. No accidental second strength multiplication was found.

Practical verdict label: 81-READ BUDGET FUNDAMENTALLY INSUFFICIENT.
Scope: the requested symmetric nonnegative tensor-product 9-position axis
family, fixed Source halo and phase/strength fidelity in this bounded search.
This is evidence against proceeding, NOT a mathematical impossibility proof,
NOT a guaranteed global optimum, and NOT a GPU-observed adaptive quality FAIL.
Some small-strength cases improve substantially, so fixed scaling is a genuine
restriction, just not a sufficient explanation of the primary failure.

Stop rule honored: no Phase 2/3, no shader, no new screenshot/video, no runtime
H removal, no benchmark, no 11x11 escalation, no target or original-repo edits.
