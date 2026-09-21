# Frozen before GPU inspection

Train the symmetric nonnegative 9-tap axis kernel; evaluate all 81 Cartesian
combinations in one GPU pass. Four increasing positive offsets and five masses
(center plus four symmetric pairs) are the optimization variables.
Masses are constrained to sum to one. Offset separation >=.15 source pixels;
maximum offset <=R+1, including <=1px LINEAR footprint stays within halo R+2.
There is no exact-variance constraint; variance is a soft fidelity objective.

Training strengths: 1, .8, .5, .25, .1. Phases: 0, .25, .5, .75.
The GL_LINEAR response is expanded to its two integer texel neighbors at each
phase/strength, not exp(i*omega*continuousOffset) point sampling.
CURRENT reference uses the previously extracted production packed offsets and
weights. A8 quantization is not modeled. In 1D, CURRENT's X and Y axes both
filter from full source lattice to the quarter lattice; H preserves Y and V
reads H at the same quarter-X center, so the same axis model applies. Cross-axis
quantization and 2D phase combinations remain GPU validation responsibilities.

Source dimensions are unchanged. Both original fixtures have exact 4:1 ratios,
giving source Nyquist .125 cycles/pixel. Use low band [0,.125), stop band
[.125,.5], no invented gap around the true folding boundary. CURRENT itself
determines the low-band desired response; no ideal Gaussian replacement.

Include the four output polyphases of quarter sampling + LINEAR reconstruction
in signal energy. This chain is periodically shift-variant, not one scalar LTI
transfer function. Also apply the unchanged Late Smooth blur contribution to
response errors/alias energy: at small strengths the output is already sharp
Source, so forcing the intermediate blur to become an unrelated strong lowpass
would be the wrong target. Raw metrics at every strength are still reported.

Six dimensionless components, fixed before visual testing:

1. Low-band complex error from CURRENT, reconstructed and handoff-weighted;
   normalize by gain RMS .05.
2. Reconstructed stop-band energy, handoff-weighted; normalize by RMS .10.
3. Worst frequency and phase stop-band power for each strength, handoff-weighted;
   average across strengths; normalize by peak .25.
4. Phase variance of complex response, reconstructed/handoff-weighted;
   normalize by RMS .10.
5. Mean relative effective variance error across all phases/strengths;
   normalize by .15.
6. Difference from CURRENT's diagonal-vs-axis response at radial frequencies
   .04/.08/.12, handoff-weighted; normalize by RMS .05.

These are engineering scalarizations, not a perceptual proof. Two predetermined
Pareto preferences only: balanced=(1,1,1,1,1,1), width=(2,1,1,1,4,2).
Five deterministic starting supports per preference/radius, max220 SLSQP
iterations. No visual feedback enters optimization; no global optimality claim.
Validate finalists on a denser phase/frequency grid and held-out strengths before
choosing at most two candidate families for GPU. Negative weights prohibited.

If optimized9 fails, only then assess 11 offline under these same objectives.
11 is not automatically allowed on GPU: compare its frequency gain and actual
121-read arithmetic first. No 13+, coefficient sweep, shader variant per strength,
prefilter/reconstruction modification, or runtime performance measurement here.

## Pre-GPU coverage correction

The five-strength pilot's held-out Strong24 strength=.65 had a max response
~.665 versus CURRENT ~.066. Before any new GPU capture, retain all pilot results
under `pilot/` and refine using a fixed .05 strength grid from 1.00 to .10.
Use the four distinct lowest balanced-score pilot starts per radius, with the
same two coefficient sets. No visual result or coefficient changes enter this
refinement. Selection is ONE radius-dependent optimized9 design: balanced-score
minimum of the Pareto set. Both radius constants are parameters of that design,
not two shader variants chosen dynamically by strength/phase.
