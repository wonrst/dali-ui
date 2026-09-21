# Source batching — final measurement tables

P=PERFORMANCE, H=HIGH. 변화율=(candidate/baseline−1)×100; 음수는 비용 감소. CPU와 GPU 각각 독립 process 3회, 12 cases × 2 variants × 2 measurement types × 3 = 144 runs. CPU는 비계측 process CPU ms/s, GPU는 별도 timer-query draw time ms/frame. progress=0.5 고정 + KeepRendering; 애니메이션 FPS 측정이 아니다.

## Primary CPU / GPU

| Case | CPU baseline mean [min,max] ms/s | CPU candidate mean [min,max] | CPU 변화 | GPU baseline → candidate ms/frame | GPU 변화 |
|---|---:|---:|---:|---:|---:|
| 500×350×8 / P / A8 | 258.2 [239.2, 277.9] | 206.0 [192.2, 221.4] | -20.2% | 0.640 → 0.433 | -32.4% |
| 500×350×8 / P / RGBA | 300.0 [288.2, 311.0] | 224.1 [202.2, 257.7] | -25.3% | 0.725 → 0.536 | -26.0% |
| 500×350×8 / H / A8 | 677.5 [660.0, 706.9] | 616.5 [565.6, 682.2] | -9.0% | 0.890 → 0.764 | -14.2% |
| 500×350×8 / H / RGBA | 666.9 [639.7, 717.2] | 500.2 [457.0, 558.8] | -25.0% | 1.076 → 0.909 | -15.5% |
| 500×500×8 / P / A8 | 346.5 [326.1, 360.5] | 228.6 [203.2, 270.3] | -34.0% | 1.120 → 0.676 | -39.7% |
| 500×500×8 / P / RGBA | 415.5 [412.2, 421.7] | 237.6 [206.1, 255.5] | -42.8% | 1.260 → 0.825 | -34.5% |
| 500×500×8 / H / A8 | 778.8 [772.2, 792.0] | 662.0 [541.6, 726.6] | -15.0% | 1.390 → 1.023 | -26.4% |
| 500×500×8 / H / RGBA | 696.6 [478.9, 814.5] | 703.2 [692.4, 713.8] | +1.0% | 1.650 → 1.306 | -20.8% |
| FHD×1 / P / A8 | 217.7 [185.9, 247.3] | 230.2 [221.9, 244.2] | +5.7% | 0.683 → 0.539 | -21.0% |
| FHD×1 / P / RGBA | 222.1 [204.7, 252.0] | 183.1 [169.5, 198.8] | -17.6% | 0.837 → 0.671 | -19.7% |
| FHD×1 / H / A8 | 213.3 [177.0, 237.1] | 254.3 [233.4, 265.7] | +19.2% | 0.947 → 0.841 | -11.2% |
| FHD×1 / H / RGBA | 230.1 [199.0, 247.1] | 217.0 [191.2, 238.0] | -5.7% | 1.206 → 1.012 | -16.1% |

## GPU stage detail

각 셀은 baseline → candidate, ms/frame. Total은 각 run의 stage 합 평균이다.

| Case | Source | H | V | Output |
|---|---:|---:|---:|---:|
| 500×350×8 / P / A8 | 0.274 → 0.068 | 0.109 → 0.098 | 0.100 → 0.103 | 0.156 → 0.163 |
| 500×350×8 / P / RGBA | 0.348 → 0.157 | 0.120 → 0.116 | 0.105 → 0.106 | 0.152 → 0.158 |
| 500×350×8 / H / A8 | 0.262 → 0.074 | 0.167 → 0.178 | 0.333 → 0.371 | 0.129 → 0.142 |
| 500×350×8 / H / RGBA | 0.344 → 0.159 | 0.219 → 0.217 | 0.373 → 0.384 | 0.140 → 0.149 |
| 500×500×8 / P / A8 | 0.602 → 0.128 | 0.128 → 0.126 | 0.110 → 0.117 | 0.280 → 0.305 |
| 500×500×8 / P / RGBA | 0.702 → 0.240 | 0.166 → 0.162 | 0.122 → 0.128 | 0.270 → 0.296 |
| 500×500×8 / H / A8 | 0.590 → 0.113 | 0.171 → 0.197 | 0.401 → 0.460 | 0.229 → 0.253 |
| 500×500×8 / H / RGBA | 0.694 → 0.227 | 0.245 → 0.272 | 0.469 → 0.536 | 0.242 → 0.271 |
| FHD×1 / P / A8 | 0.278 → 0.147 | 0.054 → 0.053 | 0.054 → 0.051 | 0.297 → 0.289 |
| FHD×1 / P / RGBA | 0.383 → 0.235 | 0.100 → 0.093 | 0.072 → 0.069 | 0.282 → 0.274 |
| FHD×1 / H / A8 | 0.256 → 0.117 | 0.125 → 0.132 | 0.320 → 0.334 | 0.245 → 0.258 |
| FHD×1 / H / RGBA | 0.377 → 0.213 | 0.178 → 0.170 | 0.385 → 0.367 | 0.266 → 0.262 |

## Draw count

Steady frame의 Source / H / V / Output 구조; 두 quality와 두 format에서 동일한 감소. Raw query 구간은 시작/종료가 프레임 경계와 정확히 맞지 않아 평균 count에 0.01~0.10 수준의 차이가 있으며, 아래는 UTC/renderer inventory와 일치하는 정수 draw 구조다.

| Workload | Baseline | Candidate | Total |
|---|---:|---:|---:|
| 350 | 48 / 8 / 8 / 8 | 8 / 8 / 8 / 8 | 72 → 32 |
| 500 | 112 / 8 / 8 / 8 | 8 / 8 / 8 / 8 | 136 → 32 |
| fhd | 31 / 6 / 6 / 6 | 6 / 6 / 6 / 6 | 49 → 24 |

## Setup

ordinary → blur setter 직전부터 모든 companion publication 확인까지. 3 CPU runs, polling/frame scheduling/기존 raster 포함. GPU upload 완료 latency 아님.

| Case | Wall ms baseline → candidate | Process CPU ms baseline → candidate |
|---|---:|---:|
| 500×350×8 / P / A8 | 88.35 → 75.94 | 85.06 → 73.54 |
| 500×350×8 / P / RGBA | 92.31 → 75.45 | 91.05 → 79.94 |
| 500×350×8 / H / A8 | 81.27 → 74.46 | 80.67 → 71.04 |
| 500×350×8 / H / RGBA | 91.41 → 81.07 | 86.79 → 79.43 |
| 500×500×8 / P / A8 | 155.84 → 134.98 | 155.93 → 133.16 |
| 500×500×8 / P / RGBA | 179.39 → 148.42 | 185.36 → 155.58 |
| 500×500×8 / H / A8 | 158.05 → 134.64 | 161.87 → 132.61 |
| 500×500×8 / H / RGBA | 174.49 → 147.38 | 183.15 → 149.07 |
| FHD×1 / P / A8 | 142.31 → 141.26 | 142.01 → 146.69 |
| FHD×1 / P / RGBA | 157.31 → 156.04 | 162.55 → 165.91 |
| FHD×1 / H / A8 | 141.36 → 138.46 | 147.70 → 150.81 |
| FHD×1 / H / RGBA | 157.13 → 154.18 | 162.58 → 163.47 |

## Atlas construction isolated cost

최종 production 준비 함수를 별도 실행 파일에 동일 O0로 링크. 실제 baseline line-plane dimensions를 사용하되 입력 buffer 생성/채움은 측정 밖이다. 각 case 한 process, 5 warmup 뒤 20회. atlas allocation/zero/copy/guard/isolated-plane release 포함; text raster 및 GPU upload 제외. Allocator/cache 상태에 민감하므로 크기 간 선형 비용 모델이나 cold-start 예측으로 쓰지 않는다.

| One Label | Format | CPU mean [min,max] ms | Wall mean ms |
|---|---|---:|---:|
| 350 | A8 | 0.112 [0.107, 0.149] | 0.112 |
| 350 | RGBA | 0.210 [0.194, 0.259] | 0.211 |
| 500 | A8 | 0.077 [0.065, 0.291] | 0.078 |
| 500 | RGBA | 0.518 [0.463, 0.738] | 0.519 |
| fhd | A8 | 1.071 [0.817, 3.111] | 1.072 |
| fhd | RGBA | 5.121 [4.921, 5.375] | 5.122 |

## Actual texture inventory

Unique handles를 세고 dimensions×bytes/pixel을 합산. MiB=2²⁰ bytes. FBO는 total의 부분집합. VRAM allocation/residency/driver overhead 아님. Ordinary foreground/global metadata/shared LUT 포함.

| Case | Total MiB baseline → candidate | 변화 | Atlas planes MiB candidate | FBO MiB (unchanged) | Textures baseline → candidate | RSS MiB baseline → candidate |
|---|---:|---:|---:|---:|---:|---:|
| 500×350×8 / P / A8 | 9.537 → 9.877 | +3.56% | 3.378 | 2.607 | 136 → 56 | 159.03 → 158.97 |
| 500×350×8 / P / RGBA | 22.915 → 23.526 | +2.67% | 6.081 | 10.430 | 200 → 80 | 184.17 → 182.25 |
| 500×350×8 / H / A8 | 12.884 → 13.224 | +2.64% | 3.378 | 5.955 | 136 → 56 | 157.68 → 158.82 |
| 500×350×8 / H / RGBA | 36.304 → 36.915 | +1.68% | 6.081 | 23.818 | 200 → 80 | 182.02 → 176.20 |
| 500×500×8 / P / A8 | 22.148 → 23.239 | +4.92% | 7.995 | 6.165 | 264 → 56 | 191.06 → 190.36 |
| 500×500×8 / P / RGBA | 53.442 → 55.405 | +3.67% | 14.391 | 24.661 | 392 → 80 | 221.20 → 219.65 |
| 500×500×8 / H / A8 | 30.057 → 31.147 | +3.63% | 7.995 | 14.073 | 264 → 56 | 190.70 → 183.83 |
| 500×500×8 / H / RGBA | 85.076 → 87.039 | +2.31% | 14.391 | 56.294 | 392 → 80 | 220.94 → 214.44 |
| FHD×1 / P / A8 | 23.445 → 24.139 | +2.96% | 8.514 | 5.975 | 77 → 17 | 174.54 → 176.04 |
| FHD×1 / P / RGBA | 55.348 → 56.597 | +2.26% | 15.326 | 23.900 | 110 → 20 | 220.43 → 201.95 |
| FHD×1 / H / A8 | 24.685 → 25.378 | +2.81% | 8.514 | 7.214 | 72 → 12 | 174.15 → 179.23 |
| FHD×1 / H / RGBA | 60.306 → 61.554 | +2.07% | 15.326 | 28.857 | 105 → 15 | 220.93 → 201.89 |

RSS는 전체 process의 driver/allocator cache 포함 checkpoint 평균이며 leak 또는 live GPU bytes의 지표로 해석하지 않는다.

## Scene / render objects

Texture를 제외한 count는 두 quality/format 동일. Label/companion subtree와 offscreen task의 unique object inventory이며 default scene task는 제외한다.

| Workload | Actor | Renderer | Geometry | Offscreen RenderTask |
|---|---:|---:|---:|---:|
| 350 | 88 → 88 | 80 → 40 | 25 → 33 | 24 → 24 |
| 500 | 88 → 88 | 144 → 40 | 25 → 33 | 24 → 24 |
| fhd | 51 → 51 | 50 → 25 | 19 → 25 | 18 → 18 |

## Input plane replacement, one Label

HIGH/PERFORMANCE 공통. Baseline isolated line inputs → candidate atlas planes; ordinary/global inputs는 별도로 유지한다.

| Workload | Format | Line input MiB → Atlas MiB | Input textures | Atlas dimensions |
|---|---|---:|---:|---|
| 350 | A8 | 0.380 → 0.422 | 12 → 2 | 492×180 |
| 350 | RGBA | 0.684 → 0.760 | 18 → 3 | 492×180 |
| 500 | A8 | 0.863 → 0.999 | 28 → 2 | 499×420 |
| 500 | RGBA | 1.553 → 1.799 | 42 → 3 | 499×420 |
| fhd | A8 | 7.821 → 8.514 | 62 → 2 | 1920×930 |
| fhd | RGBA | 14.077 → 15.326 | 93 → 3 | 1920×930 |

## FBO dimensions

Atlas batching 전후 동일. 각 page의 Source/H/V (폭×높이), A8는 L8, RGBA는 RGBA8888.

| Workload | Quality | Pages | Source | H | V | Shared scratch |
|---|---|---:|---|---|---|---|
| 350 | PERFORMANCE | 8 | 542×480 | 136×480 | 136×120 | none between Labels |
| 350 | HIGH | 8 | 542×480 | 542×480 | 542×480 | none between Labels |
| 500 | PERFORMANCE | 8 | 549×1120 | 138×1120 | 138×280 | none between Labels |
| 500 | HIGH | 8 | 549×1120 | 549×1120 | 549×1120 | none between Labels |
| fhd | PERFORMANCE | 6 | 1970×480 | 493×480 | 493×120 | H across 6 pages |
| fhd | HIGH | 6 | 1970×480 | 1970×480 | 1970×480 | Source/H across 6 pages |

## FHD A8 CPU follow-up

초기 3회에서 HIGH CPU mean +19.2%, 범위 일부 겹침. 이를 숨기지 않고 동일 조건 CPU-only 5 paired runs를 추가했다. 각 반복 실행 순서를 교대했고 workload/코드는 바꾸지 않았다. 모두 포함해 판정한다.

| Quality | 추가 5회 baseline mean [min,max] ms/s | 추가 5회 candidate mean [min,max] | 변화 | 전체 8회 baseline → candidate mean | 전체 변화 |
|---|---:|---:|---:|---:|---:|
| PERFORMANCE | 209.2 [184.8, 243.7] | 180.7 [155.5, 242.9] | -13.6% | 212.4 → 199.3 | -6.2% |
| HIGH | 211.2 [180.4, 234.3] | 173.2 [149.4, 258.1] | -18.0% | 212.0 → 203.6 | -4.0% |


최초 HIGH mean 증가는 재현되지 않았고, 추가 paired 5회 중 4회는 candidate 감소/1회 증가였다. PERFORMANCE도 4회 감소/1회 증가. 따라서 repeated non-overlapping stable >10% regression 조건은 충족하지 않는다. 그렇다고 FHD CPU 개선률을 확정한 것도 아니다. 범위가 넓고 순위가 바뀌므로 **FHD CPU benefit unproven / target verification required**로 판단한다.

모든 raw runs/ranges, setup CPU/wall, heap/RSS, GPU query frames/invalid counts는 `matrix.json`, `perf/`, `fhd-cpu-followup/`에 보존했다. 어떤 CPU run도 outlier라는 이유로 제외하지 않았다.
