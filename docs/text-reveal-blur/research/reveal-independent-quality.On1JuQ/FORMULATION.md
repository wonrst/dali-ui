# Phase 1 protocol — frozen before independent optimization

Purpose: separate fixed/scaled parameterization from 9-position axis freedom.
No shader, GPU capture, runtime or performance work before the phase gates.

Reuse frequency.py from reveal-frequency-quality.HryP4o read-only. Its packed
CURRENT extraction, per-phase LINEAR gather, quarter polyphase reconstruction
and Late Smooth are unchanged. Construct CURRENT at actual strength, then set
only the candidate gather scale to 1. Candidate offsets are absolute source
pixels and weights are independent for every radius/strength. No shared-base
offset constraint and no cross-strength weight coupling.

Strong24: .896, .65, .50, plus offline sanity 1, .8, .35, .25.
Soft16: .896, .65, .50.

Nine symmetric positions, four positive offsets and five masses (center plus
four symmetric pairs). Nonnegative masses sum to one, ordered offsets with
minimum .15px separation, maximum offset R+1. Conservative 1px LINEAR reach
fits the existing halo R+2. Finite values and zero centroid are checked.
Variance is only the previous soft fidelity term, not a hard match.

Training: 16 phases 0..15/16, 257 uniform frequencies 0..0.5 plus the previous
axis/diagonal probe frequencies. Validation: 2049 uniform frequencies, the same
16 phases. Quarter Nyquist is .125 for the unchanged exact 4:1 fixtures.
Odd-size mapping is checked with the previous independently verified gather
model; there is no claim of odd-size GPU validation.

Keep the previous six normalized objectives and two scalarizations unchanged:
balanced=(1,1,1,1,1,1), width=(2,1,1,1,4,2). Terms are pass-band CURRENT error,
reconstructed stop energy, worst phase/frequency reconstructed peak, phase
variation, relative variance error, and CURRENT-relative axis/diagonal error.
No visual coefficient feedback. SLSQP max220 iterations, 16 deterministic
starts per scalarization per case. Starts include fixed9, old9, compact/wide
regular supports, Gaussian quantiles and Hermite nodes. All are just initial
guesses; every offset and mass remains free inside the constraints.

Select a balanced-score minimum from the feasible Pareto set, but expose both
primary leakage metrics and the Pareto alternatives rather than treating the
scalar score as quality. Inspect raw stop peak, integrated/RMS energy,
reconstructed stop RMS, pass error, phase variation and variance against CURRENT,
old9 and fixed optimized9. Best feasible is a bounded multistart result, not a
proven global optimum or an impossibility proof.

Phase 1 STOP: if independent Strong .65/.50 still has a large residual gap from
CURRENT, do not create adaptive parameterization/viewer/runtime. Judge the gap
relative to CURRENT/fixed9 distributions, not a new arbitrary 0.05 threshold.
Only substantial closure across these strengths can justify Phase 2. Record
the gate before doing any Phase 2 work. No automatic 11x11 escalation.
