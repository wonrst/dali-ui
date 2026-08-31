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

namespace Dali
{
namespace Ui
{
namespace Text
{

/**
 * @brief Describes how the text foreground is revealed over a normalized timeline.
 *
 * Reveal divides visible text into sequential units and controls the fade of
 * each unit as TextRevealProgress advances from 0.0 to 1.0. Applications
 * control the playback duration by animating TextRevealProgress.
 *
 * By default, text is revealed by character using an automatically selected
 * fade duration and no blur. The reveal unit, fade duration ratio, and an
 * optional blur strength can be configured explicitly.
 *
 * Reveal affects only the text foreground. Text decorations and other style
 * layers such as shadow, outline, underline, strikethrough, and background are
 * not affected.
 *
 * Inline replacement content does not participate in the reveal. When text is
 * elided, hidden source text does not consume reveal units and the ellipsis
 * participates in the visible reveal sequence.
 */
class DALI_UI_API Reveal
{
public:
  /**
   * @brief Selects automatic fade duration for the reveal units.
   *
   * When this value is used as the fade duration ratio, the fade duration is
   * selected automatically based on the visible reveal units.
   *
   * This value represents authored automatic behavior; GetFadeDurationRatio()
   * returns this value rather than the internally resolved duration ratio.
   */
  static constexpr float AUTO_FADE_DURATION_RATIO = -1.0f;

  /**
   * @brief Selects an automatically resolved blur strength.
   *
   * The implementation selects a perceptually suitable blur from the final
   * rendered text size. This value represents authored automatic behavior;
   * GetBlurStrength() returns this value rather than an internally resolved
   * value.
   */
  static constexpr float AUTO_BLUR_STRENGTH = -1.0f;

  /**
   * @brief Unit used to divide the visible text into reveal steps.
   */
  enum class Unit : uint8_t
  {
    /**
     * @brief Reveals visible text by character in logical text order.
     *
     * Characters that are rendered as one indivisible text unit may reveal
     * together.
     */
    CHARACTER,

    /**
     * @brief Reveals visible text by word in logical text order.
     *
     * Whitespace does not consume a reveal step. Punctuation is associated with
     * surrounding text where appropriate.
     */
    WORD
  };

  /**
   * @brief Selects how visible reveal units are grouped into sequences.
   */
  enum class Sequence : uint8_t
  {
    /**
     * @brief Uses one continuous sequence for all visible reveal units.
     */
    TEXT,

    /**
     * @brief Uses an independent sequence for each final visible layout line.
     *
     * Lines created by wrapping are separate sequences. Lines without a text
     * foreground reveal unit do not consume a sequence.
     */
    LINE
  };

public:
  /**
   * @brief Creates a CHARACTER, TEXT reveal with automatic fade duration.
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
   * @brief Sets the unit used to divide visible text into reveal steps.
   *
   * @param[in] unit The reveal unit.
   */
  void SetUnit(Unit unit);

  /**
   * @brief Returns the unit used to divide visible text into reveal steps.
   *
   * @return The reveal unit.
   */
  Unit GetUnit() const;

  /**
   * @brief Sets how visible reveal units are grouped into sequences.
   *
   * TEXT uses one continuous sequence. LINE uses each final visible layout
   * line, including a line created by wrapping, as an independent sequence.
   *
   * @param[in] sequence The reveal sequence grouping.
   */
  void SetSequence(Sequence sequence);

  /**
   * @brief Returns how visible reveal units are grouped into sequences.
   *
   * @return The reveal sequence grouping.
   */
  Sequence GetSequence() const;

  /**
   * @brief Sets the normalized ratio controlling the spacing between consecutive sequence starts.
   *
   * Sequence starts are evenly spaced. Zero starts all active sequences
   * together. Increasing values space consecutive starts further apart, and
   * one prevents consecutive active sequences from overlapping. A shorter
   * sequence may complete before the next sequence starts.
   *
   * This value has no visual effect while Sequence is TEXT, but the authored
   * value is retained. Values outside [0.0, 1.0] are clamped, and NaN is
   * normalized to 0.0.
   *
   * @param[in] ratio The normalized sequence start delay in [0.0, 1.0].
   */
  void SetSequenceStartDelayRatio(float ratio);

  /**
   * @brief Returns the authored normalized sequence start delay.
   *
   * @return The authored ratio in [0.0, 1.0].
   */
  float GetSequenceStartDelayRatio() const;

  /**
   * @brief Sets the fade duration of each reveal unit on the normalized timeline.
   *
   * AUTO_FADE_DURATION_RATIO selects an automatic fade duration based on the
   * visible reveal units.
   *
   * Values from 0.0 to 1.0 specify the duration of each unit fade. Zero produces
   * a step-wise reveal with no per-unit fade, while one makes all units fade
   * together over the full timeline.
   *
   * When TextRevealProgress is animated linearly from 0.0 to 1.0, the ratio
   * corresponds to the same fraction of the animation duration.
   *
   * Values outside [0.0, 1.0] are clamped, except AUTO_FADE_DURATION_RATIO.
   * NaN is normalized to 0.0.
   *
   * @param[in] ratio AUTO_FADE_DURATION_RATIO or a value in [0.0, 1.0].
   */
  void SetFadeDurationRatio(float ratio);

  /**
   * @brief Returns the authored fade duration ratio.
   *
   * If automatic duration is selected, this returns
   * AUTO_FADE_DURATION_RATIO rather than the duration resolved for the current
   * text layout.
   *
   * @return AUTO_FADE_DURATION_RATIO or the authored ratio in [0.0, 1.0].
   */
  float GetFadeDurationRatio() const;

  /**
   * @brief Sets the blur strength applied while reveal units fade in.
   *
   * Zero disables blur and preserves Fade-only Reveal behavior.
   * AUTO_BLUR_STRENGTH selects a blur suitable for the final rendered text.
   * Values from 0.0 to 1.0 specify increasing visual blur strength; the
   * implementation may adapt the internal preprocessing resolution and blur
   * radius to the rendered text and device scale.
   *
   * Values outside [0.0, 1.0] are clamped, except AUTO_BLUR_STRENGTH. NaN is
   * normalized to 0.0.
   *
   * @param[in] strength AUTO_BLUR_STRENGTH or a value in [0.0, 1.0].
   */
  void SetBlurStrength(float strength);

  /**
   * @brief Returns the authored blur strength.
   *
   * If automatic blur is selected, this returns AUTO_BLUR_STRENGTH rather
   * than the value resolved for the current text layout.
   *
   * @return AUTO_BLUR_STRENGTH or the authored strength in [0.0, 1.0].
   */
  float GetBlurStrength() const;

private:
  class Impl;
  Impl* mImpl{nullptr};
};

} // namespace Text
} // namespace Ui
} // namespace Dali
