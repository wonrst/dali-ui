# Memory admission audit / selected estimate

The old 256 MiB check combines nominal CPU planes (12 bytes per raster pixel)
and three full RGBA runtime targets (12 bytes per target pixel). PER_LINE
multiplies both by the visible sequence count. It predates cropped line payloads,
page packing, scratch sharing, and PERFORMANCE reduced H/V. It is not a hard
process/GPU memory quota, nor an exact peak estimator.

It runs in the shared PrepareRevealBlur path after final sequences and visible
image placements are selected, before line bitmap bounds, line foreground/mask/
metadata raster, crop allocations and the normalized whole metadata plane.
The original ordinary text publication/planes already exist. Runtime FBOs and
uploads follow on the event thread only after complete publication validation.
Sync can overlap original/current textures and the candidate; async additionally
retains ordinary and prepared worker payloads, in-flight requests and the previous
publication. Queue concurrency and total active Label count were never covered.

Candidates:

1. O(1) full area + N*halo strips: cheapest, but compressed line heights and
   overlapping large-font lines can have much more retained coverage. Reject.
2. Metric bands in O(visible sequences), full-width allowance, one full-raster
   transient allowance and three RGBA page targets with 25% occupancy slack:
   selected. Image bounds are accumulated during the existing placement visit,
   without an extra traversal. No new raster or packing pass; no estimator allocation.
3. Full per-format/per-quality/per-page accounting: would duplicate the graphics
   planner and require actual coverage. Reject as unnecessary complexity.

Selected PER_LINE estimate, all arithmetic in double:

- R = ceil(raster width) * ceil(raster height).
- Halo = 2*(radius+2); T = (ceil(control width)+Halo)*(ceil(control height)+Halo).
- Each text band's h = min(raster height, 2*ceil(abs(ascender)+abs(descender))+6).
  This deliberately generous metric allowance is not a glyph-bitmap bound.
  It handles compressed spacing using font metrics, not total layout/N.
- Retained line allowance: 12*raster width*sum(text-bearing h).
- Runtime page allowance: 15*(control width+Halo)*sum(min(target height,
  h*max(1,control height/raster height)+Halo+6)).
  15 = three RGBA targets * 1.25 page occupancy slack, with no scratch/quarter credit.
- Visible image coverage contributes a separate halo-expanded target band;
  overlap with text is deliberately overestimated without an index/packing table.
- Total = 12*R + retained line allowance + runtime allowance + (decorations ? 4*T : 0).
- WHOLE_TEXT retains 12*R + 12*T + optional 4*T.

The one-full-raster allowance covers transient full-width/full-height work and
ordinary/global planes conservatively; it does not multiply every such buffer by
N. This remains an inexpensive admission estimate, not an exact guarantee for
arbitrary font overhangs, remote image storage, GPU driver overhead, or concurrent
publications. Existing dimension/page/crop/allocation validation remains intact.

The threshold is unchanged at 256 MiB. The incremental estimate exits immediately
on exceeding it. Existing size checks bound dimensions before arithmetic; double
avoids uint32 radius addition / area-product wrap, and non-finite metrics fail.

Validation: old production rejects the 1500x850 / 15 visible lines / radius24
fixture. Old estimate 433.428 MiB; new 67.7454 MiB (actual raster1500x750).
Sync/async and HIGH/PERFORMANCE all publish after the change. Dedicated extreme
and invalid-input UTC rejects without any line-bounds/line-raster/metadata-raster
calls, verified with debugger function breakpoints. Threshold remains 256 MiB.
