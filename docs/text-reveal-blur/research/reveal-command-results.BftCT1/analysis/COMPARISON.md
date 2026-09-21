# Detailed target attribution comparison

CPU totals, not GPU time. Same settings / instrumentation OFF sanity must be checked separately.

## steady-entrance

Production 4 phases/215 frames; Source-only 4 phases/466 frames.

Command CPU: exclusive, additive including Other. Count=handler block invocations, not raw commands/draw calls.

| Category | Production count/frame | CPU ms/frame | Source-only count/frame | CPU ms/frame | CPU delta P−S |
|---|---:|---:|---:|---:|---:|
| RYU - Cmd.RenderPass | 102.42 | 4.884 | 34.03 | 1.599 | +3.285 |
| RYU - Cmd.DependencySync | 171.70 | 8.738 | 69.06 | 2.736 | +6.002 |
| RYU - Cmd.PipelineState | 102.42 | 0.678 | 34.03 | 0.219 | +0.459 |
| RYU - Cmd.BufferUniform | 102.42 | 1.007 | 34.03 | 0.313 | +0.694 |
| RYU - Cmd.TextureSampler | 51.21 | 0.889 | 17.02 | 0.387 | +0.502 |
| RYU - Cmd.Draw | 51.21 | 3.022 | 17.02 | 0.826 | +2.196 |
| RYU - Cmd.Other | n/a | 3.056 | n/a | 1.010 | +2.046 |

BufferUniform outside command CPU/frame: 0.178 / 0.150 ms. Not added to the command table.

## steady-exit-tail

Production 4 phases/24 frames; Source-only 4 phases/42 frames.

Command CPU: exclusive, additive including Other. Count=handler block invocations, not raw commands/draw calls.

| Category | Production count/frame | CPU ms/frame | Source-only count/frame | CPU ms/frame | CPU delta P−S |
|---|---:|---:|---:|---:|---:|
| RYU - Cmd.RenderPass | 114.00 | 5.233 | 38.00 | 1.769 | +3.464 |
| RYU - Cmd.DependencySync | 192.00 | 9.613 | 78.00 | 3.255 | +6.358 |
| RYU - Cmd.PipelineState | 114.00 | 0.755 | 38.00 | 0.233 | +0.522 |
| RYU - Cmd.BufferUniform | 114.00 | 0.976 | 38.00 | 0.320 | +0.656 |
| RYU - Cmd.TextureSampler | 57.00 | 0.997 | 19.00 | 0.440 | +0.557 |
| RYU - Cmd.Draw | 57.00 | 2.766 | 19.00 | 0.945 | +1.821 |
| RYU - Cmd.Other | n/a | 3.367 | n/a | 1.127 | +2.241 |

BufferUniform outside command CPU/frame: 0.180 / 0.159 ms. Not added to the command table.

## exit-setup-spike

Production 4 phases/4 frames; Source-only 4 phases/4 frames.

Create CPU: inclusive, additive between these queues only. Count=resource initialization attempts. Max in JSON is queue batch, not individual resource.

| Category | Production count/frame | CPU ms/frame | Source-only count/frame | CPU ms/frame | CPU delta P−S |
|---|---:|---:|---:|---:|---:|
| RYU - Create.Texture | 97.00 | 2.213 | 59.00 | 1.098 | +1.115 |
| RYU - Create.Buffer | 0.25 | 0.006 | 0.00 | 0.000 | +0.006 |
| RYU - Create.Framebuffer | 57.00 | 44.355 | 19.00 | 10.182 | +34.173 |

BufferUniform outside command CPU/frame: 0.255 / 0.137 ms. Not added to the command table.
