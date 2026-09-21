# Step A — radius decision (before timing capture / performance)

Compared Soft floor10: max16/24/32 and Strong floor12: max24/32/40.
Balanced alpha parameters and native gamma1 remained fixed.

Select one max radius: **32**.

Native p=.1–.4 images show a broader, less readable blur layer than old A.
It does not remove the early-Y structured modulation, but makes blur more present.
At p=.5–.9 the unchanged floor/alpha trajectory still dominates; increasing max
radius alone does not solve the requested late-timeline presence.
40 is broader again, without a compelling extra benefit over32 for this corpus,
and increases halo/source footprint and kernel work further. Do not select40.

Next: freeze32, compare gamma1/1.5/2 with the same Balanced alphas.
No performance run was made for this radius decision.
