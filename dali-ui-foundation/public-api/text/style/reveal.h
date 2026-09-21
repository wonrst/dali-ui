#pragma once

/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <dali-ui-foundation/public-api/dali-ui-common.h>

#include <cstdint>

namespace DALI_NAMESPACE
{
namespace Ui
{
namespace Text
{

/**
 * @brief Describes how visible text and inline ImageSpan content are revealed.
 *
 * TextRevealProgress controls visibility from 0.0 (hidden) to 1.0 (visible).
 * Applications control playback by animating this property.
 *
 * Unit selects CHARACTER, WORD, LINE, or continuous PIXEL progression.
 * Sequence selects one WHOLE_TEXT timeline or independent PER_LINE timelines
 * for the final visible layout lines.
 *
 * Reveal affects the text foreground and inline ImageSpan content. ImageSpan
 * content reveals as an atomic item in CHARACTER, WORD, and LINE modes, and
 * reveals spatially across the rendered image in PIXEL mode. Text decorations
 * and style layers such as shadow, outline, underline, strikethrough, and
 * background are not affected.
 *
 * When content is elided, hidden source content does not consume reveal units
 * and the visible ellipsis participates in the reveal progression.
 *
 * By default, Reveal uses CHARACTER with WHOLE_TEXT sequencing, no sequence
 * stagger, an automatically resolved fade duration, and no blur.
 *
 * Blur uses the same TextRevealProgress. Reveal and blur timing are normalized
 * together to finish at progress 1.0. Animation alpha functions affect both;
 * Reveal does not create or extend an Animation.
 *
 * @note Blur is substantially more expensive than Reveal without blur. Setup
 * and rendering require additional offscreen processing and resources. Cost
 * depends on rendered area, radius, quality, and the number of active Labels
 * and sequences.
 *
 * @note Reaching an endpoint or holding progress still does not automatically
 * stop blur processing or release its resources. Disable blur or Reveal when
 * no longer needed.
 */
class DALI_UI_API Reveal
{
public:
  /**
   * @brief Selects automatic fade duration.
   *
   * GetFadeDurationRatio() returns this authored value rather than the
   * duration resolved for the current layout.
   */
  static constexpr float AUTO_FADE_DURATION_RATIO = -1.0f;

  /**
   * @brief Selects the granularity of reveal progression.
   */
  enum class Unit : uint8_t
  {
    /**
     * @brief Reveals visible text by character while preserving shaping boundaries.
     *
     * Progression follows logical text order. Characters that form one
     * indivisible shaped unit reveal together.
     */
    CHARACTER,

    /**
     * @brief Reveals visible text by word in logical text order.
     *
     * Whitespace does not create a reveal unit.
     */
    WORD,

    /**
     * @brief Reveals one final visible layout line as a unit.
     *
     * Lines are determined after wrapping and overflow handling. Lines without
     * revealable visible content do not consume a reveal unit.
     */
    LINE,

    /**
     * @brief Reveals visible content continuously across its rendered foreground.
     *
     * PIXEL follows logical text order and shaping boundaries while distributing
     * reveal timing continuously in pixel space. Inline ImageSpan content
     * contributes its reserved width to the progression and reveals spatially
     * across the rendered image.
     *
     * The progression is normalized and does not correspond one-to-one with
     * physical framebuffer pixels.
     */
    PIXEL
  };

  /**
   * @brief Selects how reveal units share the normalized reveal timeline.
   */
  enum class Sequence : uint8_t
  {
    /**
     * @brief Uses one reveal sequence across all final visible content.
     */
    WHOLE_TEXT,

    /**
     * @brief Uses an independent sequence for each final visible layout line.
     *
     * Lines created by wrapping are separate sequences, while lines without
     * revealable content do not create a sequence. With Unit::LINE, each
     * sequence contains one whole-line reveal unit.
     */
    PER_LINE
  };

  /**
   * @brief Selects the quality and processing cost of reveal blur.
   */
  enum class BlurQuality : uint8_t
  {
    /**
     * @brief Prioritizes blur quality using full-resolution filtering, with higher processing cost.
     */
    HIGH,

    /**
     * @brief Prioritizes performance using reduced-resolution blur processing.
     */
    PERFORMANCE,

    /**
     * @brief Reduces filtering cost for constrained devices using quarter-resolution blur.
     *
     * Blends sharp text with reduced-resolution blur using a perceptual transition.
     * The visual result can differ from HIGH and PERFORMANCE.
     */
    ECONOMY
  };

public:
  /**
   * @brief Creates a CHARACTER reveal using WHOLE_TEXT sequencing and automatic fade duration.
   */
  Reveal();

  /**
   * @brief Creates a reveal by copying another reveal.
   *
   * @param[in] rhs The reveal to copy.
   */
  Reveal(const Reveal& rhs);

  /**
   * @brief Creates a reveal by moving another reveal.
   *
   * @param[in] rhs The reveal to move.
   */
  Reveal(Reveal&& rhs) noexcept;

  /**
   * @brief Copies another reveal to this object.
   *
   * @param[in] rhs The reveal to copy.
   */
  Reveal& operator=(const Reveal& rhs);

  /**
   * @brief Moves another reveal to this object.
   *
   * @param[in] rhs The reveal to move.
   */
  Reveal& operator=(Reveal&& rhs) noexcept;

  /**
   * @brief Destructor.
   */
  ~Reveal();

  /**
   * @brief Returns the value that disables text reveal.
   *
   * Pass this value to Label::SetTextReveal() to remove reveal rendering.
   * It can be copied or compared, but its configuration getters and setters
   * must not be called.
   *
   * @return A shared immutable none value.
   */
  static const Reveal& None();

  /**
   * @brief Compares this reveal with another reveal.
   *
   * @param[in] rhs The reveal to compare with.
   * @return true if both values are equal.
   */
  bool operator==(const Reveal& rhs) const;

  /**
   * @brief Compares this reveal with another reveal.
   *
   * @param[in] rhs The reveal to compare with.
   * @return true if both values are not equal.
   */
  bool operator!=(const Reveal& rhs) const;

public:
  /**
   * @brief Sets the reveal unit.
   *
   * @param[in] unit The reveal unit.
   */
  void SetUnit(Unit unit);

  /**
   * @brief Gets the reveal unit.
   *
   * @return The reveal unit.
   */
  Unit GetUnit() const;

  /**
   * @brief Sets the reveal sequencing mode.
   *
   * @param[in] sequence The reveal sequencing mode.
   */
  void SetSequence(Sequence sequence);

  /**
   * @brief Gets the reveal sequencing mode.
   *
   * @return The reveal sequencing mode.
   */
  Sequence GetSequence() const;

  /**
   * @brief Sets the fade duration on the normalized reveal timeline.
   *
   * AUTO_FADE_DURATION_RATIO selects the duration automatically. Values from
   * 0.0 to 1.0 specify the normalized fade duration; zero disables the fade.
   * The duration depends on the unit and sequence schedule and optional blur;
   * it is not generally a fraction of the total Animation duration.
   *
   * Values outside [0.0, 1.0] are clamped, except AUTO_FADE_DURATION_RATIO.
   * NaN is normalized to 0.0.
   *
   * @param[in] ratio AUTO_FADE_DURATION_RATIO or a value in [0.0, 1.0].
   */
  void SetFadeDurationRatio(float ratio);

  /**
   * @brief Gets the authored fade duration ratio.
   *
   * @return AUTO_FADE_DURATION_RATIO or the authored ratio in [0.0, 1.0].
   */
  float GetFadeDurationRatio() const;

  /**
   * @brief Sets the stagger between consecutive reveal sequences.
   *
   * The ratio controls the spacing between consecutive active sequence starts.
   * A value of 0.0 starts all active sequences together, while 1.0 prevents
   * consecutive sequences from overlapping.
   *
   * Stagger affects PER_LINE sequencing. WHOLE_TEXT uses a single sequence, so
   * the authored value has no visual effect. With Unit::LINE, stagger controls
   * the start spacing between whole-line transitions independently of the fade
   * duration.
   *
   * Values outside [0.0, 1.0] are clamped, and NaN is normalized to 0.0.
   *
   * @param[in] ratio The sequence stagger ratio in [0.0, 1.0].
   */
  void SetSequenceStaggerRatio(float ratio);

  /**
   * @brief Gets the authored sequence stagger ratio.
   *
   * @return The authored ratio in [0.0, 1.0].
   */
  float GetSequenceStaggerRatio() const;

  /**
   * @brief Sets the blur radius in logical pixels.
   *
   * Zero disables blur. Values outside [0.0, 64.0] are clamped, and NaN is
   * normalized to 0.0. The default is 0.0. GetBlurRadius() returns the authored
   * logical-pixel value.
   *
   * Blur affects synchronous and asynchronous text foreground. Each sequence
   * shares one blur strength; it is not a separate blur for each reveal unit.
   *
   * Inline ImageSpan content shares its sequence's blur and reveal timing.
   * Image loading does not delay or restart the timeline; a ready image uses
   * the current progress.
   *
   * @note Blur is not applied to embossed text or height-tiled text textures.
   * These configurations, or those exceeding supported size limits, retain
   * Reveal without blur. The authored blur settings are preserved.
   *
   * @note Blur can extend outside the Label bounds; use Label clipping to
   * restrict it. ImageSpan edge antialiasing may differ from rendering without
   * blur, including at progress 1.0.
   *
   * @param[in] radius The blur radius in [0.0, 64.0].
   */
  void SetBlurRadius(float radius);

  /**
   * @brief Gets the authored blur radius.
   *
   * @return The blur radius in logical pixels.
   */
  float GetBlurRadius() const;

  /**
   * @brief Selects the blur quality and performance trade-off.
   *
   * The default is PERFORMANCE. Quality does not change the reveal or blur
   * timeline, and has no visual effect when blur is disabled.
   *
   * @param[in] quality The blur quality.
   */
  void SetBlurQuality(BlurQuality quality);

  /**
   * @brief Gets the authored blur quality.
   *
   * @return The blur quality.
   */
  BlurQuality GetBlurQuality() const;

  /**
   * @brief Sets the blur duration relative to the common sequence interval.
   *
   * All active sequences share the reference interval used for staggering.
   * The ratio is not directly a fraction of the total Animation duration or
   * each individual line's reveal span.
   *
   * Zero disables blur. A value of 1.0 uses the complete reference interval.
   * The blur strength decreases from its maximum at each sequence start to
   * zero at the end of its blur interval. Units revealed later in the sequence
   * can therefore appear after its blur has ended.
   * Reveal and blur timing are normalized together to finish at progress 1.0.
   *
   * Values outside [0.0, 1.0] are clamped, and NaN is normalized to 0.0.
   * The default is 1.0. A nonzero blur radius is also required to enable blur.
   *
   * @param[in] ratio The blur duration ratio in [0.0, 1.0].
   */
  void SetBlurDurationRatio(float ratio);

  /**
   * @brief Gets the authored blur duration ratio.
   *
   * @return The blur duration ratio in [0.0, 1.0].
   */
  float GetBlurDurationRatio() const;

private:
  class Impl;
  Impl* mImpl{nullptr};
};

} // namespace Text
} // namespace Ui
} //namespace DALI_NAMESPACE
