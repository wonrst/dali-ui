# Optional 11x11 gate — NO GPU candidate

This decision is made after offline optimization only. No 11x11 shader was
installed in the viewer, and no 11x11 framebuffer was captured.

The 9x9 failure is consistent with sparse source coverage: the selected R24
kernel still leaves >2-source-pixel gaps after applying strength=.896, has
large phase-dependent stop-band peaks, and shows structured modulation despite
near-unit captured energy. This justifies the offline 11-tap investigation;
it is not a proof that every possible nine-tap arrangement must fail.

11 uses the same constraints, objective coefficients and strength/phase grids.
It improves the selected R24 9-tap kernel's raw stop RMS by approximately
23–28% at strengths .896/.8/.65/.5. However, it is not sufficiently close to
CURRENT across that interval:

| R24 strength | CURRENT peak / RMS | optimized9 peak / RMS | offline11 peak / RMS |
|---|---:|---:|---:|
| .896 | .281 / .096 | .544 / .221 | .443 / .166 |
| .8 | .168 / .072 | .435 / .206 | .301 / .150 |
| .65 | .066 / .034 | .414 / .171 | .260 / .124 |
| .5 | .018 / .009 | .337 / .124 | .249 / .095 |

These are dense 16-phase metrics, not GPU quality scores. Reconstruction does
not remove the remaining discrepancy: at strength=.5 the reconstructed RMS is
.0761 versus CURRENT .00693, while the unchanged Late Smooth mix is still 1.
At .896, integrated stop energy remains about 3x CURRENT; at .5 about 124x.
Low-band error is improved only modestly. Thus the requested condition
"substantially close to CURRENT" is not met for the primary Strong case.

Soft16's offline response improves more, but costs 51.25% more filtering
expressions than CURRENT. Strong24 costs +0.8333%, which alone is not an
impossible budget; budget is not the sole reason to reject it. The combined
quality-evidence and cost gate is insufficient to justify another GPU probe.

No arbitrary visual acceptance threshold is claimed. This is a conservative
engineering No-Go under the requested gate, NOT an observed 11x11 visual FAIL,
NOT a global optimum, and NOT a proof against all two-stage architectures.
Stop here: no 11 GPU, no 13+, no runtime H removal, no performance/target work.
