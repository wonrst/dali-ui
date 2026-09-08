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
 * Reveal controls how content becomes visible as TextRevealProgress advances
 * from 0.0 to 1.0. Applications control playback duration by animating
 * TextRevealProgress.
 *
 * Unit selects the reveal granularity, while Sequence selects whether reveal
 * units share one timeline across the whole visible content or use an
 * independent timeline for each final visible line.
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
 * When blur is enabled, TextRevealProgress controls the combined reveal and
 * blur timeline. The existing reveal schedule and blur intervals are scaled
 * together to finish at progress 1.0. Animation alpha functions affect both
 * effects; Reveal does not create or extend an Animation.
 *
 * @note Blur is substantially more expensive than Reveal without blur. Setup
 * requires additional raster/metadata processing, temporary CPU buffers and
 * texture uploads. Foreground capture and horizontal/vertical Gaussian passes
 * add GPU work, texture bandwidth and offscreen textures with blur margins.
 * Cost depends on rendered area, UI/render scale, radius, content format and
 * the number of active Labels and sequences. Larger radii require more
 * filtering samples and larger margins.
 *
 * @note Disable blur when no longer needed. Reaching an Animation endpoint or
 * holding progress still does not unschedule its passes or release its resources.
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
     * Lines are determined after shaping, wrapping, maximum-line limiting,
     * ellipsis, bidirectional layout, and ImageSpan placement. Lines without
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
   * The ratio is resolved with the unit and sequence schedule, and optional
   * blur normalization; it is not generally a fraction of the total Animation
   * duration.
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
   * When automatic duration is selected, this returns
   * AUTO_FADE_DURATION_RATIO rather than the resolved duration.
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
   * normalized to 0.0. The default is 0.0. After UI scaling, a nonzero radius
   * uses a minimum kernel radius of 4 pixels and is rounded up to an even
   * integer. GetBlurRadius() returns the authored logical-pixel value.
   *
   * Blur affects synchronous and asynchronous text foreground. Each sequence
   * shares one blur strength; it is not a separate blur for each reveal unit.
   * Progress controls the strength using a fixed kernel, without rebuilding
   * that kernel for each frame. Decorations retain ordinary Reveal behavior.
   * Asynchronous render scaling changes source raster resolution, not the
   * displayed blur radius.
   *
   * Inline ImageSpan content shares its sequence's blur and reveal timing.
   * Image loading does not delay or restart the timeline; a ready image uses
   * the current progress.
   *
   * @note Blur is not applied to embossed text or height-tiled text textures.
   * These configurations retain Reveal without blur. Configurations exceeding
   * the supported kernel or render-target size limits also use Reveal
   * without blur. The authored blur settings are retained in these cases.
   *
   * @note Blur can extend outside the Label bounds; use Label clipping to
   * restrict it. Offscreen capture can produce different image-edge
   * antialiasing from direct ImageSpan rendering, including at progress 1.0.
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
   * @brief Sets the blur duration relative to the common sequence interval.
   *
   * The reference is the same sequence interval used for sequence staggering,
   * not the entire Animation duration or each individual line's reveal span.
   * WHOLE_TEXT has one reference interval; PER_LINE shares one reference
   * interval across all active lines, including short final lines.
   *
   * Zero disables blur. A value of 1.0 uses the complete reference interval.
   * The blur strength decreases from its maximum at each sequence start to
   * zero at the end of its blur interval. Units revealed later in the sequence
   * can therefore appear after its blur has ended.
   * Blur intervals overlap when they exceed the sequence start spacing.
   * Reveal and blur are normalized together to finish at progress 1.0, so
   * actual durations depend on the layout and the Animation alpha function.
   * If blur is the only time-varying effect, it occupies the complete timeline.
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
