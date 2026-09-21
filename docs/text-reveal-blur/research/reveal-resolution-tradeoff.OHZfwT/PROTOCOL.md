# Frozen offline protocol

Compare A (third target, seven positions/axis) first, then B (half target,
five positions/axis). Source remains FHD plus R+2 halo; no production changes.
R16/R24 at actual strength .896/.65/.50, each independently optimized.

Use the previous packed production extraction, LINEAR gather and Late Smooth.
Do NOT use the previous quarter reconstruction weights for A/B. Compute
target=ceil(source/divisor), d=source/target, target centers=(j+.5)*d-.5,
and output lookup=(n+.5)/d-.5. Reconstruct the two neighboring target samples
including their different source phases and complex phase shifts. Compare
CURRENT quarter and each candidate at identical full-source output positions.
Raw gather still covers uniform phases 0..15/16. Exact mapping is independently
checked with sampled sine sources, including odd extents, before optimization.

Train on both width and height of the actual fixture; 12 consecutive interior
output points plus 24 points across the extent cover output polyphases and
non-integer phase progression. Validation uses all interior output pixels and
actual target phases. Frequency training: 257 uniform 0...0.5 plus exact nominal
boundaries; final validation: 2049 frequencies including actual Nyquist values.

Candidate own stop-band is f >= target/(2*source), separately for each axis.
Compare CURRENT and prior independent quarter9 on that SAME band; do not divide
candidate-own leakage by a differently integrated quarter-band number.
Also report each path's own band and final reconstructed output fidelity.

Absolute independent positive offsets and center/pair masses. Symmetric,
nonnegative, normalized, finite, centroid zero; minimum gap .15px and maximum
offset R+1 ensure 1px LINEAR reach remains inside existing halo R+2.
Variance is a soft CURRENT-relative penalty, never a hard equality.

Six objective terms, fixed before search:
1 final reconstructed pass-band complex error vs CURRENT (own f<Nyquist),
2 final reconstructed own-stop energy,
3 raw worst phase/frequency own-stop peak,
4 raw phase variation,
5 relative filter variance error,
6 reconstructed transition error vs CURRENT (own Nyquist +/- .05).
Normalizations: .05^2/.10^2/.25^2/.10^2/.15^2/.05^2 respectively.
Late Smooth blur contribution is included; it is 1 at the mandatory strengths.
Two fixed scalarizations: balanced=(1,1,1,1,1,1), width=(2,1,1,1,4,2).
This adapts the previous objective to different output lattices; no eye tuning.

16 deterministic initializations per scalarization/case; SLSQP, maximum220
iterations. Analytic piecewise LINEAR derivatives independently finite-difference
checked away from integer discontinuities. All offsets/masses remain free.
Select a balanced-score Pareto representative and separately inspect best
reconstructed-leakage and best worst-peak solutions across ALL starts.
No global optimum/impossibility claim. No new absolute PASS threshold.

GO only if Strong .65/.50 gaps close enough relative to CURRENT/quarter9 to
justify visual verification and Soft16 profile remains plausible. If both
candidates retain large leakage/fidelity gaps, STOP direct raster; no GPU
viewer, runtime removal, performance, target work, or further kernel tuning.
