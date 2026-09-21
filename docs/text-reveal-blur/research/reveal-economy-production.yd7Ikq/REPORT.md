# Text::Reveal ECONOMY — production implementation / cleanup

## A. Executive verdict

**TARGET-READY.** 2026-09-21, Ubuntu/GLES 검증 기준.

HIGH/PERFORMANCE를 보존하고 ECONOMY를 정상 public/runtime 경로로 추가했다.
Core/Adaptor/UI의 이번 연구용 trace와 V-only/one-tap/source-only 경로를 제거했다.
일반 샘플에서 세 품질을 선택할 수 있고 새 성능 계측 코드는 없다.

PC build, 전체 UI UTC, 실제 GLES 화면 비교, 집중 ASAN/LSAN 검증을 수행했다.
이번 단계에서 GBS/Windows 빌드나 실제 TV 설치·FPS·GPU 시간·RSS/VRAM 측정은 하지 않았다.
따라서 TARGET-READY는 **타겟 검증에 넘길 수 있는 source 상태**이지 타겟 성능/품질 승인이라는 뜻은 아니다.

## B. Clean baseline / removed diagnostics

시작 상태는 BASELINE.json (로컬 자료: `BASELINE.json`), 복구 가능한 원본은 backup (로컬 자료: `backup`)에 있다.

| Repo | 시작 HEAD | 제거한 task-scoped instrumentation |
|---|---|---|
| Core | `8228720460a4910151f4eb4ad36976816b13a102` | trace.cpp collector 연결, core/update attribution, untracked reveal-attribution.inc |
| Adaptor | `a3ab9b8db9637fda4c973b5d59c4d4b95a9d5286` | combined update/render, program binding, EGL swap attribution |
| UI | `05087317cac8ea9600bba498f00ccf8086a79d3f` | preparation/runtime/publication/demo RYU trace; V-only/one-tap/source-only PoC |

UI production 비교 기준은 실험 전 `b54bb666` (Batch text reveal blur sources)이다.
Source/Output batching, D2, Gaussian shader factory, async/lifecycle production 수정은 보존했다.
일반 플랫폼의 기존 진단 기능이나 unrelated performance 샘플은 제거하지 않았다.

`samples/text/text-reveal-perf-example.cpp`와 해당 CMake target 한 줄을 제거했다.
이 파일은 Git에서 복구 가능하다. untracked collector는 backup/reveal-attribution.inc에서 복구 가능하다.
기존 외부 연구 보고서와 PoC 디렉터리는 수정/삭제하지 않았다.

관련 source와 build 설정에서 RYU_REVEAL_ATTRIBUTION, DALI_TRACE_REVEAL,
gRevealBoundaryTrace/gRevealDemoTrace, CommandDrain, USE_PERFORMANCE_*POC,
CreateOneTapDiagnostic, TEXT_REVEAL_*DIAGNOSTIC, TEXT-REVEAL-*POC, `RYU - ` 잔여 없음.

## C. Remaining git history

`perf test` (`511fef03`), `perf test 2` (`035e79f3`), `perf test 3` (`05087317`)는
시작 시 origin/devel_blur_text에 포함되어 있었다. **공유 이력은 rewrite하지 않았다.**
그 위에 non-destructive cleanup을 쌓는다. commit 존재와 production source 잔여를 구분한다.
reset --hard / stash / rebase / push 없음. 검증 전 변경은 모두 unstaged로 유지했다.

## D. BlurQuality API / conversion / compatibility

`enum class BlurQuality : uint8_t`에 ECONOMY를 마지막으로 append했다.

| 값 | 숫자 | 기본 선택 |
|---|---:|---|
| HIGH | 0 | 기존 값 유지 |
| PERFORMANCE | 1 | **기본값 유지** |
| ECONOMY | 2 | 새 선택지 |

public 설명은 constrained-device filtering cost, reduced-resolution/perceptual composition,
HIGH/PERFORMANCE와 다른 시각 결과를 명시한다. gamma/property나 새 setter는 추가하지 않았다.
기존 Set/GetBlurQuality, copy/move/equality, Label Set/GetTextReveal을 그대로 사용한다.

Repository-wide 사용처 감사 결과 별도 BlurQuality 문자열 parser/Property::Map/builder 변환표는 없다.
typed invocation과 Label/TextVisual state가 enum을 전달한다. unknown enum runtime fallback은
기존대로 PERFORMANCE이며 UTC로 확인했다. struct/public class layout 변경 없음.
baseline shared library 대비 exported symbol 제거는 0개(removed-symbols.txt (로컬 자료: `removed-symbols.txt`)).
정식 ABI checker나 구버전 타겟 binary 실행 검증을 대체하는 결과는 아니다.

## E. ECONOMY architecture

```text
current Source(progress), full resolution ──────────────────┐
       │                                                  │ sharp
       ▼                                                  │
H: quarter X / quarter Y, original horizontal kernel       │
       │                                                  │
       ▼                                                  │
V: same size, Gaussian converted to H's actual Y domain    │
       │ blur                                             │
       └───────────────────────► Output ◄───────────────────┘
```

ページ当たり Source/H/V の **3 offscreen stages/tasks/FBO**を維持する。
Outputは既存の画面描画。prefilter、第三のOutput texture、追加blur level/passなし。
各 H/V は最初から必要な小さいサイズで割り当て、full-size Hを後で交換しない。
同一寸法/formatのページ間で既存H scratch共有を維持し、SourceはOutput用に保持する。

Internal RuntimeRevealBlurPathに ECONOMY_QUARTER を追加した。
共通オーケストレーションを保ち、dimensions、V kernel、Output shader variantで明確に分岐する。

## F. Dimension / storage / D2 policy

W/Hはhaloとpage paddingを含むSourceの実際のpixel extent。

| 品質 | Source | H | V |
|---|---|---|---|
| HIGH | W×H | W×H | W×H |
| PERFORMANCE | W×H | ceil(W/4)×H | ceil(W/4)×ceil(H/4) |
| ECONOMY | W×H | ceil(W/4)×ceil(H/4) | Hと同寸法 |

タスク数削減ではなく、Hのfragment area/storageとV filtering workを減らす変更である。
Outputのfull-resolution Source retention/二枚sample/dependencyはPERFORMANCE同様に残る。
original text/metadata texture、CPU raster、ページ包装の仕組みは変えていない。

ECONOMYにはD2のfull-Y H coverage bandを適用せず、full padded line quadsを使用する。
quarter-Yでの同一coverageを新たに仮定しない保守的な選択である。
HIGH/PERFORMANCEのD2は変更なし。
Source→H、H→V、Output Source/Vは既存の同じlogical line rectangleを使用し、
H→V clampには**実際の入力texture inverse extent**を渡す。

## G. V Gaussian derivation / ownership

元のCalculateGaussianWeight/BellCurveWidth/GaussianConstantsを小さい内部headerへ抽出した。
旧GaussianBlurAlgorithmの計算順序・式・cache/UBO所有モデルは変更しない。
抽出後の元係数はnumSamples 1～100の全組で旧実装とbyte-identicalだった。

ECONOMY Vは次を計算する。

```text
s       = actual SourceHeight / actual HHeight
n       = validated kernelRadius / 2
sigmaV  = originalBellCurveWidth(n) / s
support = floor((2*n - 1) / s)
```

有限supportの低解像度Gaussianを正負対称に正規化し、隣接tapをbilinear packingする。
単純なinteger radius/4ではない。s=1は元の係数をそのまま使う。
ゼロweightのoffsetは0。最小2pairsへゼロpaddingしsingle-element uniform reflectionを回避する。

layout依存の係数を無制限なfloat-keyed global cacheに入れない。
V shaderがそのprivate UniformBlockを**strong connectionで所有**し、renderer/shader終了時に解放する。
既存共有Gaussian UBOを変更/公開しない。
異なるactual scaleの二つのrendererが異なる係数を持ち、一方の解放が他方に影響しないUTCを追加した。

## H. Perceptual output curve / endpoint work

native amount `b = 1 - smoothstep(0,1,q)` を使用する。

```text
floor      = min(R, clamp(R*0.5, 10, 12))
radius     = floor + (R-floor) * smoothstep(.35,1,b)
sharpAlpha = 1 - smoothstep(0,.60,b)
blurAlpha  = min(1-sharpAlpha, .90*smoothstep(0,.65,b))
output     = currentSource*sharpAlpha + currentBlur*blurAlpha
```

曲線はupdate-sideの既存progress constraintで評価する。fragmentにpowや追加filterはない。
ECONOMY shader define/vec2 weightsを分離し、PERFORMANCE Late Smooth式を変えていない。
premultiplied入力を同じweightで合成し、weight合計は1以下。
full-blur側は合計.9となるため意図的な減光はあり得る。

blurAlpha=0ではH/V strengthを0とし既存copy early-outを使う。
before-startは既存line-start handlingを維持する。
taskを停止/再開する新しい状態機械は作らず、seek/reverseで古いFBOを読むリスクを増やさない。
endpointでもタスク/clear/copy/Outputの全コストがゼロになるわけではない。

## I. Authored radius behavior

public [0,64] clamp/default/NaN処理を変更しない。ECONOMYによるpreset overrideなし。
floorはR16で10、R24で12、大きいRでも12以下、小さいRではR以下。
既存の最低/偶数kernel radiusへ丸めた係数に対し、ECONOMY sampling strengthを
effective authored radius / validated kernel radiusとして設定する。

Demo Strong: **entrance24 / exit48**、Medium20/40、Soft16/32を保つ。
R32固定やgamma1.5の研究設定はない。

## J. Timing behavior

**gamma=1、native timingのまま。** 別のclock、pow、Animation、callback待ちは追加しない。
Unit/Fade/Stagger/BlurDurationRatio、start/duration、progress/completion、reverseは既存planを使う。
変えるのはその時点のnative blur amountからsampling radiusと合成weightを求める部分だけ。
同じtimelineでも知覚上のsharpnessや明るさは品質によって異なり得る。

## K. HIGH regression

実験前b54bb666のGaussian/runtime/rendererを別libraryとして構築し、現在版と同じ環境で比較。
実progress .20/.50/.75/.90/1.0の5画面は**全てpixel-identical、最大channel差0**。
各画面はWHOLE_TEXT/PER_LINE × A8/gradient/ImageSpanの6Labelを含む。
Source/H/V full-size、元のshader/kernel/caches/source scratch契約を維持した。

## L. PERFORMANCE regression

同じ5画面で**全てpixel-identical、最大channel差0**。
quarter-X/full-Y H、quarter-XY V、Late Smoothの2px/8px anchorsとquintic式を維持した。
Source retention、D2、refresh rate、task countは不変。

結果：visual-regression.json (로컬 자료: `visual-regression.json`)。
この10枚は具体的なfixtureでの比較であり、全デバイス/全入力のpixel-equivalenceの証明ではない。
capturing前にKeepRenderingでproperty更新をflushし、ログのSTATEで実progressを確認した。

## M. ECONOMY WHOLE_TEXT

sync/asyncのpublish、dimension selection、A8/RGBA、forward/reverse/seek、quality切替をUTCで確認。
実GLESでもSource+Vの合成とsharp endpointを確認した。
scalar outputのvec2 weightとreduced V kernelを使い、既存whole-text Source captureを再利用する。

## N. ECONOMY PER_LINE

native line start/Staggerに同じcurveを個別適用する。
1/2/6/64/65-line、複数page、draw-batch boundary、style/layout変更をownership stressに含めた。
Outputのline-local weight、quarterXY H/V、3N offscreen task、独立clampを確認。
異なるlineの色/Source表現を再構成せず、既存captureとpage mappingを使用する。

## O. A8 / RGBA / Gradient / ImageSpan

A8のtext-color復元、premultiplied RGBA、whole gradient、gradient overlay/multicolorを
既存captureと合成の枠内で処理する。色を別のモデルへ変換しない。
実GLES比較fixtureは白A8、赤青gradient、ローカルin-memory ImageSpanを含む。
実装中に見つかったbatch ECONOMYの空uniform blockを除去し、最終shader compile/linkエラーなし。

ImageSpan UTCはsync/async × WHOLE_TEXT/PER_LINE、ready/failure/resource refreshとNone復帰を確認。
ImageVisualコードは変更していない。既存の単一sample captureによるMSAA edge差は残る。
細かな色味/haloの知覚品質はこのfixtureでの確認範囲であり、全corpus無差異とは主張しない。

## P. Odd size / radius edges

runtime UTCはeven/odd width/height/both、radius0/.5/4/5/24/64を確認。
kernel UTCはsource height1/2/3/4/5/101/1180/1181とradius4/6/16/24/48/64/200を検査。
public Reveal最大radiusは64。200はGaussian helperの既存対応範囲の検査である。
normalization、finite/nonnegative weights/offsets、source-space support、actual ratio、単位scale一致を検証。
最小textから64/65-lineまでfont metricsに依存するexact glyph countを追加しない。

## Q. Lifecycle / async / invalidation

scene connect/disconnect/reconnect、**animation実行中のscene removal**、None、destroy/recreate、
same-text/changed-text、radius/quality変更、style/gradient/size変更、ImageSpan readinessを確認。
quality切替は既存publication invalidationで旧runtimeを解放して新しいresourceを組み立てる。
async revisionやCPU preparation、font/cache/language invalidationのarchitecture変更なし。

font/cache/language通知は従来通りSourceの再publishに到達するコード経路を監査した。
言語通知と全glyph-cache破棄の組合せをECONOMY専用に新たにfault-injectしたわけではない。
既存の関連Label/TextVisual UTCも全suite内で実行した。

## R. Sample UX

- `text-effect-demo.example`:既存qualityボタンを High → Performance → Economy → Highにした。
  **3キー/KP_3**でも切替。既存Blur ON/Strong、1Sync/2Async、scene transitionsを保つ。
- `text-reveal.example`:既存QUALITY行へ**Economy**を追加。radius/blur-time/progress controlsを保つ。

通常demoで3品質を切替→0再開→次pageまで実行。
通常Reveal sampleで3品質の**Preset 2**を実行した
（entrance24、BlurDurationRatio1、EASE_OUT_SQUARE → wait → exit48）。
[ECONOMY動作画面](reveal-preset-economy.png)、[Demo ECONOMY](demo-economy.png)。

既存long entranceのBlurDurationRatio=.5は変更していない。
short entrance/Preset 2の1.0と別fixtureの1.0で要求条件を確認した。
sampleにtrace/FPS/GPU timer等を追加していない。

## S. Tests / builds

| 項目 | 結果 |
|---|---:|
| Core full build/install (trace除去後) | PASS |
| Adaptor full build/install (trace除去後) | PASS |
| UI foundation/components full build/install | PASS |
| normal demo / Reveal sample build | PASS |
| public foundation UTC | 2434/2434 |
| internal foundation UTC | 715/715 |
| components UTC | 464/464 |
| **UI UTC合計** | **3613/3613** |
| focused ASAN/LSAN cases | 7/7 |
| HIGH/PERFORMANCE real-GLES reference captures | 10/10 identical |
| original Gaussian coefficient identity | 100/100 |
| git diff --check / diagnostic residue search | PASS / 0 matches |

ECONOMY kernel/ownership/curve/dimensions-lifecycle/ImageCaptureの5UTCを追加し、
public value、quality publication、ownership stressを拡張した。
初回internal runで2件の既存shader-string UTCが失敗した。
原因は既にb54bb666のSource batchingで導入されたTEXT_SOURCE_UVを旧literal期待値が含まなかったため。
該当2期待値だけをproduction文字列に合わせ、その後全internal suiteはPASS。
失敗記録も削除せずutc-full (로컬 자료: `utc-full`)へ残した。最終internal記録はutc-final (로컬 자료: `utc-final`)。

Core/Adaptorの独立UTC suiteは今回実行していない（変更は未commit trace除去のみ、full build検証）。
対象GLESは現在ローカル環境。GBS/Windows/TV driver固有検証は未実施。

## T. Leak / resource results

既存155cyclesを保ち、ECONOMY25cyclesを追加した180cycle stressがPASS。
mock GL live texturesは0→0、allocated buffersは5→5、tracked weak objectsは全て失効、
scene default以外のtaskは0に戻る。ImageSpan既存100cycle testもfull suiteでPASS。
ECONOMY専用ImageSpanとquality切替を別途実行した。

private V UniformBlockはrenderer存続中に保持され、renderer解放後は失効する。
異なるscaleの二つの所有関係も検証した。Source/H/V textureとV shader weak handleも失効する。

ASAN/LSANはGaussian algorithm、新renderer/runtime、Reveal runtime UTCを再compileして計測した
**focused build**。他のUI/Core/Adaptor既存objectsは未instrumentedで、全platform ASANではない。
7対象全てexit0、ASAN/LSANエラーなし。logs (로컬 자료: `asan`)、build scope (로컬 자료: `build-focused-asan.py`)。
これはGPU driver resident memory/RSSの無増加測定ではない。今回新しい性能/VRAM測定はしていない。

## U. Remaining quality / cost trade-offs

ECONOMYはquarter-Y Hにprefilterを足さず、weak Gaussianの正確な再現を目的としない。
PERFORMANCEよりline/stroke modulation、banding、softness、減光が見える場合がある。
Native timingは同じだが知覚されるblur/contrastの変化は同じではない。
必要に応じてHIGH/PERFORMANCEを選べる。

3-stage/task submissionとfull Source/metadata/Outputコストは残る。
H/Vの構造的削減がtarget FPSにどれだけ寄与するかは、今回新たに測っていない。
既存のunsupported size/tiling/emboss等のordinary Reveal fallback契約も拡張していない。

## V. Target validation checklist

1. 現在の正常なCore/AdaptorとUIを通常のGBS手順でbuild/installする。trace flag/env/LD_PRELOADは不要。
2. `samples/text`の通常`text-effect-demo.example`を起動。Blur ON/Strongを維持し、
   quality buttonまたは3キーでHIGH/PERFORMANCE/ECONOMYを切替。0で同じ場面を再開。
3. 1Sync/2Async、入場24/退場48、複数Labelの同時退場とpage移動を比較する。
4. 通常`text-reveal.example`でQUALITYを選択し、Preset 1/2、WHOLE_TEXT/PER_LINE、
   odd previewサイズ、UI scale、A8/gradient/画像、forward/reverse/seekを確認する。
5. Radius0/小半径/64、blur on/off、None、rapid quality/text変更、scene移動・終了を確認。
6. ECONOMYのline/stroke modulationと減光を実UXで承認する。
   FPS等が必要なら通常アプリ＋外部ツールで測る。sourceに研究用collectorを戻さない。

## W. Final git status / commits

검증 후 다음 세 commit으로 완료했다. 모두 `Signed-off-by: Bowon Ryu <bowon.ryu@samsung.com>`을 포함한다.

| Commit | Title |
|---|---|
| `41faff78` | Remove reveal performance diagnostics |
| `37534795` | Add economy text reveal blur |
| `6eaab62a` | Add economy blur sample controls |

Core/Adaptor는 시작 HEAD의 변경 없이 clean. UI `devel_blur_text`도 clean이다.
공유된 perf commit 이력은 그대로 두었고, 새 local cleanup commit의 staging 분리만 보정했다.
최종 누적 diff --check 통과, 관련 diagnostic symbol 0 matches. **push하지 않았다.**
