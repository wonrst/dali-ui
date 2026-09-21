# Source-level decision, before disposable implementation

FEASIBLE FOR SIMPLE A8 FAST PATH, narrowly and conditionally.

Both H and Output can evaluate a virtual Source pixel grid from already-uploaded
L8 coverage and RGBA8 Reveal metadata. The Gaussian kernel need not change.
PER_LINE must use the existing isolated CPU-raster source atlas, not the whole
Label bitmap, because neighboring lines can overlap. One shared source TextureSet
per page is required for the first diagnostic; other pages retain capture.

This is NOT equivalent to multiplying two filtered samples. Reconstruct four
Source-grid pixel centers, evaluate coverage/reveal/alpha there, round to the
8-bit render target representation, then bilinearly interpolate. H can need
eight texture instructions instead of one per old Source read. The same function
is required by the sharp Output. This is the principal cost/complexity risk.

The first external copy supports fixed axis-aligned layout, scale 1, no image,
no style planes, L8 foreground plus one metadata texture. Existing line bounds,
halo, D2, H/V kernels, progress constraints, output order and page policy stay.
Animation of the owner position/opacity is separate from the capture transform.
Dynamic foreground transform support is not claimed by this diagnostic.

Source actor/FBO/task/camera creation must be skipped before allocation, not
disabled afterwards. The hidden foreground actor remains for constraint and
restore ownership. Unsupported pages and HIGH keep the original code.

Quality is the first gate. A failed pixel-semantic attempt is not a reason to
expand into a duplicate text pipeline; stop and report the remaining issue.
