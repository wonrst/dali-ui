# PC warm repetitions: detailed instrumentation

CPU ms/frame; each row distribution is over five independent process warm-cycle means, not a confidence interval. Quantiles: linear interpolation (NumPy default).

## steady-entrance

| Metric | Variant | median | p25 | p75 | p95 |
|---|---|---:|---:|---:|---:|
| iteration_cpu_ms | production | 7.370118 | 7.339355 | 7.429398 | 7.469927 |
| iteration_cpu_ms | source-only | 3.688208 | 3.683558 | 3.709084 | 4.017478 |
| update_cpu_ms | production | 0.999594 | 0.985866 | 1.020256 | 1.023717 |
| update_cpu_ms | source-only | 0.709536 | 0.698850 | 0.710230 | 0.734613 |
| offscreen_cpu_ms | production | 4.630314 | 4.612696 | 4.658758 | 4.659363 |
| offscreen_cpu_ms | source-only | 1.608048 | 1.602186 | 1.615718 | 1.897370 |
| offscreen_command_cpu_ms | production | 3.288885 | 3.270010 | 3.289731 | 3.341940 |
| offscreen_command_cpu_ms | source-only | 1.155332 | 1.142148 | 1.157306 | 1.407158 |
| RenderPass_cpu_ms | production | 1.033468 | 1.023901 | 1.039668 | 1.049476 |
| RenderPass_cpu_ms | source-only | 0.375491 | 0.372501 | 0.377428 | 0.436434 |
| DependencySync_cpu_ms | production | 1.258698 | 1.250341 | 1.264401 | 1.283437 |
| DependencySync_cpu_ms | source-only | 0.461752 | 0.456747 | 0.462878 | 0.604539 |
| PipelineState_cpu_ms | production | 0.124614 | 0.122654 | 0.124690 | 0.125571 |
| PipelineState_cpu_ms | source-only | 0.035129 | 0.034205 | 0.035379 | 0.040342 |
| BufferUniform_cpu_ms | production | 0.296155 | 0.294795 | 0.297711 | 0.301666 |
| BufferUniform_cpu_ms | source-only | 0.086495 | 0.085168 | 0.086691 | 0.099267 |
| TextureSampler_cpu_ms | production | 0.139701 | 0.139488 | 0.141214 | 0.144103 |
| TextureSampler_cpu_ms | source-only | 0.055799 | 0.055214 | 0.056185 | 0.065940 |
| Draw_cpu_ms | production | 0.175780 | 0.175749 | 0.178766 | 0.178820 |
| Draw_cpu_ms | source-only | 0.052137 | 0.052011 | 0.054020 | 0.060432 |
| Other_cpu_ms | production | 0.260228 | 0.258554 | 0.260410 | 0.263235 |
| Other_cpu_ms | source-only | 0.086455 | 0.086432 | 0.086948 | 0.100648 |

## steady-exit-tail

| Metric | Variant | median | p25 | p75 | p95 |
|---|---|---:|---:|---:|---:|
| iteration_cpu_ms | production | 7.596970 | 7.596508 | 7.668699 | 8.430939 |
| iteration_cpu_ms | source-only | 3.610799 | 3.490772 | 3.643753 | 3.659648 |
| update_cpu_ms | production | 0.810555 | 0.803194 | 0.851863 | 0.916421 |
| update_cpu_ms | source-only | 0.475607 | 0.462133 | 0.488216 | 0.503598 |
| offscreen_cpu_ms | production | 5.159538 | 5.089492 | 5.172234 | 5.469692 |
| offscreen_cpu_ms | source-only | 1.719268 | 1.699008 | 1.725267 | 1.751600 |
| offscreen_command_cpu_ms | production | 3.731226 | 3.637903 | 3.783894 | 4.036597 |
| offscreen_command_cpu_ms | source-only | 1.249018 | 1.224568 | 1.254591 | 1.292985 |
| RenderPass_cpu_ms | production | 1.185992 | 1.152389 | 1.210713 | 1.305832 |
| RenderPass_cpu_ms | source-only | 0.422045 | 0.421245 | 0.438476 | 0.450264 |
| DependencySync_cpu_ms | production | 1.487948 | 1.443273 | 1.498671 | 1.592645 |
| DependencySync_cpu_ms | source-only | 0.511458 | 0.508598 | 0.530647 | 0.532112 |
| PipelineState_cpu_ms | production | 0.140499 | 0.139172 | 0.144033 | 0.149569 |
| PipelineState_cpu_ms | source-only | 0.028470 | 0.028234 | 0.029688 | 0.030668 |
| BufferUniform_cpu_ms | production | 0.259745 | 0.259284 | 0.268842 | 0.279687 |
| BufferUniform_cpu_ms | source-only | 0.056739 | 0.054670 | 0.056926 | 0.059752 |
| TextureSampler_cpu_ms | production | 0.159714 | 0.158916 | 0.162007 | 0.175215 |
| TextureSampler_cpu_ms | source-only | 0.066890 | 0.066589 | 0.067958 | 0.070687 |
| Draw_cpu_ms | production | 0.198481 | 0.193165 | 0.201787 | 0.215356 |
| Draw_cpu_ms | source-only | 0.053106 | 0.051499 | 0.054974 | 0.055171 |
| Other_cpu_ms | production | 0.291333 | 0.290378 | 0.308564 | 0.320438 |
| Other_cpu_ms | source-only | 0.096522 | 0.094501 | 0.097167 | 0.102385 |

## exit-creation-frame

| Metric | Variant | median | p25 | p75 | p95 |
|---|---|---:|---:|---:|---:|
| iteration_cpu_ms | production | 14.437988 | 14.221928 | 14.468357 | 14.825234 |
| iteration_cpu_ms | source-only | 7.679756 | 7.640991 | 8.229421 | 8.371121 |
| create_queue_cpu_ms | production | 2.538095 | 2.532976 | 2.549367 | 2.622018 |
| create_queue_cpu_ms | source-only | 1.045172 | 1.037247 | 1.054074 | 1.084081 |
| Framebuffer_create_cpu_ms | production | 2.263647 | 2.251166 | 2.290305 | 2.356339 |
| Framebuffer_create_cpu_ms | source-only | 0.850041 | 0.845476 | 0.860295 | 0.904446 |
| Texture_create_cpu_ms | production | 0.266857 | 0.259535 | 0.270285 | 0.273267 |
| Texture_create_cpu_ms | source-only | 0.179614 | 0.169299 | 0.185632 | 0.189795 |
| Framebuffer_create_count | production | 57.000000 | 57.000000 | 57.000000 | 57.000000 |
| Framebuffer_create_count | source-only | 19.000000 | 19.000000 | 19.000000 | 19.000000 |

## exit-setup-spike

| Metric | Variant | median | p25 | p75 | p95 |
|---|---|---:|---:|---:|---:|
| iteration_cpu_ms | production | 7.591924 | 7.440942 | 7.820286 | 13.114448 |
| iteration_cpu_ms | source-only | 3.791158 | 3.753445 | 6.443702 | 8.013977 |
| create_queue_cpu_ms | production | 0.002457 | 0.002357 | 0.002470 | 2.112639 |
| create_queue_cpu_ms | source-only | 0.002318 | 0.002292 | 0.004128 | 0.874092 |
| Framebuffer_create_cpu_ms | production | 0.000000 | 0.000000 | 0.000000 | 1.898278 |
| Framebuffer_create_cpu_ms | source-only | 0.000000 | 0.000000 | 0.000000 | 0.732387 |
| Texture_create_cpu_ms | production | 0.000000 | 0.000000 | 0.000000 | 0.207628 |
| Texture_create_cpu_ms | source-only | 0.000000 | 0.000000 | 0.000000 | 0.134686 |
| Framebuffer_create_count | production | 0.000000 | 0.000000 | 0.000000 | 45.600000 |
| Framebuffer_create_count | source-only | 0.000000 | 0.000000 | 0.000000 | 15.200000 |

## Warm entrance counts

| Metric | PC production | PC source-only | TV production | TV source-only |
|---|---:|---:|---:|---:|
| RenderPass_count | 106.750000 | 35.583333 | 102.461909 | 34.026091 |
| DependencySync_count | 178.916667 | 72.166667 | 171.769849 | 69.052182 |
| Draw_count | 53.375000 | 17.791667 | 51.230955 | 17.013045 |
| BufferUniform_count | 106.750000 | 35.583333 | 102.461909 | 34.026091 |
| TextureSampler_count | 53.375000 | 17.791667 | 51.230955 | 17.013045 |
| PipelineState_count | 106.750000 | 35.583333 | 102.461909 | 34.026091 |
| Framebuffer_create_count | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
