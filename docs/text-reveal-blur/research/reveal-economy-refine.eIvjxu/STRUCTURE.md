# Logical resources and filtering expressions

No VRAM/RSS measurement. FBO extent × shader reads is an upper bound, not measured fragment invocations.

| Demo arm / phase | Offscreen tasks | Unique FBO textures | Source pixels | H pixels | V pixels | Source+H+V bytes | H+V full-extent expressions |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline / CardsStart | 60 | 60 | 561,629 | 140,684 | 35,831 | 816,864 | 4,236,360 |
| baseline / ExitStart | 57 | 57 | 1,654,232 | 413,688 | 104,435 | 2,668,015 | 24,869,904 |
| economy / ExitStart | 57 | 57 | 1,225,048 | 77,547 | 77,547 | 1,686,142 | 3,101,880 |
| economy / CardsStart | 60 | 60 | 719,277 | 45,735 | 45,735 | 897,243 | 1,841,072 |
| matched / CardsStart | 60 | 60 | 719,277 | 180,156 | 45,735 | 1,045,944 | 7,228,512 |
| matched / ExitStart | 57 | 57 | 1,225,048 | 306,360 | 77,547 | 1,965,751 | 12,285,024 |

Native inventories are separate, non-timed processes. Snapshot at phase start + about350–450ms. Shared textures deduplicated for bytes, not task work. Existing CURRENT D2 geometry can cover less than H framebuffer extent.

| Primary fixture / arm | radius | Source | H | V | Source bytes | H+V bytes | H/V reads | expressions |
|---|---:|---|---|---|---:|---:|---|---:|
| radius-soft0-R24-mode7 / CURRENT | 24 | [1972, 1132] | [493, 1132] | [493, 283] | 2,232,304 | 697,595 | 24/24 | 16,742,280 |
| radius-soft0-R24-mode7 / A | 24 | [1972, 1132] | [493, 283] | [493, 283] | 2,232,304 | 279,038 | 24/6 | 4,185,570 |
| radius-soft0-R32-mode7 / CURRENT | 24 | [1972, 1132] | [493, 1132] | [493, 283] | 2,232,304 | 697,595 | 24/24 | 16,742,280 |
| radius-soft0-R32-mode7 / A | 32 | [1988, 1148] | [497, 287] | [497, 287] | 2,282,224 | 285,278 | 32/8 | 5,705,560 |
| radius-soft0-R40-mode7 / CURRENT | 24 | [1972, 1132] | [493, 1132] | [493, 283] | 2,232,304 | 697,595 | 24/24 | 16,742,280 |
| radius-soft0-R40-mode7 / A | 40 | [2004, 1164] | [501, 291] | [501, 291] | 2,332,656 | 291,582 | 40/10 | 7,289,550 |
| radius-soft1-R16-mode7 / CURRENT | 16 | [1956, 1116] | [489, 1116] | [489, 279] | 2,182,896 | 682,155 | 16/16 | 10,914,480 |
| radius-soft1-R16-mode7 / A | 16 | [1956, 1116] | [489, 279] | [489, 279] | 2,182,896 | 272,862 | 16/4 | 2,728,620 |
| radius-soft1-R24-mode7 / CURRENT | 16 | [1956, 1116] | [489, 1116] | [489, 279] | 2,182,896 | 682,155 | 16/16 | 10,914,480 |
| radius-soft1-R24-mode7 / A | 24 | [1972, 1132] | [493, 283] | [493, 283] | 2,232,304 | 279,038 | 24/6 | 4,185,570 |
| radius-soft1-R32-mode7 / CURRENT | 16 | [1956, 1116] | [489, 1116] | [489, 279] | 2,182,896 | 682,155 | 16/16 | 10,914,480 |
| radius-soft1-R32-mode7 / A | 32 | [1988, 1148] | [497, 287] | [497, 287] | 2,282,224 | 285,278 | 32/8 | 5,705,560 |

| p=q | qBlur | b | radius floor10 / floor12 | sharpAlpha | blurAlpha |
|---:|---:|---:|---|---:|---:|
| 0.0 | 0.0000 | 1.0000 | 32.000 / 32.000 | 0.0000 | 0.9000 |
| 0.1 | 0.0316 | 0.9971 | 31.999 / 31.999 | 0.0000 | 0.9000 |
| 0.2 | 0.0894 | 0.9774 | 31.922 / 31.929 | 0.0000 | 0.9000 |
| 0.3 | 0.1643 | 0.9279 | 31.247 / 31.316 | 0.0000 | 0.9000 |
| 0.4 | 0.2530 | 0.8404 | 28.672 / 28.974 | 0.0000 | 0.9000 |
| 0.5 | 0.3536 | 0.7134 | 22.940 / 23.764 | 0.0000 | 0.9000 |
| 0.6 | 0.4648 | 0.5528 | 15.087 / 16.625 | 0.0176 | 0.8456 |
| 0.7 | 0.5857 | 0.3728 | 10.079 / 12.072 | 0.3217 | 0.5485 |
| 0.8 | 0.7155 | 0.1967 | 10.000 / 12.000 | 0.7480 | 0.1974 |
| 0.9 | 0.8538 | 0.0579 | 10.000 / 12.000 | 0.9739 | 0.0201 |
| 1.0 | 1.0000 | 0.0000 | 10.000 / 12.000 | 1.0000 | 0.0000 |
