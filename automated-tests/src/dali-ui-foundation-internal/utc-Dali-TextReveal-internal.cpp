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

#include <dali-ui-foundation/internal/text/controller/text-controller-impl.h>
#include <dali-ui-foundation/internal/text/controller/text-controller.h>
#include <dali-ui-foundation/internal/text/rendering/text-reveal-fade-blur-processor.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter-impl.h>
#include <dali-ui-foundation/internal/text/rendering/text-typesetter.h>
#include <dali-ui-foundation/internal/text/rendering/view-model.h>
#include <dali-ui-foundation/internal/text/replacement/replacement-run-snapshot.h>
#include <dali-ui-foundation/internal/text/reveal/text-reveal.h>
#include <dali-ui-foundation/public-api/animation/duration.h>
#include <dali-ui-foundation/public-api/animation/label-animation-bridge.autogen.h>
#include <dali-ui-foundation/public-api/text/style/reveal.h>
#include <dali-ui-foundation/public-api/text/styled-text/font-span.h>
#include <dali-ui-foundation/public-api/text/styled-text/styled-text-builder.h>
#include <dali-ui-foundation/public-api/views/text-controls/label.h>
#include <dali-ui-test-suite-utils.h>
#include <dali.h>
#include <dali/devel-api/text-abstraction/segmentation.h>
#include <dali/integration-api/pixel-data-integ.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace Dali;

namespace
{
namespace Reveal = Dali::Ui::Text::Internal::Reveal;
namespace UiText = Dali::Ui::Text;

constexpr float EPSILON = 0.0001f;

float CalculateRevealOpacityReference(float progress, float unitStart, float fadeDuration)
{
  progress = std::isnan(progress) ? 0.0f : std::max(0.0f, std::min(1.0f, progress));
  if(progress <= 0.0f)
  {
    return 0.0f;
  }
  if(progress >= 1.0f)
  {
    return 1.0f;
  }
  if(fadeDuration <= 0.000001f)
  {
    return progress >= unitStart ? 1.0f : 0.0f;
  }
  return std::max(0.0f, std::min(1.0f, (progress - unitStart) / fadeDuration));
}

UiText::ControllerPtr BuildReplacementController(const char*                                 text,
                                                 std::initializer_list<UiText::CharacterRun> ranges,
                                                 const Size&                                 size,
                                                 bool                                        elideText         = false,
                                                 float                                       replacementHeight = 24.0f)
{
  UiText::ControllerPtr     controller = UiText::Controller::New();
  UiText::Controller::Impl& impl       = UiText::Controller::Impl::GetImplementation(*controller.Get());
  controller->SetText(text);
  controller->SetDefaultFontSize(20.0f, UiText::Controller::PIXEL_SIZE);
  controller->SetTextElideEnabled(elideText);

  UiText::ReplacementSourceSnapshot source;
  uint64_t                          identity = 1u;
  for(const UiText::CharacterRun& range : ranges)
  {
    UiText::ReplacementRunSnapshot replacement;
    replacement.logicalCharacterRange = range;
    replacement.metrics.width         = 32.0f;
    replacement.metrics.height        = replacementHeight;
    replacement.type                  = UiText::ReplacementType::IMAGE;
    replacement.occurrenceIdentity    = identity++;
    replacement.image.source          = "unused.png";
    source.runs.PushBack(replacement);
  }
  source.sourceRevision                       = 1u;
  source.hasValidReplacementSource            = !source.runs.Empty();
  impl.GetOrCreateReplacementSourceSnapshot() = source;
  controller->Relayout(size);
  return controller;
}

UiText::ControllerPtr BuildReferenceMetricController(const char* text,
                                                     float       fontSize,
                                                     float       relativeLineSize,
                                                     float       uiScale,
                                                     const Size& layoutSize)
{
  UiText::ControllerPtr controller = UiText::Controller::New();
  controller->SetMultiLineEnabled(true);
  controller->SetText(text);
  controller->SetDefaultFontSize(fontSize, UiText::Controller::PIXEL_SIZE);
  controller->SetRelativeLineSize(relativeLineSize);
  controller->SetUiScale(uiScale);
  controller->Relayout(layoutSize);
  return controller;
}

UiText::ControllerPtr BuildMixedSizeReferenceMetricController(float       largeSize,
                                                              bool        heading,
                                                              const Size& layoutSize)
{
  const char*               text    = heading
                                        ? "Heading body line body line body line body line body line body line"
                                        : "small BIG small";
  UiText::StyledTextBuilder builder = UiText::StyledTextBuilder::New(text);
  UiText::FontAttributes    attributes;
  attributes.SetSize(largeSize);
  DALI_TEST_CHECK(builder.SetSpan(UiText::FontSpan::New(attributes),
                                  heading ? 0u : 6u,
                                  heading ? 7u : 9u));

  UiText::ControllerPtr controller = UiText::Controller::New();
  controller->SetDefaultFontSize(20.0f, UiText::Controller::PIXEL_SIZE);
  controller->SetStyledText(builder.Build());
  controller->SetMultiLineEnabled(true);
  controller->Relayout(layoutSize);
  return controller;
}
} // namespace

void utc_dali_text_reveal_internal_startup(void)
{
  test_return_value = TET_UNDEF;
}

void utc_dali_text_reveal_internal_cleanup(void)
{
  test_return_value = TET_PASS;
}

int UtcDaliTextRevealWordPlanP(void)
{
  const UiText::Character      text[]     = {'H', 'i', ' ', 'a', 'l', 'l'};
  const UiText::CharacterIndex glyphMap[] = {0u, 0u, 1u, 2u, 3u, 4u, 5u};
  const UiText::WordBreakInfo  breaks[]   = {
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_BREAK, TextAbstraction::WORD_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_BREAK};

  const Reveal::Plan plan = Reveal::BuildPlan(text, 6u, glyphMap, 7u, Reveal::Unit::WORD,
                                              UiText::Reveal::AUTO_FADE_DURATION_RATIO, breaks);
  DALI_TEST_EQUALS(plan.GetUnitCount(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[0], 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[1], 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[2], 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[3], Reveal::NO_UNIT, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[4], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[6], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.unitStart[0], 0.0f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.unitStart[1], 1.0f / (1.0f + std::sqrt(2.0f)), EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.fadeDuration, std::sqrt(2.0f) / (1.0f + std::sqrt(2.0f)), EPSILON, TEST_LOCATION);

  // Actual DALi segmentation exposes punctuation boundaries. WORD reveal
  // folds adjacent punctuation into the neighboring word so it consumes no
  // standalone timing unit: "Hello, (world)!" -> two units.
  const UiText::Character punctuationText[] = {
    '"', 'H', 'e', 'l', 'l', 'o', ',', ' ', '(', 'w', 'o', 'r', 'l', 'd', ')', '!'};
  const UiText::CharacterIndex punctuationMap[] = {
    0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};
  const UiText::WordBreakInfo punctuationBreaks[] = {
    TextAbstraction::WORD_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_BREAK, TextAbstraction::WORD_BREAK, TextAbstraction::WORD_BREAK,
    TextAbstraction::WORD_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_BREAK, TextAbstraction::WORD_BREAK, TextAbstraction::WORD_BREAK};
  const Reveal::Plan punctuationPlan = Reveal::BuildPlan(
    punctuationText, 16u, punctuationMap, 16u, Reveal::Unit::WORD,
    UiText::Reveal::AUTO_FADE_DURATION_RATIO, punctuationBreaks);
  DALI_TEST_EQUALS(punctuationPlan.GetUnitCount(), 2u, TEST_LOCATION);
  for(uint32_t glyph = 0u; glyph <= 6u; ++glyph)
  {
    DALI_TEST_EQUALS(punctuationPlan.glyphToUnit[glyph], 0u, TEST_LOCATION);
  }
  DALI_TEST_EQUALS(punctuationPlan.glyphToUnit[7u], Reveal::NO_UNIT, TEST_LOCATION);
  for(uint32_t glyph = 8u; glyph < 16u; ++glyph)
  {
    DALI_TEST_EQUALS(punctuationPlan.glyphToUnit[glyph], 1u, TEST_LOCATION);
  }

  // Apostrophes and numeric punctuation are already retained inside DALi
  // segmentation units and remain untouched by the post-processing.
  const UiText::Character      apostropheText[]   = {'d', 'o', 'n', '\'', 't', ' ', '1', ',', '0', '0', '0', '.', '5', '0'};
  const UiText::CharacterIndex apostropheMap[]    = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u};
  const UiText::WordBreakInfo  apostropheBreaks[] = {
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_BREAK, TextAbstraction::WORD_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_NO_BREAK, TextAbstraction::WORD_BREAK};
  const Reveal::Plan apostrophePlan = Reveal::BuildPlan(
    apostropheText, 14u, apostropheMap, 14u, Reveal::Unit::WORD,
    UiText::Reveal::AUTO_FADE_DURATION_RATIO, apostropheBreaks);
  DALI_TEST_EQUALS(apostrophePlan.GetUnitCount(), 2u, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealWordSymbolPunctuationCorpusP(void)
{
  auto CheckUnits = [&](const char*                                  source,
                        std::initializer_list<UiText::WordBreakInfo> breakFixture,
                        std::initializer_list<uint32_t>              expected)
  {
    const size_t length = std::strlen(source);
    DALI_TEST_EQUALS(length, breakFixture.size(), TEST_LOCATION);
    DALI_TEST_EQUALS(length, expected.size(), TEST_LOCATION);

    std::vector<UiText::Character>      text(length);
    std::vector<UiText::CharacterIndex> glyphMap(length);
    std::vector<UiText::WordBreakInfo>  breaks(breakFixture);
    for(size_t index = 0u; index < length; ++index)
    {
      text[index]     = static_cast<unsigned char>(source[index]);
      glyphMap[index] = static_cast<UiText::CharacterIndex>(index);
    }
    const Reveal::Plan plan  = Reveal::BuildPlan(text.data(), static_cast<UiText::Length>(length),
                                                 glyphMap.data(), static_cast<UiText::Length>(length),
                                                 Reveal::Unit::WORD, UiText::Reveal::AUTO_FADE_DURATION_RATIO, breaks.data());
    size_t             index = 0u;
    for(uint32_t unit : expected)
    {
      DALI_TEST_EQUALS(plan.glyphToUnit[index++], unit, TEST_LOCATION);
    }
  };

  constexpr auto B = TextAbstraction::WORD_BREAK;
  constexpr auto N = TextAbstraction::WORD_NO_BREAK;
  // These fixtures are the output of the production DALi segmentation engine
  // for this corpus. (The UTC text-abstraction stub intentionally does not run
  // the production segmenter.) Punctuation folds into an adjacent word, while
  // currency, math and general symbols retain the segmenter's boundaries.
  CheckUnits("$100", {B, N, N, B}, {0u, 1u, 1u, 1u});
  CheckUnits("A+B", {B, B, B}, {0u, 1u, 2u});
  CheckUnits("x=10", {B, B, N, B}, {0u, 1u, 2u, 2u});
  CheckUnits("foo/bar", {N, N, B, B, N, N, B}, {0u, 0u, 0u, 0u, 1u, 1u, 1u});
  CheckUnits("a|b", {B, B, B}, {0u, 1u, 2u});
  CheckUnits("~test", {B, N, N, N, B}, {0u, 1u, 1u, 1u, 1u});
  CheckUnits("100%", {N, N, B, B}, {0u, 0u, 0u, 0u});
  CheckUnits("email@example.com", {N, N, N, N, B, B, N, N, N, N, N, N, N, N, N, N, B},
             {0u, 0u, 0u, 0u, 0u, 0u,
              1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u});
  CheckUnits("C++", {B, B, B}, {0u, 1u, 2u});
  END_TEST;
}

int UtcDaliTextRevealFadeDurationRatioScheduleP(void)
{
  const uint32_t autoUnitCounts[] = {1u, 2u, 5u, 10u, 20u, 50u, 100u, 200u};
  for(uint32_t unitCount : autoUnitCounts)
  {
    std::vector<UiText::Character>      text(unitCount, static_cast<UiText::Character>('A'));
    std::vector<UiText::CharacterIndex> glyphMap(unitCount);
    for(uint32_t index = 0u; index < unitCount; ++index)
    {
      glyphMap[index] = index;
    }

    const Reveal::Plan automatic   = Reveal::BuildPlan(text.data(), unitCount, glyphMap.data(), unitCount,
                                                       Reveal::Unit::CHARACTER,
                                                       UiText::Reveal::AUTO_FADE_DURATION_RATIO);
    const float        fadeSpan    = std::min(10.0f, std::max(1.0f, std::sqrt(static_cast<float>(unitCount))));
    const float        denominator = static_cast<float>(unitCount - 1u) + fadeSpan;
    DALI_TEST_EQUALS(automatic.fadeDurationRatio, UiText::Reveal::AUTO_FADE_DURATION_RATIO, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(automatic.fadeDuration, fadeSpan / denominator, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(automatic.unitStart.back() + automatic.fadeDuration, 1.0f, EPSILON, TEST_LOCATION);
    if(unitCount > 1u)
    {
      DALI_TEST_EQUALS(automatic.unitStart[1] - automatic.unitStart[0], 1.0f / denominator, EPSILON, TEST_LOCATION);
      DALI_TEST_CHECK(automatic.fadeDuration > automatic.unitStart[1]);
    }
  }

  const uint32_t explicitUnitCounts[] = {1u, 2u, 5u, 20u, 100u};
  const float    ratios[]             = {0.0f, 0.05f, 0.1f, 0.2f, 0.5f, 1.0f};
  for(uint32_t unitCount : explicitUnitCounts)
  {
    std::vector<UiText::Character>      text(unitCount, static_cast<UiText::Character>('A'));
    std::vector<UiText::CharacterIndex> glyphMap(unitCount);
    for(uint32_t index = 0u; index < unitCount; ++index)
    {
      glyphMap[index] = index;
    }

    for(float ratio : ratios)
    {
      const Reveal::Plan plan = Reveal::BuildPlan(text.data(), unitCount, glyphMap.data(), unitCount,
                                                  Reveal::Unit::CHARACTER, ratio);
      DALI_TEST_EQUALS(plan.fadeDurationRatio, ratio, EPSILON, TEST_LOCATION);
      DALI_TEST_EQUALS(plan.fadeDuration, ratio, EPSILON, TEST_LOCATION);
      DALI_TEST_EQUALS(plan.unitStart.front(), 0.0f, EPSILON, TEST_LOCATION);
      if(unitCount == 1u)
      {
        DALI_TEST_EQUALS(plan.unitStart.front(), 0.0f, EPSILON, TEST_LOCATION);
      }
      else
      {
        const float expectedInterval = (1.0f - ratio) / static_cast<float>(unitCount - 1u);
        DALI_TEST_EQUALS(plan.unitStart.back() + plan.fadeDuration, 1.0f, EPSILON, TEST_LOCATION);
        for(uint32_t unit = 1u; unit < unitCount; ++unit)
        {
          DALI_TEST_EQUALS(plan.unitStart[unit] - plan.unitStart[unit - 1u], expectedInterval, EPSILON, TEST_LOCATION);
        }
      }

      if(ratio == 0.0f)
      {
        DALI_TEST_EQUALS(plan.fadeDuration, 0.0f, EPSILON, TEST_LOCATION);
        DALI_TEST_EQUALS(CalculateRevealOpacityReference(0.0f, plan.unitStart.front(), plan.fadeDuration),
                         0.0f, EPSILON, TEST_LOCATION);
        DALI_TEST_EQUALS(CalculateRevealOpacityReference(0.0001f, plan.unitStart.front(), plan.fadeDuration),
                         1.0f, EPSILON, TEST_LOCATION);
      }
      if(ratio == 1.0f)
      {
        for(float start : plan.unitStart)
        {
          DALI_TEST_EQUALS(start, 0.0f, EPSILON, TEST_LOCATION);
        }
      }
    }
  }

  // Unit selection changes ownership only; the same final unit count and
  // authored ratio produce the same schedule for CHARACTER and WORD.
  const UiText::Character      wordText[]   = {'A', ' ', 'B', ' ', 'C', ' ', 'D', ' ', 'E'};
  const UiText::CharacterIndex wordMap[]    = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  const UiText::WordBreakInfo  wordBreaks[] = {
    TextAbstraction::WORD_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_BREAK, TextAbstraction::WORD_NO_BREAK,
    TextAbstraction::WORD_BREAK};
  const Reveal::Plan words = Reveal::BuildPlan(wordText, 9u, wordMap, 9u, Reveal::Unit::WORD, 0.2f, wordBreaks);
  DALI_TEST_EQUALS(words.GetUnitCount(), 5u, TEST_LOCATION);
  DALI_TEST_EQUALS(words.fadeDuration, 0.2f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(words.unitStart.back() + words.fadeDuration, 1.0f, EPSILON, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealCharacterPlanP(void)
{
  // Synthetic shaping map: ffi and e+combining acute share cluster starts.
  const UiText::Character      text[]     = {'o', 'f', 'f', 'i', 'c', 'e', ' ', 'e', 0x0301u};
  const UiText::CharacterIndex glyphMap[] = {0u, 1u, 4u, 5u, 6u, 7u, 7u};
  const Reveal::Plan           plan       = Reveal::BuildPlan(text, 9u, glyphMap, 7u,
                                                              Reveal::Unit::CHARACTER, UiText::Reveal::AUTO_FADE_DURATION_RATIO);

  DALI_TEST_EQUALS(plan.GetUnitCount(), 5u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[0], 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[1], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[2], 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[3], 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[4], Reveal::NO_UNIT, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[5], 4u, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[6], 4u, TEST_LOCATION);

  // Visual glyph order may be reversed by bidi layout; unit timing remains in
  // logical character order while each visual glyph keeps the correct unit.
  const UiText::Character      bidiText[]       = {'A', 'B', ' ', 0x05D0u, 0x05D1u};
  const UiText::CharacterIndex visualGlyphMap[] = {4u, 3u, 2u, 1u, 0u};
  const Reveal::Plan           bidiPlan         = Reveal::BuildPlan(bidiText, 5u, visualGlyphMap, 5u,
                                                                    Reveal::Unit::CHARACTER, UiText::Reveal::AUTO_FADE_DURATION_RATIO);
  DALI_TEST_EQUALS(bidiPlan.GetUnitCount(), 4u, TEST_LOCATION);
  DALI_TEST_EQUALS(bidiPlan.glyphToUnit[0], 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(bidiPlan.glyphToUnit[1], 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(bidiPlan.glyphToUnit[2], Reveal::NO_UNIT, TEST_LOCATION);
  DALI_TEST_EQUALS(bidiPlan.glyphToUnit[3], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(bidiPlan.glyphToUnit[4], 0u, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealSyntheticReplacementDoesNotConsumeTimingP(void)
{
  UiTestApplication application;

  auto BuildPlan = [](const UiText::ModelInterface& model, Reveal::Unit unit, float ratio)
  {
    if(unit == Reveal::Unit::WORD)
    {
      TextAbstraction::Segmentation segmentation = TextAbstraction::Segmentation::New();
      return Reveal::BuildPlan(model, unit, ratio, segmentation);
    }
    return Reveal::BuildCharacterPlan(model, ratio);
  };

  auto CheckNoDeadUnits = [](const UiText::ModelInterface& model, const Reveal::Plan& plan)
  {
    const UiText::GlyphInfo* glyphs = model.GetGlyphs();
    DALI_TEST_CHECK(glyphs);
    DALI_TEST_EQUALS(plan.glyphToUnit.size(), static_cast<size_t>(model.GetNumberOfGlyphs()), TEST_LOCATION);
    std::vector<bool> hasOrdinaryGlyph(plan.GetUnitCount(), false);
    for(UiText::GlyphIndex glyph = 0u; glyph < model.GetNumberOfGlyphs(); ++glyph)
    {
      const uint32_t unit = plan.glyphToUnit[glyph];
      if(UiText::IsSyntheticReplacementGlyph(glyphs[glyph]))
      {
        DALI_TEST_EQUALS(unit, Reveal::NO_UNIT, TEST_LOCATION);
      }
      else if(unit != Reveal::NO_UNIT)
      {
        DALI_TEST_CHECK(unit < hasOrdinaryGlyph.size());
        hasOrdinaryGlyph[unit] = true;
      }
    }
    for(bool populated : hasOrdinaryGlyph)
    {
      DALI_TEST_CHECK(populated);
    }
  };

  // This is the processing model produced for "A [ImageSpan] B": the logical
  // image range is replaced by one layout-only synthetic glyph.
  UiText::ControllerPtr primary = BuildReplacementController(
    "A icon B", {{2u, 4u}}, Size(320.0f, 80.0f));
  const UiText::ModelInterface* primaryModel = primary->GetRenderTextModel();
  DALI_TEST_CHECK(primaryModel && primaryModel != primary->GetLogicalTextModel());
  for(Reveal::Unit unit : {Reveal::Unit::CHARACTER, Reveal::Unit::WORD})
  {
    const Reveal::Plan automatic = BuildPlan(*primaryModel, unit, UiText::Reveal::AUTO_FADE_DURATION_RATIO);
    CheckNoDeadUnits(*primaryModel, automatic);
    DALI_TEST_EQUALS(automatic.GetUnitCount(), 2u, TEST_LOCATION);
    DALI_TEST_EQUALS(automatic.fadeDurationRatio, UiText::Reveal::AUTO_FADE_DURATION_RATIO, EPSILON, TEST_LOCATION);
    const float fadeSpan = std::sqrt(2.0f);
    DALI_TEST_EQUALS(automatic.fadeDuration, fadeSpan / (1.0f + fadeSpan), EPSILON, TEST_LOCATION);

    const Reveal::Plan explicitRatio = BuildPlan(*primaryModel, unit, 0.2f);
    CheckNoDeadUnits(*primaryModel, explicitRatio);
    DALI_TEST_EQUALS(explicitRatio.GetUnitCount(), 2u, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitRatio.fadeDurationRatio, 0.2f, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitRatio.fadeDuration, 0.2f, EPSILON, TEST_LOCATION);
  }

  // Multiple image replacements cover start, middle and end. Units containing
  // remaining ordinary glyphs survive, while replacement-only units vanish.
  UiText::ControllerPtr multiple = BuildReplacementController(
    "XaYbZ", {{0u, 1u}, {2u, 1u}, {4u, 1u}}, Size(320.0f, 80.0f));
  const UiText::ModelInterface* multipleModel = multiple->GetRenderTextModel();
  DALI_TEST_CHECK(multipleModel);
  const Reveal::Plan multipleCharacters = BuildPlan(*multipleModel, Reveal::Unit::CHARACTER, 0.2f);
  const Reveal::Plan multipleWords      = BuildPlan(*multipleModel, Reveal::Unit::WORD, 0.2f);
  CheckNoDeadUnits(*multipleModel, multipleCharacters);
  CheckNoDeadUnits(*multipleModel, multipleWords);
  DALI_TEST_EQUALS(multipleCharacters.GetUnitCount(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(multipleWords.GetUnitCount(), 1u, TEST_LOCATION);

  // Logical unit ordering must remain independent of bidi visual traversal.
  UiText::ControllerPtr bidi = BuildReplacementController(
    "\xD7\x90X\xD7\x91", {{1u, 1u}}, Size(320.0f, 80.0f));
  const UiText::ModelInterface* bidiModel = bidi->GetRenderTextModel();
  DALI_TEST_CHECK(bidiModel);
  for(Reveal::Unit unit : {Reveal::Unit::CHARACTER, Reveal::Unit::WORD})
  {
    const Reveal::Plan bidiPlan = BuildPlan(*bidiModel, unit, 0.2f);
    CheckNoDeadUnits(*bidiModel, bidiPlan);
    if(unit == Reveal::Unit::CHARACTER)
    {
      const auto& glyphMap = bidiModel->GetGlyphsToCharacters();
      std::vector<std::pair<UiText::CharacterIndex, uint32_t>> logicalUnits;
      for(UiText::GlyphIndex glyph = 0u; glyph < bidiModel->GetNumberOfGlyphs(); ++glyph)
      {
        if(bidiPlan.glyphToUnit[glyph] != Reveal::NO_UNIT)
        {
          logicalUnits.emplace_back(glyphMap[glyph], bidiPlan.glyphToUnit[glyph]);
        }
      }
      std::sort(logicalUnits.begin(), logicalUnits.end());
      DALI_TEST_CHECK(!logicalUnits.empty());
      DALI_TEST_EQUALS(logicalUnits.front().second, 0u, TEST_LOCATION);
      for(size_t index = 1u; index < logicalUnits.size(); ++index)
      {
        DALI_TEST_CHECK(logicalUnits[index - 1u].second <= logicalUnits[index].second);
      }
      DALI_TEST_EQUALS(logicalUnits.back().second, bidiPlan.GetUnitCount() - 1u, TEST_LOCATION);
    }
  }

  // END ellipsis may retain or remove the image box, but the final Reveal
  // schedule must be backed only by ordinary text or the generated ellipsis.
  UiText::ControllerPtr elided = BuildReplacementController(
    "A icon B C D E F G H I J K L M N O P", {{2u, 4u}}, Size(320.0f, 80.0f), true);
  const Vector3 elidedNaturalSize = elided->GetNaturalSize(false);
  DALI_TEST_CHECK(elidedNaturalSize.x > 0.0f && elidedNaturalSize.y > 0.0f);
  const Vector2 elidedLayoutSize(elidedNaturalSize.x * 0.5f, elidedNaturalSize.y + 1.0f);
  elided->Relayout(elidedLayoutSize);
  const UiText::ModelInterface* elidedModel = elided->GetRenderTextModel();
  DALI_TEST_CHECK(elidedModel);
  UiText::TypesetterPtr typesetter = UiText::Typesetter::New(elidedModel);
  typesetter->SetFinalElisionResult(elided->GetFinalElisionResult());
  for(Reveal::Unit unit : {Reveal::Unit::CHARACTER, Reveal::Unit::WORD})
  {
    const Reveal::Plan source = BuildPlan(*elidedModel, unit, UiText::Reveal::AUTO_FADE_DURATION_RATIO);
    CheckNoDeadUnits(*elidedModel, source);
    const Reveal::Plan finalPlan = typesetter->CreateFinalRevealPlan(source, unit);
    UiText::ViewModel* viewModel = typesetter->GetViewModel();
    DALI_TEST_CHECK(viewModel);
    const UiText::GlyphIndex ellipsisGlyph = viewModel->GetEllipsisFinalGlyphIndex();
    DALI_TEST_CHECK(ellipsisGlyph < finalPlan.glyphToUnit.size());

    const UiText::GlyphInfo* finalGlyphs = viewModel->GetGlyphs();
    DALI_TEST_CHECK(finalGlyphs);
    std::vector<bool> hasRenderableGlyph(finalPlan.GetUnitCount(), false);
    for(UiText::GlyphIndex glyph = 0u; glyph < finalPlan.glyphToUnit.size(); ++glyph)
    {
      const uint32_t revealUnit = finalPlan.glyphToUnit[glyph];
      if(UiText::IsSyntheticReplacementGlyph(finalGlyphs[glyph]))
      {
        DALI_TEST_EQUALS(revealUnit, Reveal::NO_UNIT, TEST_LOCATION);
      }
      else if(revealUnit != Reveal::NO_UNIT)
      {
        hasRenderableGlyph[revealUnit] = true;
      }
    }
    for(bool populated : hasRenderableGlyph)
    {
      DALI_TEST_CHECK(populated);
    }
  }
  END_TEST;
}

int UtcDaliTextRevealProgressEndpointsP(void)
{
  const uint32_t unitCounts[]         = {1u, 2u, 7u, 257u};
  const float    fadeDurationRatios[] = {0.0f, UiText::Reveal::AUTO_FADE_DURATION_RATIO};
  for(uint32_t unitCount : unitCounts)
  {
    std::vector<UiText::Character>      text(unitCount, static_cast<UiText::Character>('A'));
    std::vector<UiText::CharacterIndex> glyphMap(unitCount);
    for(uint32_t index = 0u; index < unitCount; ++index)
    {
      glyphMap[index] = index;
    }

    for(float fadeDurationRatio : fadeDurationRatios)
    {
      const Reveal::Plan plan = Reveal::BuildPlan(text.data(), unitCount, glyphMap.data(), unitCount,
                                                  Reveal::Unit::CHARACTER, fadeDurationRatio);
      for(uint32_t unit = 0u; unit < plan.GetUnitCount(); ++unit)
      {
        // Metadata encodes start in two 8-bit channels. Decode that exact
        // representation before checking the CPU/shader endpoint contract.
        const uint32_t encodedStart = static_cast<uint32_t>(
          std::round(std::max(0.0f, std::min(1.0f, plan.unitStart[unit])) * 65535.0f));
        const float decodedStart = static_cast<float>(encodedStart) / 65535.0f;
        DALI_TEST_CHECK(CalculateRevealOpacityReference(0.0f, decodedStart, plan.fadeDuration) == 0.0f);
        DALI_TEST_CHECK(CalculateRevealOpacityReference(1.0f, decodedStart, plan.fadeDuration) == 1.0f);
      }
    }
  }
  END_TEST;
}

int UtcDaliTextRevealLabelAnimationNoRerasterP(void)
{
  UiTestApplication application;
  Dali::Ui::Label   label = Dali::Ui::Label::New("Hello sequential reveal world");
  label.SetProperty(Actor::Property::SIZE, Vector3(420.0f, 80.0f, 0.0f));

  application.GetScene().Add(label);
  application.SendNotification();
  application.Render(16);
  DALI_TEST_EQUALS(label.GetPropertyIndex("uTextRevealProgress"), Property::INVALID_INDEX, TEST_LOCATION);
  DALI_TEST_EQUALS(label.GetRendererAt(0u).GetTextures().GetTextureCount(), 1u, TEST_LOCATION);

  UiText::Reveal reveal;
  reveal.SetUnit(UiText::Reveal::Unit::WORD);
  label.SetTextReveal(reveal);
  label.SetTextRevealProgress(0.0f);
  application.SendNotification();
  application.Render(16);
  application.SendNotification();
  application.Render(16);

  DALI_TEST_CHECK(label.GetRendererCount() > 0u);
  Renderer   renderer = label.GetRendererAt(0u);
  TextureSet textures = renderer.GetTextures();
  DALI_TEST_CHECK(renderer.GetPropertyIndex("uTextRevealProgress") != Property::INVALID_INDEX);
  DALI_TEST_CHECK(renderer.GetPropertyIndex("uTextRevealFadeDuration") != Property::INVALID_INDEX);
  DALI_TEST_CHECK(textures.GetTextureCount() >= 2u);
  Texture metadata = textures.GetTexture(textures.GetTextureCount() - 1u);

  TestGlAbstraction& gl = application.GetGlAbstraction();
  gl.EnableTextureCallTrace(true);
  gl.ResetTextureCallStack();

  Animation animation = Animation::New(0.20f);
  label.Animate(animation).TextRevealProgress(1.0f, Dali::Ui::Duration(0.20f));
  animation.Play();
  for(uint32_t frame = 0u; frame < 8u; ++frame)
  {
    application.SendNotification();
    application.Render(32);
    DALI_TEST_CHECK(label.GetRendererAt(0u) == renderer);
    TextureSet currentTextures = label.GetRendererAt(0u).GetTextures();
    DALI_TEST_CHECK(currentTextures.GetTexture(currentTextures.GetTextureCount() - 1u) == metadata);
  }
  DALI_TEST_EQUALS(label.GetCurrentProperty<float>(label.GetPropertyIndex("uTextRevealProgress")),
                   1.0f, EPSILON, TEST_LOCATION);

  Animation reverse = Animation::New(0.20f);
  label.Animate(reverse).TextRevealProgress(0.0f, Dali::Ui::Duration(0.20f));
  reverse.Play();
  for(uint32_t frame = 0u; frame < 8u; ++frame)
  {
    application.SendNotification();
    application.Render(32);
    DALI_TEST_CHECK(label.GetRendererAt(0u) == renderer);
    TextureSet currentTextures = label.GetRendererAt(0u).GetTextures();
    DALI_TEST_CHECK(currentTextures.GetTexture(currentTextures.GetTextureCount() - 1u) == metadata);
  }
  DALI_TEST_EQUALS(label.GetCurrentProperty<float>(label.GetPropertyIndex("uTextRevealProgress")),
                   0.0f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(gl.GetTextureTrace().CountMethod("TexImage2D"), 0, TEST_LOCATION);
  DALI_TEST_EQUALS(gl.GetTextureTrace().CountMethod("TexSubImage2D"), 0, TEST_LOCATION);
  DALI_TEST_EQUALS(gl.GetTextureTrace().CountMethod("CompressedTexImage2D"), 0, TEST_LOCATION);
  DALI_TEST_EQUALS(gl.GetTextureTrace().CountMethod("CompressedTexSubImage2D"), 0, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealWhitespaceAndPunctuationP(void)
{
  const UiText::Character      text[]     = {'H', 'i', ' ', ' ', '\n', '!', ' ', 'A'};
  const UiText::CharacterIndex glyphMap[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u};
  const Reveal::Plan           plan       = Reveal::BuildPlan(text, 8u, glyphMap, 8u,
                                                              Reveal::Unit::CHARACTER, UiText::Reveal::AUTO_FADE_DURATION_RATIO);
  DALI_TEST_EQUALS(plan.GetUnitCount(), 4u, TEST_LOCATION); // H, i, !, A
  DALI_TEST_EQUALS(plan.glyphToUnit[2], Reveal::NO_UNIT, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[3], Reveal::NO_UNIT, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[4], Reveal::NO_UNIT, TEST_LOCATION);
  DALI_TEST_EQUALS(plan.glyphToUnit[6], Reveal::NO_UNIT, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealFinalElisionProjectionP(void)
{
  Reveal::Plan source;
  source.glyphToUnit         = {0u, 1u, 2u, 3u, 4u};
  source.unitStart           = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f};
  source.fadeDurationRatio   = UiText::Reveal::AUTO_FADE_DURATION_RATIO;
  const float sourceFadeSpan = std::sqrt(5.0f);
  source.fadeDuration        = sourceFadeSpan / (4.0f + sourceFadeSpan);

  // Only source units 1 and 4 survive, in visual bidi order. Projection
  // compacts them in logical order and appends the generated END ellipsis.
  const UiText::GlyphIndex invalid     = std::numeric_limits<UiText::GlyphIndex>::max();
  const UiText::GlyphIndex endMap[]    = {4u, 1u, invalid};

  const Reveal::Plan character = Reveal::ProjectToFinalGlyphs(
    source, 3u, endMap, 2u, Reveal::Unit::CHARACTER);
  DALI_TEST_EQUALS(character.GetUnitCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(character.glyphToUnit[0], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(character.glyphToUnit[1], 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(character.glyphToUnit[2], 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(character.unitStart.back() + character.fadeDuration, 1.0f, EPSILON, TEST_LOCATION);

  const Reveal::Plan word = Reveal::ProjectToFinalGlyphs(
    source, 3u, endMap, 2u, Reveal::Unit::WORD);
  DALI_TEST_EQUALS(word.GetUnitCount(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(word.glyphToUnit[0], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(word.glyphToUnit[1], 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(word.glyphToUnit[2], 1u, TEST_LOCATION);
  const float wordFadeSpan = std::sqrt(2.0f);
  DALI_TEST_EQUALS(word.fadeDurationRatio, UiText::Reveal::AUTO_FADE_DURATION_RATIO, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(word.fadeDuration, wordFadeSpan / (1.0f + wordFadeSpan), EPSILON, TEST_LOCATION);
  DALI_TEST_CHECK(std::fabs(word.fadeDuration - source.fadeDuration) > EPSILON);

  Reveal::Plan explicitSource      = source;
  explicitSource.fadeDurationRatio = 0.2f;
  const Reveal::Plan explicitWord  = Reveal::ProjectToFinalGlyphs(
    explicitSource, 3u, endMap, 2u, Reveal::Unit::WORD);
  DALI_TEST_EQUALS(explicitWord.GetUnitCount(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitWord.fadeDurationRatio, 0.2f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitWord.fadeDuration, 0.2f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitWord.unitStart[1] - explicitWord.unitStart[0], 0.8f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitWord.unitStart.back() + explicitWord.fadeDuration, 1.0f, EPSILON, TEST_LOCATION);

  const UiText::GlyphIndex noEllipsisMap[] = {4u, 1u};
  const Reveal::Plan       noEllipsis      = Reveal::ProjectToFinalGlyphs(
    source, 2u, noEllipsisMap, invalid, Reveal::Unit::CHARACTER);
  DALI_TEST_EQUALS(noEllipsis.GetUnitCount(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(noEllipsis.glyphToUnit[0], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(noEllipsis.glyphToUnit[1], 0u, TEST_LOCATION);

  const UiText::GlyphIndex onlyMap[] = {invalid};
  for(Reveal::Unit unit : {Reveal::Unit::CHARACTER, Reveal::Unit::WORD})
  {
    const Reveal::Plan only = Reveal::ProjectToFinalGlyphs(
      source, 1u, onlyMap, 0u, unit);
    DALI_TEST_EQUALS(only.GetUnitCount(), 1u, TEST_LOCATION);
    DALI_TEST_EQUALS(only.glyphToUnit[0], 0u, TEST_LOCATION);
  }

  END_TEST;
}

int UtcDaliTextRevealBidiVisibleUnitOrderingP(void)
{
  Reveal::Plan source;
  source.glyphToUnit       = {3u, 2u, Reveal::NO_UNIT, 1u, 0u};
  source.unitStart         = {0.0f, 0.25f, 0.5f, 0.75f};
  source.fadeDurationRatio = 0.2f;

  const UiText::GlyphIndex mapping[] = {0u, 1u, 3u};
  const Reveal::Plan       projected = Reveal::ProjectToFinalGlyphs(
    source, 3u, mapping, UiText::FinalElisionResult::INVALID_GLYPH_INDEX,
    Reveal::Unit::CHARACTER);
  DALI_TEST_EQUALS(projected.GetUnitCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(projected.glyphToUnit[0], 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(projected.glyphToUnit[1], 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(projected.glyphToUnit[2], 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(projected.fadeDuration, 0.2f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(projected.unitStart.back(), 0.8f, EPSILON, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealNonFiniteProgressP(void)
{
  const UiText::Character      text[]     = {'A'};
  const UiText::CharacterIndex glyphMap[] = {0u};

  const Reveal::Plan fadePlan = Reveal::BuildPlan(
    text, 1u, glyphMap, 1u, Reveal::Unit::CHARACTER, UiText::Reveal::AUTO_FADE_DURATION_RATIO);
  DALI_TEST_EQUALS(fadePlan.fadeDurationRatio, UiText::Reveal::AUTO_FADE_DURATION_RATIO, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(CalculateRevealOpacityReference(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.5f),
                   0.0f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(CalculateRevealOpacityReference(std::numeric_limits<float>::infinity(), 0.0f, 0.5f),
                   1.0f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(CalculateRevealOpacityReference(-std::numeric_limits<float>::infinity(), 0.0f, 0.5f),
                   0.0f, EPSILON, TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealEllipsisMetadataP(void)
{
  UiTestApplication     application;
  UiText::ControllerPtr controller = UiText::Controller::New();
  controller->SetText("Alpha Beta Gamma Delta Epsilon Zeta Eta Theta Iota Kappa Lambda");
  controller->SetDefaultFontSize(24.0f, UiText::Controller::PIXEL_SIZE);
  controller->SetTextElideEnabled(true);
  const Vector3 naturalSize = controller->GetNaturalSize(false);
  DALI_TEST_CHECK(naturalSize.x > 0.0f && naturalSize.y > 0.0f);
  const Vector2 layoutSize(naturalSize.x * 0.5f, naturalSize.y + 1.0f);
  controller->Relayout(layoutSize);

  const UiText::ModelInterface* model = controller->GetRenderTextModel();
  DALI_TEST_CHECK(model);
  const UiText::FinalElisionResult* finalElision = controller->GetFinalElisionResult();
  DALI_TEST_CHECK(finalElision);
  DALI_TEST_CHECK(finalElision->ellipsisFinalGlyphIndex < finalElision->glyphs.Count());
  DALI_TEST_EQUALS(finalElision->finalToSourceGlyphIndices[finalElision->ellipsisFinalGlyphIndex],
                   UiText::FinalElisionResult::INVALID_GLYPH_INDEX,
                   TEST_LOCATION);

  for(Reveal::Unit unit : {Reveal::Unit::CHARACTER, Reveal::Unit::WORD})
  {
    Reveal::Plan source;
    if(unit == Reveal::Unit::WORD)
    {
      TextAbstraction::Segmentation segmentation = TextAbstraction::Segmentation::New();
      source                                     = Reveal::BuildPlan(*model, unit, UiText::Reveal::AUTO_FADE_DURATION_RATIO, segmentation);
    }
    else
    {
      source = Reveal::BuildCharacterPlan(*model, UiText::Reveal::AUTO_FADE_DURATION_RATIO);
    }

    UiText::TypesetterPtr typesetter = UiText::Typesetter::New(model);
    typesetter->SetFinalElisionResult(finalElision);
    const Reveal::Plan    finalPlan  = typesetter->CreateFinalRevealPlan(source, unit);
    UiText::ViewModel*    viewModel  = typesetter->GetViewModel();
    DALI_TEST_CHECK(viewModel);
    const UiText::GlyphIndex ellipsisGlyph = viewModel->GetEllipsisFinalGlyphIndex();
    DALI_TEST_CHECK(ellipsisGlyph < finalPlan.glyphToUnit.size());
    DALI_TEST_CHECK(viewModel->GetFinalToSourceGlyphIndices());
    DALI_TEST_EQUALS(viewModel->GetFinalToSourceGlyphIndices()[ellipsisGlyph],
                     UiText::FinalElisionResult::INVALID_GLYPH_INDEX,
                     TEST_LOCATION);
    DALI_TEST_CHECK(finalPlan.glyphToUnit[ellipsisGlyph] != Reveal::NO_UNIT);
    DALI_TEST_CHECK(finalPlan.GetUnitCount() > 0u);
    DALI_TEST_CHECK(finalPlan.GetUnitCount() < source.GetUnitCount());

    float     fadeDuration = 0.0f;
    PixelData metadata     = typesetter->RenderTextRevealMetadata(
      layoutSize, UiText::Direction::LEFT_TO_RIGHT,
      finalPlan, fadeDuration);
    DALI_TEST_CHECK(metadata);
    DALI_TEST_EQUALS(fadeDuration, finalPlan.fadeDuration, EPSILON, TEST_LOCATION);

    const uint32_t ellipsisUnit  = finalPlan.glyphToUnit[ellipsisGlyph];
    const uint32_t ellipsisStart = static_cast<uint32_t>(
      std::round(finalPlan.unitStart[ellipsisUnit] * 65535.0f));
    const Dali::Integration::PixelDataBuffer pixels = Dali::Integration::GetPixelDataBuffer(metadata);
    DALI_TEST_CHECK(pixels.buffer);
    bool foundEllipsisTiming = false;
    for(uint32_t y = 0u; y < metadata.GetHeight() && !foundEllipsisTiming; ++y)
    {
      const uint8_t* row = pixels.buffer + y * metadata.GetStrideBytes();
      for(uint32_t x = 0u; x < metadata.GetWidth(); ++x)
      {
        const uint8_t* pixel = row + x * 4u;
        if(pixel[2] != 0u)
        {
          const uint32_t encoded = static_cast<uint32_t>(pixel[0]) * 256u + pixel[1];
          foundEllipsisTiming |= encoded == ellipsisStart;
        }
      }
    }
    DALI_TEST_CHECK(foundEllipsisTiming);
    DALI_TEST_EQUALS(CalculateRevealOpacityReference(0.0f, finalPlan.unitStart[ellipsisUnit], fadeDuration),
                     0.0f, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(CalculateRevealOpacityReference(1.0f, finalPlan.unitStart[ellipsisUnit], fadeDuration),
                     1.0f, EPSILON, TEST_LOCATION);

    // Final mapping is a one-shot reveal projection request. A subsequent
    // ordinary render must consume no mapping storage.
    PixelData ordinary = typesetter->Render(layoutSize, UiText::Direction::LEFT_TO_RIGHT,
                                            UiText::Typesetter::RENDER_NO_STYLES);
    DALI_TEST_CHECK(ordinary);
    DALI_TEST_CHECK(viewModel->GetFinalToSourceGlyphIndices() == nullptr);
  }
  END_TEST;
}

int UtcDaliTextRevealTileBoundaryMetadataP(void)
{
  UiTestApplication application;
  const Vector2     fullSize(180.0f, 160.0f);

  UiText::ControllerPtr controller = UiText::Controller::New();
  // Stable ASCII ascenders and descenders exercise a boundary crossing without
  // relying on emoji fallback or font-specific italic overhang.
  controller->SetText("Agjpqy Agjpqy Agjpqy Agjpqy Agjpqy");
  controller->SetDefaultFontSize(40.0f, UiText::Controller::PIXEL_SIZE);
  controller->SetMultiLineEnabled(true);
  controller->Relayout(fullSize);

  const UiText::ModelInterface* model = controller->GetRenderTextModel();
  DALI_TEST_CHECK(model);
  UiText::TypesetterPtr typesetter = UiText::Typesetter::New(model);
  const Reveal::Plan    sourcePlan = Reveal::BuildCharacterPlan(*model, UiText::Reveal::AUTO_FADE_DURATION_RATIO);
  const Reveal::Plan    finalPlan  = typesetter->CreateFinalRevealPlan(sourcePlan, Reveal::Unit::CHARACTER);

  float     fadeDuration = 0.0f;
  PixelData full         = typesetter->RenderTextRevealMetadata(
    fullSize, UiText::Direction::LEFT_TO_RIGHT, finalPlan, fadeDuration);
  DALI_TEST_CHECK(full);
  const Dali::Integration::PixelDataBuffer fullPixels = Dali::Integration::GetPixelDataBuffer(full);
  DALI_TEST_CHECK(fullPixels.buffer);

  // Pick a boundary that cuts through actual glyph coverage. A correct tiled
  // traversal must then reproduce the two adjacent full-texture rows exactly.
  uint32_t boundary = 0u;
  for(uint32_t y = 1u; y < full.GetHeight() && boundary == 0u; ++y)
  {
    const uint8_t* previous = fullPixels.buffer + (y - 1u) * full.GetStrideBytes();
    const uint8_t* current  = fullPixels.buffer + y * full.GetStrideBytes();
    for(uint32_t x = 0u; x < full.GetWidth(); ++x)
    {
      if(previous[x * 4u + 2u] != 0u && current[x * 4u + 2u] != 0u)
      {
        boundary = y;
        break;
      }
    }
  }
  DALI_TEST_CHECK(boundary > 0u && boundary < full.GetHeight());

  PixelData upper = typesetter->RenderTextRevealMetadata(
    Vector2(fullSize.width, static_cast<float>(boundary)),
    UiText::Direction::LEFT_TO_RIGHT, finalPlan, fadeDuration, 0u, fullSize);
  PixelData lower = typesetter->RenderTextRevealMetadata(
    Vector2(fullSize.width, fullSize.height - static_cast<float>(boundary)),
    UiText::Direction::LEFT_TO_RIGHT, finalPlan, fadeDuration, boundary, fullSize);
  DALI_TEST_CHECK(upper);
  DALI_TEST_CHECK(lower);

  const Dali::Integration::PixelDataBuffer upperPixels = Dali::Integration::GetPixelDataBuffer(upper);
  const Dali::Integration::PixelDataBuffer lowerPixels = Dali::Integration::GetPixelDataBuffer(lower);
  const size_t                             rowBytes    = static_cast<size_t>(full.GetWidth()) * 4u;
  for(uint32_t y = 0u; y < boundary; ++y)
  {
    DALI_TEST_EQUALS(std::memcmp(fullPixels.buffer + y * full.GetStrideBytes(),
                                 upperPixels.buffer + y * upper.GetStrideBytes(),
                                 rowBytes),
                     0,
                     TEST_LOCATION);
  }
  for(uint32_t y = boundary; y < full.GetHeight(); ++y)
  {
    DALI_TEST_EQUALS(std::memcmp(fullPixels.buffer + y * full.GetStrideBytes(),
                                 lowerPixels.buffer + (y - boundary) * lower.GetStrideBytes(),
                                 rowBytes),
                     0,
                     TEST_LOCATION);
  }

  const Reveal::FadeBlurParameters fadeBlurParameters = Reveal::ResolveFadeBlurParameters(
    Reveal::ResolveFadeBlurReferencePixelSize(*model, false),
    UiText::Reveal::AUTO_BLUR_STRENGTH,
    static_cast<uint32_t>(fullSize.width),
    static_cast<uint32_t>(fullSize.height));
  const uint32_t guardBand = Reveal::GetFadeBlurGuardBand(fadeBlurParameters);
  const uint32_t fadeBlurBoundary = std::max(4u, boundary & ~3u);
  PixelData fullFadeBlur = typesetter->RenderTextRevealMetadata(
    fullSize, UiText::Direction::LEFT_TO_RIGHT, finalPlan, fadeDuration,
    0u, Size::ZERO, false, Size::ZERO,
    fadeBlurParameters.scale, fadeBlurParameters.targetRadius, false, guardBand);
  PixelData upperFadeBlur = typesetter->RenderTextRevealMetadata(
    Vector2(fullSize.width, static_cast<float>(fadeBlurBoundary)),
    UiText::Direction::LEFT_TO_RIGHT, finalPlan, fadeDuration, 0u, fullSize,
    false, Size::ZERO, fadeBlurParameters.scale, fadeBlurParameters.targetRadius, false, guardBand);
  PixelData lowerFadeBlur = typesetter->RenderTextRevealMetadata(
    Vector2(fullSize.width, fullSize.height - static_cast<float>(fadeBlurBoundary)),
    UiText::Direction::LEFT_TO_RIGHT, finalPlan, fadeDuration, fadeBlurBoundary, fullSize,
    false, Size::ZERO, fadeBlurParameters.scale, fadeBlurParameters.targetRadius, false, guardBand);
  DALI_TEST_CHECK(fullFadeBlur && upperFadeBlur && lowerFadeBlur);
  const Dali::Integration::PixelDataBuffer fullFadeBlurPixels =
    Dali::Integration::GetPixelDataBuffer(fullFadeBlur);
  const Dali::Integration::PixelDataBuffer upperFadeBlurPixels =
    Dali::Integration::GetPixelDataBuffer(upperFadeBlur);
  const Dali::Integration::PixelDataBuffer lowerFadeBlurPixels =
    Dali::Integration::GetPixelDataBuffer(lowerFadeBlur);
  for(uint32_t y = 0u; y < fadeBlurBoundary; ++y)
  {
    DALI_TEST_EQUALS(std::memcmp(fullFadeBlurPixels.buffer + y * fullFadeBlur.GetStrideBytes(),
                                 upperFadeBlurPixels.buffer + y * upperFadeBlur.GetStrideBytes(),
                                 rowBytes),
                     0,
                     TEST_LOCATION);
  }
  for(uint32_t y = fadeBlurBoundary; y < fullFadeBlur.GetHeight(); ++y)
  {
    DALI_TEST_EQUALS(std::memcmp(fullFadeBlurPixels.buffer + y * fullFadeBlur.GetStrideBytes(),
                                 lowerFadeBlurPixels.buffer + (y - fadeBlurBoundary) * lowerFadeBlur.GetStrideBytes(),
                                 rowBytes),
                     0,
                     TEST_LOCATION);
  }
  END_TEST;
}

int UtcDaliTextRevealMetadataOwnershipHaloP(void)
{
  constexpr uint32_t WIDTH      = 5u;
  constexpr uint32_t HEIGHT     = 5u;
  constexpr uint32_t PIXEL_SIZE = 4u;

  auto pixelAt = [PIXEL_SIZE](std::vector<uint8_t>& pixels, uint32_t width, uint32_t x, uint32_t y)
  {
    return pixels.data() + (static_cast<size_t>(y) * width + x) * PIXEL_SIZE;
  };

  std::vector<uint8_t> metadata(static_cast<size_t>(WIDTH) * HEIGHT * PIXEL_SIZE, 0u);
  uint8_t*             center = pixelAt(metadata, WIDTH, 2u, 2u);
  center[0u]                  = 0x12u;
  center[1u]                  = 0x34u;
  center[2u]                  = 0xffu;
  center[3u]                  = 0xc8u;

  Reveal::ExpandMetadataOwnership(metadata.data(), WIDTH, HEIGHT);

  for(uint32_t y = 0u; y < HEIGHT; ++y)
  {
    for(uint32_t x = 0u; x < WIDTH; ++x)
    {
      const uint8_t* pixel = pixelAt(metadata, WIDTH, x, y);
      const uint32_t dx    = x > 2u ? x - 2u : 2u - x;
      const uint32_t dy    = y > 2u ? y - 2u : 2u - y;
      if(dx == 0u && dy == 0u)
      {
        // Raster-owned metadata, including coverage, remains bit-identical.
        DALI_TEST_EQUALS(pixel[0u], 0x12u, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[1u], 0x34u, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[2u], 0xffu, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[3u], 0xc8u, TEST_LOCATION);
      }
      else if(dx <= 1u && dy <= 1u)
      {
        // Halo texels own the same timing while carrying no raster coverage.
        DALI_TEST_EQUALS(pixel[0u], 0x12u, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[1u], 0x34u, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[2u], 0xffu, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[3u], 0u, TEST_LOCATION);
      }
      else
      {
        // Newly written halo texels must not seed a recursive expansion.
        DALI_TEST_EQUALS(pixel[0u], 0u, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[1u], 0u, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[2u], 0u, TEST_LOCATION);
        DALI_TEST_EQUALS(pixel[3u], 0u, TEST_LOCATION);
      }
    }
  }

  // When two units compete for an empty texel, later-start-wins prevents a
  // future glyph's LINEAR coverage from inheriting an earlier unit's timing.
  std::vector<uint8_t> conflict(3u * PIXEL_SIZE, 0u);
  uint8_t*             early = pixelAt(conflict, 3u, 0u, 0u);
  early[0u]                  = 0x10u;
  early[1u]                  = 0x00u;
  early[2u]                  = 0xffu;
  early[3u]                  = 0xffu;
  uint8_t* late              = pixelAt(conflict, 3u, 2u, 0u);
  late[0u]                   = 0x90u;
  late[1u]                   = 0x00u;
  late[2u]                   = 0xffu;
  late[3u]                   = 0x01u;

  Reveal::ExpandMetadataOwnership(conflict.data(), 3u, 1u);

  const uint8_t* selected = pixelAt(conflict, 3u, 1u, 0u);
  DALI_TEST_EQUALS(selected[0u], 0x90u, TEST_LOCATION);
  DALI_TEST_EQUALS(selected[1u], 0x00u, TEST_LOCATION);
  DALI_TEST_EQUALS(selected[2u], 0xffu, TEST_LOCATION);
  DALI_TEST_EQUALS(selected[3u], 0u, TEST_LOCATION);

  // A buffer containing no revealable glyph ownership remains background.
  std::vector<uint8_t> empty(3u * 3u * PIXEL_SIZE, 0u);
  Reveal::ExpandMetadataOwnership(empty.data(), 3u, 3u);
  DALI_TEST_CHECK(std::all_of(empty.begin(), empty.end(), [](uint8_t value) { return value == 0u; }));
  END_TEST;
}

int UtcDaliTextRevealFadeBlurMetadataP(void)
{
  UiTestApplication     application;
  UiText::ControllerPtr controller = UiText::Controller::New();
  controller->SetText("AV");
  controller->SetDefaultFontSize(80.0f, UiText::Controller::PIXEL_SIZE);
  controller->SetCharacterSpacing(-20.0f);
  const Vector2 layoutSize(220.0f, 112.0f);
  controller->Relayout(layoutSize);

  const UiText::ModelInterface* model = controller->GetRenderTextModel();
  DALI_TEST_CHECK(model);
  UiText::TypesetterPtr typesetter = UiText::Typesetter::New(model);
  const Reveal::Plan sourcePlan = Reveal::BuildCharacterPlan(*model, 0.5f);
  const Reveal::Plan finalPlan  = typesetter->CreateFinalRevealPlan(sourcePlan, Reveal::Unit::CHARACTER);
  DALI_TEST_EQUALS(finalPlan.GetUnitCount(), 2u, TEST_LOCATION);

  float ordinaryFadeDuration = 0.0f;
  PixelData ordinary = typesetter->RenderTextRevealMetadata(layoutSize,
                                                            UiText::Direction::LEFT_TO_RIGHT,
                                                            finalPlan,
                                                            ordinaryFadeDuration);
  float fadeBlurDuration = 0.0f;
  PixelData fadeBlur = typesetter->RenderTextRevealMetadata(layoutSize,
                                                            UiText::Direction::LEFT_TO_RIGHT,
                                                            finalPlan,
                                                            fadeBlurDuration,
                                                            0u,
                                                            Size::ZERO,
                                                            false,
                                                            Size::ZERO,
                                                            0.25f);
  DALI_TEST_CHECK(ordinary && fadeBlur);
  DALI_TEST_EQUALS(fadeBlurDuration, ordinaryFadeDuration, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(fadeBlur.GetPixelFormat(), Pixel::RGBA8888, TEST_LOCATION);
  DALI_TEST_EQUALS(fadeBlur.GetWidth(), ordinary.GetWidth(), TEST_LOCATION);
  DALI_TEST_EQUALS(fadeBlur.GetHeight(), ordinary.GetHeight(), TEST_LOCATION);

  const Dali::Integration::PixelDataBuffer ordinaryPixels = Dali::Integration::GetPixelDataBuffer(ordinary);
  const Dali::Integration::PixelDataBuffer fadeBlurPixels = Dali::Integration::GetPixelDataBuffer(fadeBlur);
  DALI_TEST_CHECK(ordinaryPixels.buffer && fadeBlurPixels.buffer);

  bool foundBlurCoverage       = false;
  bool foundBlurHalo           = false;
  bool foundLaterBlurOwnership = false;
  for(uint32_t y = 0u; y < fadeBlur.GetHeight(); ++y)
  {
    const uint8_t* ordinaryRow = ordinaryPixels.buffer + static_cast<size_t>(y) * ordinary.GetStrideBytes();
    const uint8_t* fadeBlurRow = fadeBlurPixels.buffer + static_cast<size_t>(y) * fadeBlur.GetStrideBytes();
    for(uint32_t x = 0u; x < fadeBlur.GetWidth(); ++x)
    {
      const uint8_t* ordinaryPixel = ordinaryRow + static_cast<size_t>(x) * 4u;
      const uint8_t* fadeBlurPixel = fadeBlurRow + static_cast<size_t>(x) * 4u;
      foundBlurCoverage |= fadeBlurPixel[3u] != 0u;
      foundBlurHalo |= ordinaryPixel[2u] == 0u && fadeBlurPixel[3u] != 0u;

      if(ordinaryPixel[2u] != 0u)
      {
        DALI_TEST_EQUALS(fadeBlurPixel[0u], ordinaryPixel[0u], TEST_LOCATION);
        DALI_TEST_EQUALS(fadeBlurPixel[1u], ordinaryPixel[1u], TEST_LOCATION);
        const uint32_t sharpStart = static_cast<uint32_t>(ordinaryPixel[0u]) * 256u + ordinaryPixel[1u];
        const uint32_t blurStart  = static_cast<uint32_t>(fadeBlurPixel[2u]) * 257u;
        DALI_TEST_CHECK(blurStart >= sharpStart);
        foundLaterBlurOwnership |= blurStart > sharpStart + 256u;
      }
    }
  }
  DALI_TEST_CHECK(foundBlurCoverage);
  DALI_TEST_CHECK(foundBlurHalo);
  DALI_TEST_CHECK(foundLaterBlurOwnership);

  TextAbstraction::Segmentation segmentation  = TextAbstraction::Segmentation::New();
  const Reveal::Plan            wordSourcePlan = Reveal::BuildPlan(*model, Reveal::Unit::WORD, 0.5f, segmentation);
  const Reveal::Plan            wordFinalPlan  = typesetter->CreateFinalRevealPlan(wordSourcePlan, Reveal::Unit::WORD);
  DALI_TEST_EQUALS(wordFinalPlan.GetUnitCount(), 1u, TEST_LOCATION);
  float     wordFadeDuration = 0.0f;
  PixelData wordFadeBlur     = typesetter->RenderTextRevealMetadata(
    layoutSize,
    UiText::Direction::LEFT_TO_RIGHT,
    wordFinalPlan,
    wordFadeDuration,
    0u,
    Size::ZERO,
    false,
    Size::ZERO,
    0.25f);
  DALI_TEST_CHECK(wordFadeBlur);
  const Dali::Integration::PixelDataBuffer wordPixels = Dali::Integration::GetPixelDataBuffer(wordFadeBlur);
  DALI_TEST_CHECK(wordPixels.buffer);
  bool foundWordBlurCoverage = false;
  for(uint32_t y = 0u; y < wordFadeBlur.GetHeight(); ++y)
  {
    const uint8_t* wordRow = wordPixels.buffer + static_cast<size_t>(y) * wordFadeBlur.GetStrideBytes();
    for(uint32_t x = 0u; x < wordFadeBlur.GetWidth(); ++x)
    {
      foundWordBlurCoverage |= wordRow[static_cast<size_t>(x) * 4u + 3u] != 0u;
    }
  }
  DALI_TEST_CHECK(foundWordBlurCoverage);
  END_TEST;
}

int UtcDaliTextRevealFadeBlurProcessorP(void)
{
  constexpr uint32_t PIXEL_SIZE = 4u;

  PixelBuffer invalid;
  DALI_TEST_CHECK(!Reveal::PrepareFadeBlurBuffer(invalid, 0.5f, 4.0f));
  DALI_TEST_CHECK(!Reveal::CropFadeBlurBuffer(invalid, 0u, 0u, 1u, 1u));

  PixelBuffer cropSource = PixelBuffer::New(4u, 4u, Pixel::L8);
  DALI_TEST_CHECK(cropSource);
  DALI_TEST_CHECK(!Reveal::CropFadeBlurBuffer(cropSource, 4u, 0u, 1u, 1u));
  DALI_TEST_CHECK(!Reveal::CropFadeBlurBuffer(cropSource, 0u, 4u, 1u, 1u));
  DALI_TEST_CHECK(!Reveal::CropFadeBlurBuffer(cropSource,
                                              std::numeric_limits<uint32_t>::max(),
                                              0u,
                                              2u,
                                              1u));
  DALI_TEST_EQUALS(cropSource.GetWidth(), 4u, TEST_LOCATION);
  DALI_TEST_EQUALS(cropSource.GetHeight(), 4u, TEST_LOCATION);

  // Channel-independent averaging must preserve premultiplied RGBA. A convex
  // combination cannot make a color channel exceed its alpha channel.
  PixelBuffer rgba = PixelBuffer::New(8u, 8u, Pixel::RGBA8888);
  DALI_TEST_CHECK(rgba);
  for(uint32_t y = 0u; y < rgba.GetHeight(); ++y)
  {
    uint8_t* row = rgba.GetBuffer() + static_cast<size_t>(y) * rgba.GetStrideBytes();
    for(uint32_t x = 0u; x < rgba.GetWidth(); ++x)
    {
      uint8_t*      pixel = row + static_cast<size_t>(x) * PIXEL_SIZE;
      const uint8_t alpha = static_cast<uint8_t>((x + y) * 16u);
      pixel[0u]           = static_cast<uint8_t>(alpha / 2u);
      pixel[1u]           = alpha;
      pixel[2u]           = static_cast<uint8_t>(alpha / 4u);
      pixel[3u]           = alpha;
    }
  }
  DALI_TEST_CHECK(Reveal::PrepareFadeBlurBuffer(rgba, 0.25f, 8.0f));
  DALI_TEST_EQUALS(rgba.GetWidth(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(rgba.GetHeight(), 2u, TEST_LOCATION);
  for(uint32_t y = 0u; y < rgba.GetHeight(); ++y)
  {
    const uint8_t* row = rgba.GetBuffer() + static_cast<size_t>(y) * rgba.GetStrideBytes();
    for(uint32_t x = 0u; x < rgba.GetWidth(); ++x)
    {
      const uint8_t* pixel = row + static_cast<size_t>(x) * PIXEL_SIZE;
      DALI_TEST_CHECK(pixel[0u] <= pixel[3u]);
      DALI_TEST_CHECK(pixel[1u] <= pixel[3u]);
      DALI_TEST_CHECK(pixel[2u] <= pixel[3u]);
    }
  }

  PixelBuffer negligibleRgba = PixelBuffer::New(8u, 8u, Pixel::RGBA8888);
  DALI_TEST_CHECK(negligibleRgba);
  memset(negligibleRgba.GetBuffer(), 1u,
         static_cast<size_t>(negligibleRgba.GetStrideBytes()) * negligibleRgba.GetHeight());
  DALI_TEST_CHECK(Reveal::PrepareFadeBlurBuffer(negligibleRgba, 0.25f, 8.0f));
  const size_t negligibleRgbaSize = static_cast<size_t>(negligibleRgba.GetStrideBytes()) * negligibleRgba.GetHeight();
  DALI_TEST_CHECK(std::all_of(negligibleRgba.GetBuffer(),
                              negligibleRgba.GetBuffer() + negligibleRgbaSize,
                              [](uint8_t value)
  { return value == 0u; }));

  // Direct low-resolution projection writes only metadata A and avoids a
  // full-size PixelBuffer temporary.
  PixelBuffer coverage = PixelBuffer::New(8u, 8u, Pixel::L8);
  DALI_TEST_CHECK(coverage);
  memset(coverage.GetBuffer(), 0u, static_cast<size_t>(coverage.GetStrideBytes()) * coverage.GetHeight());
  coverage.GetBuffer()[3u * coverage.GetStrideBytes() + 3u] = 255u;
  DALI_TEST_CHECK(Reveal::PrepareFadeBlurBuffer(coverage, 0.25f, 4.0f));
  std::vector<uint8_t> projected(8u * 8u * PIXEL_SIZE, 0u);
  for(size_t index = 0u; index < projected.size(); index += PIXEL_SIZE)
  {
    projected[index]      = 1u;
    projected[index + 1u] = 2u;
    projected[index + 2u] = 3u;
  }
  Reveal::WriteFadeBlurCoverage(coverage, projected.data(), 8u, 8u);
  bool foundCoverage = false;
  for(size_t index = 0u; index < projected.size(); index += PIXEL_SIZE)
  {
    DALI_TEST_EQUALS(projected[index], 1u, TEST_LOCATION);
    DALI_TEST_EQUALS(projected[index + 1u], 2u, TEST_LOCATION);
    DALI_TEST_EQUALS(projected[index + 2u], 3u, TEST_LOCATION);
    foundCoverage |= projected[index + 3u] != 0u;
  }
  DALI_TEST_CHECK(foundCoverage);

  auto setOwnership = [PIXEL_SIZE](std::vector<uint8_t>& metadata,
                                   uint32_t              width,
                                   uint32_t              x,
                                   uint32_t              y,
                                   uint16_t              encodedStart,
                                   uint8_t               coverageValue)
  {
    uint8_t* pixel = metadata.data() + (static_cast<size_t>(y) * width + x) * PIXEL_SIZE;
    pixel[0u]      = static_cast<uint8_t>(encodedStart >> 8u);
    pixel[1u]      = static_cast<uint8_t>(encodedStart & 0xffu);
    pixel[2u]      = 255u;
    pixel[3u]      = coverageValue;
  };

  constexpr uint32_t   WIDTH  = 25u;
  constexpr uint32_t   HEIGHT = 25u;
  std::vector<uint8_t> octagon(static_cast<size_t>(WIDTH) * HEIGHT * PIXEL_SIZE, 0u);
  setOwnership(octagon, WIDTH, 12u, 12u, 0x8000u, 255u);
  Reveal::MaterializeFadeBlurTiming(octagon.data(), WIDTH, HEIGHT, 1.0f, 4.0f,
                                    false);
  const size_t squareCorner = (static_cast<size_t>(17u) * WIDTH + 17u) * PIXEL_SIZE + 2u;
  DALI_TEST_EQUALS(octagon[squareCorner], 0u, TEST_LOCATION);

  // Overlapping footprints still select the later unit, so a future glyph
  // cannot borrow an earlier unit's blur timing.
  std::vector<uint8_t> conflict(static_cast<size_t>(WIDTH) * HEIGHT * PIXEL_SIZE, 0u);
  setOwnership(conflict, WIDTH, 8u, 12u, 0x2000u, 255u);
  setOwnership(conflict, WIDTH, 16u, 12u, 0xd000u, 255u);
  Reveal::MaterializeFadeBlurTiming(conflict.data(), WIDTH, HEIGHT, 1.0f, 4.0f,
                                    false);
  const size_t overlap = (static_cast<size_t>(12u) * WIDTH + 12u) * PIXEL_SIZE + 2u;
  const size_t late    = (static_cast<size_t>(12u) * WIDTH + 16u) * PIXEL_SIZE + 2u;
  DALI_TEST_CHECK(conflict[late] != 0u);
  DALI_TEST_EQUALS(conflict[overlap], conflict[late], TEST_LOCATION);

  // Coverage-aware timing discards only negligible normal-glyph halo. The
  // preserved-color path passes false and therefore never applies this cut.
  std::vector<uint8_t> negligible(static_cast<size_t>(WIDTH) * HEIGHT * PIXEL_SIZE, 0u);
  setOwnership(negligible, WIDTH, 12u, 12u, 0x8000u, 1u);
  Reveal::MaterializeFadeBlurTiming(negligible.data(), WIDTH, HEIGHT, 1.0f, 4.0f,
                                    true);
  for(size_t index = 2u; index < negligible.size(); index += PIXEL_SIZE)
  {
    DALI_TEST_EQUALS(negligible[index], 0u, TEST_LOCATION);
  }

  // The optimized footprint must cover every meaningful texel produced by
  // the selected blur and LINEAR projection at both supported radii/scales.
  // This guards the spatial safety contract independently of font metrics.
  for(float scale : {1.0f, 0.5f, 0.25f})
  {
    for(float radius : {4.0f, 8.0f})
    {
      constexpr uint32_t MASK_SIZE = 64u;
      PixelBuffer        mask      = PixelBuffer::New(MASK_SIZE, MASK_SIZE, Pixel::L8);
      DALI_TEST_CHECK(mask);
      memset(mask.GetBuffer(), 0u, static_cast<size_t>(mask.GetStrideBytes()) * mask.GetHeight());

      std::vector<uint8_t> timing(static_cast<size_t>(MASK_SIZE) * MASK_SIZE * PIXEL_SIZE, 0u);
      for(uint32_t y = 28u; y < 36u; ++y)
      {
        memset(mask.GetBuffer() + static_cast<size_t>(y) * mask.GetStrideBytes() + 28u, 255u, 8u);
        for(uint32_t x = 28u; x < 36u; ++x)
        {
          setOwnership(timing, MASK_SIZE, x, y, 0x8000u, 255u);
        }
      }

      Reveal::ExpandMetadataOwnership(timing.data(), MASK_SIZE, MASK_SIZE);
      DALI_TEST_CHECK(Reveal::PrepareFadeBlurBuffer(mask, scale, radius));
      Reveal::WriteFadeBlurCoverage(mask, timing.data(), MASK_SIZE, MASK_SIZE);
      Reveal::MaterializeFadeBlurTiming(timing.data(), MASK_SIZE, MASK_SIZE, scale, radius,
                                        true);
      for(size_t index = 0u; index < timing.size(); index += PIXEL_SIZE)
      {
        if(timing[index + 3u] >= 2u)
        {
          DALI_TEST_CHECK(timing[index + 2u] != 0u);
        }
      }
    }
  }
  END_TEST;
}

int UtcDaliTextRevealFadeBlurAdaptivePolicyP(void)
{
  UiTestApplication application;

  struct Expected
  {
    float    referencePixelSize;
    float    radius;
    float    scale;
    uint32_t supportRadius;
  };
  const Expected expected[] = {
    {12.0f, 4.0f, 0.5f, 2u},
    {15.0f, 4.0f, 0.5f, 2u},
    {18.0f, 4.0f, 0.5f, 2u},
    {24.0f, 4.0f, 0.5f, 2u},
    {30.0f, 4.0f, 0.5f, 2u},
    {42.0f, 5.0f, 0.5f, 3u},
    {63.0f, 8.0f, 0.25f, 2u},
  };
  for(const Expected& item : expected)
  {
    const Reveal::FadeBlurParameters automatic =
      Reveal::ResolveFadeBlurParameters(item.referencePixelSize, UiText::Reveal::AUTO_BLUR_STRENGTH);
    const Reveal::FadeBlurParameters explicitFull =
      Reveal::ResolveFadeBlurParameters(item.referencePixelSize, 1.0f);
    DALI_TEST_EQUALS(automatic.referencePixelSize, item.referencePixelSize, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(automatic.targetRadius, item.radius, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(automatic.scale, item.scale, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(automatic.supportRadius, item.supportRadius, TEST_LOCATION);
    DALI_TEST_EQUALS(automatic.firstRadius + automatic.secondRadius,
                     automatic.supportRadius,
                     TEST_LOCATION);
    DALI_TEST_CHECK(automatic.firstRadius > 0u);
    DALI_TEST_CHECK(automatic.secondRadius > 0u);
    DALI_TEST_EQUALS(Reveal::GetFadeBlurGuardBand(automatic) % 4u, 0u, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitFull.referencePixelSize, automatic.referencePixelSize, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitFull.targetRadius, automatic.targetRadius, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitFull.scale, automatic.scale, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitFull.supportRadius, automatic.supportRadius, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitFull.firstRadius, automatic.firstRadius, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitFull.secondRadius, automatic.secondRadius, TEST_LOCATION);
    DALI_TEST_EQUALS(Reveal::GetFadeBlurGuardBand(explicitFull),
                     Reveal::GetFadeBlurGuardBand(automatic),
                     TEST_LOCATION);

    float previousAdaptiveRadius = 0.0f;
    for(float strength : {0.25f, 0.5f, 0.75f, 1.0f})
    {
      const Reveal::FadeBlurParameters explicitStrength =
        Reveal::ResolveFadeBlurParameters(item.referencePixelSize, strength);
      DALI_TEST_CHECK(explicitStrength.targetRadius > previousAdaptiveRadius);
      DALI_TEST_CHECK(explicitStrength.targetRadius <= automatic.targetRadius);
      DALI_TEST_EQUALS(explicitStrength.scale, automatic.scale, EPSILON, TEST_LOCATION);
      previousAdaptiveRadius = explicitStrength.targetRadius;
    }
  }

  const Reveal::FadeBlurParameters automatic20 =
    Reveal::ResolveFadeBlurParameters(20.0f, UiText::Reveal::AUTO_BLUR_STRENGTH, 320u, 96u);
  float previousRadius = 0.0f;
  for(float strength : {0.25f, 0.5f, 0.75f, 1.0f})
  {
    const Reveal::FadeBlurParameters explicitStrength =
      Reveal::ResolveFadeBlurParameters(20.0f, strength, 320u, 96u);
    DALI_TEST_CHECK(explicitStrength.targetRadius > previousRadius);
    DALI_TEST_EQUALS(explicitStrength.targetRadius,
                     automatic20.targetRadius * std::sqrt(strength),
                     EPSILON,
                     TEST_LOCATION);
    DALI_TEST_CHECK(explicitStrength.targetRadius >= automatic20.targetRadius * strength);
    DALI_TEST_CHECK(explicitStrength.targetRadius <= automatic20.targetRadius);
    DALI_TEST_EQUALS(explicitStrength.scale, automatic20.scale, EPSILON, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitStrength.lowResolutionWidth, automatic20.lowResolutionWidth, TEST_LOCATION);
    DALI_TEST_EQUALS(explicitStrength.lowResolutionHeight, automatic20.lowResolutionHeight, TEST_LOCATION);
    previousRadius = explicitStrength.targetRadius;
  }

  const Reveal::FadeBlurParameters explicitFull20 =
    Reveal::ResolveFadeBlurParameters(20.0f, 1.0f, 320u, 96u);
  DALI_TEST_EQUALS(explicitFull20.targetRadius, automatic20.targetRadius, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitFull20.scale, automatic20.scale, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitFull20.supportRadius, automatic20.supportRadius, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitFull20.lowResolutionWidth, automatic20.lowResolutionWidth, TEST_LOCATION);
  DALI_TEST_EQUALS(explicitFull20.lowResolutionHeight, automatic20.lowResolutionHeight, TEST_LOCATION);

  PixelBuffer automaticBuffer = PixelBuffer::New(64u, 32u, Pixel::RGBA8888);
  PixelBuffer explicitBuffer  = PixelBuffer::New(64u, 32u, Pixel::RGBA8888);
  DALI_TEST_CHECK(automaticBuffer && explicitBuffer);
  for(uint32_t y = 0u; y < automaticBuffer.GetHeight(); ++y)
  {
    for(uint32_t x = 0u; x < automaticBuffer.GetWidth(); ++x)
    {
      const size_t index = static_cast<size_t>(y) * automaticBuffer.GetStrideBytes() + x * 4u;
      automaticBuffer.GetBuffer()[index]      = static_cast<uint8_t>(x * 3u);
      automaticBuffer.GetBuffer()[index + 1u] = static_cast<uint8_t>(y * 7u);
      automaticBuffer.GetBuffer()[index + 2u] = static_cast<uint8_t>(x + y);
      automaticBuffer.GetBuffer()[index + 3u] = static_cast<uint8_t>((x * 5u + y * 3u) & 0xffu);
    }
  }
  memcpy(explicitBuffer.GetBuffer(),
         automaticBuffer.GetBuffer(),
         static_cast<size_t>(automaticBuffer.GetStrideBytes()) * automaticBuffer.GetHeight());
  DALI_TEST_CHECK(Reveal::PrepareFadeBlurBuffer(automaticBuffer,
                                                automatic20.scale,
                                                automatic20.targetRadius));
  DALI_TEST_CHECK(Reveal::PrepareFadeBlurBuffer(explicitBuffer,
                                                explicitFull20.scale,
                                                explicitFull20.targetRadius));
  DALI_TEST_EQUALS(explicitBuffer.GetWidth(), automaticBuffer.GetWidth(), TEST_LOCATION);
  DALI_TEST_EQUALS(explicitBuffer.GetHeight(), automaticBuffer.GetHeight(), TEST_LOCATION);
  DALI_TEST_EQUALS(memcmp(explicitBuffer.GetBuffer(),
                          automaticBuffer.GetBuffer(),
                          static_cast<size_t>(automaticBuffer.GetStrideBytes()) * automaticBuffer.GetHeight()),
                   0,
                   TEST_LOCATION);

  constexpr uint32_t METADATA_WIDTH  = 32u;
  constexpr uint32_t METADATA_HEIGHT = 16u;
  std::vector<uint8_t> automaticMetadata(METADATA_WIDTH * METADATA_HEIGHT * 4u, 0u);
  for(uint32_t x = 4u; x < 28u; x += 4u)
  {
    uint8_t* pixel = automaticMetadata.data() + (8u * METADATA_WIDTH + x) * 4u;
    pixel[0u]      = static_cast<uint8_t>(x * 7u);
    pixel[1u]      = static_cast<uint8_t>(x * 3u);
    pixel[2u]      = 255u;
    pixel[3u]      = 255u;
  }
  std::vector<uint8_t> explicitMetadata = automaticMetadata;
  Reveal::MaterializeFadeBlurTiming(automaticMetadata.data(),
                                    METADATA_WIDTH,
                                    METADATA_HEIGHT,
                                    automatic20.scale,
                                    automatic20.targetRadius,
                                    false);
  Reveal::MaterializeFadeBlurTiming(explicitMetadata.data(),
                                    METADATA_WIDTH,
                                    METADATA_HEIGHT,
                                    explicitFull20.scale,
                                    explicitFull20.targetRadius,
                                    false);
  DALI_TEST_EQUALS(memcmp(explicitMetadata.data(),
                          automaticMetadata.data(),
                          automaticMetadata.size()),
                   0,
                   TEST_LOCATION);

  const Reveal::FadeBlurParameters smallRaster =
    Reveal::ResolveFadeBlurParameters(15.0f, UiText::Reveal::AUTO_BLUR_STRENGTH, 320u, 96u);
  DALI_TEST_EQUALS(smallRaster.targetRadius, 4.0f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(smallRaster.scale, 0.5f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(smallRaster.lowResolutionWidth, 160u, TEST_LOCATION);
  DALI_TEST_EQUALS(smallRaster.lowResolutionHeight, 48u, TEST_LOCATION);

  const Reveal::FadeBlurParameters fhdSmallText =
    Reveal::ResolveFadeBlurParameters(15.0f, UiText::Reveal::AUTO_BLUR_STRENGTH, 1920u, 1080u);
  DALI_TEST_EQUALS(fhdSmallText.targetRadius, 4.0f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(fhdSmallText.scale, 0.25f, EPSILON, TEST_LOCATION);
  DALI_TEST_EQUALS(fhdSmallText.supportRadius, 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(fhdSmallText.lowResolutionWidth, 480u, TEST_LOCATION);
  DALI_TEST_EQUALS(fhdSmallText.lowResolutionHeight, 270u, TEST_LOCATION);

  // The model reference uses final pixel-space line metrics and is therefore
  // already adjusted for DPI, effective scale, and fitting.
  UiText::ControllerPtr controller = UiText::Controller::New();
  controller->SetText("Adaptive line metric");
  controller->SetDefaultFontSize(40.0f, UiText::Controller::PIXEL_SIZE);
  controller->Relayout(Vector2(480.0f, 128.0f));
  const UiText::ModelInterface* model = controller->GetRenderTextModel();
  DALI_TEST_CHECK(model && model->GetNumberOfLines() > 0u);
  float manualReference = 0.0f;
  for(UiText::Length index = 0u; index < model->GetNumberOfLines(); ++index)
  {
    manualReference = std::max(manualReference,
                               model->GetLines()[index].ascender - model->GetLines()[index].descender);
  }
  DALI_TEST_EQUALS(Reveal::ResolveFadeBlurReferencePixelSize(*model, false),
                   manualReference,
                   EPSILON,
                   TEST_LOCATION);
  END_TEST;
}

int UtcDaliTextRevealFadeBlurAdaptiveReferenceMetricP(void)
{
  UiTestApplication application;

  constexpr float LINE_HEIGHTS[] = {0.8f, 1.0f, 1.2f, 1.5f, 2.0f};
  constexpr float FONT_SIZES[]   = {16.0f, 20.0f, 24.0f, 40.0f, 84.0f};
  for(float fontSize : FONT_SIZES)
  {
    float baselineReference = 0.0f;
    for(float lineHeight : LINE_HEIGHTS)
    {
      UiText::ControllerPtr controller = BuildReferenceMetricController(
        "Hello World Second Line Third Line",
        fontSize,
        lineHeight,
        1.0f,
        Size(fontSize * 11.0f, std::max(192.0f, fontSize * 8.0f)));
      const UiText::ModelInterface* model = controller->GetRenderTextModel();
      DALI_TEST_CHECK(model && model->GetNumberOfLines() >= 2u);
      const float reference = Reveal::ResolveFadeBlurReferencePixelSize(*model, false);
      if(lineHeight == LINE_HEIGHTS[0u])
      {
        baselineReference = reference;
      }
      DALI_TEST_EQUALS(reference, baselineReference, EPSILON, TEST_LOCATION);
      DALI_TEST_CHECK(std::isfinite(model->GetLines()[0u].lineSpacing));
    }
  }

  float previousUiScaleReference = 0.0f;
  for(float uiScale : {0.8f, 1.0f, 1.2f, 1.4f})
  {
    UiText::ControllerPtr controller =
      BuildReferenceMetricController("UI scale reference", 24.0f, 1.0f, uiScale, Size(640.0f, 192.0f));
    const float reference = Reveal::ResolveFadeBlurReferencePixelSize(*controller->GetRenderTextModel(), false);
    DALI_TEST_CHECK(reference > previousUiScaleReference);
    previousUiScaleReference = reference;
  }

  UiText::ControllerPtr baseline =
    BuildReferenceMetricController("small text small", 20.0f, 1.0f, 1.0f, Size(640.0f, 192.0f));
  const float baselineReference      = Reveal::ResolveFadeBlurReferencePixelSize(*baseline->GetRenderTextModel(), false);
  float       previousMixedReference = baselineReference;
  for(float largeSize : {40.0f, 84.0f})
  {
    UiText::ControllerPtr mixed =
      BuildMixedSizeReferenceMetricController(largeSize, false, Size(640.0f, 256.0f));
    const float mixedReference = Reveal::ResolveFadeBlurReferencePixelSize(*mixed->GetRenderTextModel(), false);
    DALI_TEST_CHECK(mixedReference > previousMixedReference);
    previousMixedReference = mixedReference;

    UiText::ControllerPtr heading =
      BuildMixedSizeReferenceMetricController(largeSize, true, Size(180.0f, 640.0f));
    const UiText::ModelInterface* headingModel = heading->GetRenderTextModel();
    DALI_TEST_CHECK(headingModel && headingModel->GetNumberOfLines() >= 3u);
    const float headingReference = Reveal::ResolveFadeBlurReferencePixelSize(*headingModel, false);
    DALI_TEST_CHECK(headingReference > baselineReference);
    DALI_TEST_EQUALS(headingReference, mixedReference, EPSILON, TEST_LOCATION);
    float smallestHeadingLine = headingReference;
    for(UiText::Length line = 0u; line < headingModel->GetNumberOfLines(); ++line)
    {
      smallestHeadingLine = std::min(smallestHeadingLine,
                                     headingModel->GetLines()[line].ascender - headingModel->GetLines()[line].descender);
    }
    DALI_TEST_CHECK(smallestHeadingLine < headingReference);
  }

  UiText::ControllerPtr unfitted =
    BuildReferenceMetricController("A long line that needs substantial fitting", 84.0f, 1.0f, 1.0f,
                                   Size(640.0f, 192.0f));
  const float unfittedReference = Reveal::ResolveFadeBlurReferencePixelSize(*unfitted->GetRenderTextModel(), false);

  UiText::ControllerPtr fitted = UiText::Controller::New();
  fitted->SetText("A long line that needs substantial fitting");
  fitted->SetDefaultFontSize(84.0f, UiText::Controller::PIXEL_SIZE);
  fitted->SetMultiLineEnabled(false);
  fitted->SetTextFitEnabled(true);
  fitted->SetTextFitMinSize(12.0f, UiText::Controller::PIXEL_SIZE);
  fitted->SetTextFitMaxSize(84.0f, UiText::Controller::PIXEL_SIZE);
  fitted->SetTextFitStepSize(1.0f, UiText::Controller::PIXEL_SIZE);
  fitted->FitPointSizeforLayout(Size(240.0f, 80.0f));
  fitted->Relayout(Size(240.0f, 80.0f));
  const float fittedReference = Reveal::ResolveFadeBlurReferencePixelSize(*fitted->GetRenderTextModel(), false);
  DALI_TEST_CHECK(fittedReference < unfittedReference);

  float minFallbackReference = std::numeric_limits<float>::max();
  float maxFallbackReference = 0.0f;
  for(const char* text : {"Hello World", "안녕하세요 DALi UI", "العربية", "עברית", "Hello 🦋 World", "🦜🦚🐙🦀"})
  {
    UiText::ControllerPtr controller =
      BuildReferenceMetricController(text, 24.0f, 1.0f, 1.0f, Size(640.0f, 192.0f));
    const float reference = Reveal::ResolveFadeBlurReferencePixelSize(*controller->GetRenderTextModel(), false);
    DALI_TEST_CHECK(std::isfinite(reference) && reference > 0.0f);
    minFallbackReference = std::min(minFallbackReference, reference);
    maxFallbackReference = std::max(maxFallbackReference, reference);
  }
  DALI_TEST_CHECK(maxFallbackReference < minFallbackReference * 2.0f);

  UiText::ControllerPtr ordinaryText =
    BuildReferenceMetricController("A B", 20.0f, 1.0f, 1.0f, Size(320.0f, 320.0f));
  UiText::ControllerPtr withTallReplacement =
    BuildReplacementController("A B", {{1u, 1u}}, Size(320.0f, 320.0f), false, 240.0f);
  const UiText::ModelInterface* replacementModel = withTallReplacement->GetRenderTextModel();
  DALI_TEST_CHECK(replacementModel);
  DALI_TEST_CHECK(replacementModel->GetLines()[0u].ascender - replacementModel->GetLines()[0u].descender > 200.0f);
  DALI_TEST_EQUALS(Reveal::ResolveFadeBlurReferencePixelSize(*replacementModel, true),
                   Reveal::ResolveFadeBlurReferencePixelSize(*ordinaryText->GetRenderTextModel(), false),
                   EPSILON,
                   TEST_LOCATION);

  UiText::ControllerPtr replacementOnly =
    BuildReplacementController("X", {{0u, 1u}}, Size(320.0f, 320.0f), false, 240.0f);
  DALI_TEST_EQUALS(Reveal::ResolveFadeBlurReferencePixelSize(*replacementOnly->GetRenderTextModel(), true),
                   0.0f,
                   EPSILON,
                   TEST_LOCATION);

  UiText::ControllerPtr ordinaryMultiline =
    BuildReferenceMetricController("A\nB", 20.0f, 1.0f, 1.0f, Size(320.0f, 320.0f));
  UiText::ControllerPtr replacementOnlyLine =
    BuildReplacementController("A\nB", {{2u, 1u}}, Size(320.0f, 320.0f), false, 240.0f);
  DALI_TEST_EQUALS(Reveal::ResolveFadeBlurReferencePixelSize(*replacementOnlyLine->GetRenderTextModel(), true),
                   Reveal::ResolveFadeBlurReferencePixelSize(*ordinaryMultiline->GetRenderTextModel(), false),
                   EPSILON,
                   TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextRevealFadeBlurMultilineHaloP(void)
{
  UiTestApplication application;

  constexpr uint32_t PIXEL_SIZE     = 4u;
  constexpr float    FONT_SIZES[]   = {16.0f, 20.0f, 24.0f, 40.0f, 84.0f};
  constexpr float    LINE_HEIGHTS[] = {0.8f, 1.0f, 1.2f, 1.5f, 2.0f};
  const char* const  corpus[]       = {
    "Hello World Second Line Third Line Fourth Line Fifth Line",
    "AVATAR ffi office WWWW IIII AVATAR office WWWW IIII",
    "안녕하세요 DALi UI 두 번째 줄입니다 세 번째 줄입니다 네 번째 줄입니다",
    "العربية Hello 123 mixed עברית 123 Hello العربية mixed עברית 456",
  };

  TextAbstraction::Segmentation segmentation = TextAbstraction::Segmentation::New();
  for(const char* text : corpus)
  {
    for(float fontSize : FONT_SIZES)
    {
      for(float lineHeight : LINE_HEIGHTS)
      {
        const Size            layoutSize(fontSize * 8.0f, std::max(256.0f, fontSize * 8.0f));
        UiText::ControllerPtr controller =
          BuildReferenceMetricController(text, fontSize, lineHeight, 1.0f, layoutSize);
        const UiText::ModelInterface* model = controller->GetRenderTextModel();
        DALI_TEST_CHECK(model && model->GetNumberOfLines() >= 3u);
        UiText::TypesetterPtr            typesetter = UiText::Typesetter::New(model);
        const Reveal::FadeBlurParameters automatic  = Reveal::ResolveFadeBlurParameters(
          Reveal::ResolveFadeBlurReferencePixelSize(*model, false), UiText::Reveal::AUTO_BLUR_STRENGTH,
          static_cast<uint32_t>(layoutSize.width), static_cast<uint32_t>(layoutSize.height));
        const Reveal::FadeBlurParameters explicitMaximum = Reveal::ResolveFadeBlurParameters(
          Reveal::ResolveFadeBlurReferencePixelSize(*model, false), 1.0f,
          static_cast<uint32_t>(layoutSize.width), static_cast<uint32_t>(layoutSize.height));

        for(Reveal::Unit unit : {Reveal::Unit::CHARACTER, Reveal::Unit::WORD})
        {
          const Reveal::Plan sourcePlan = unit == Reveal::Unit::CHARACTER
                                            ? Reveal::BuildCharacterPlan(*model, 0.25f)
                                            : Reveal::BuildPlan(*model, unit, 0.25f, segmentation);
          const Reveal::Plan finalPlan  = typesetter->CreateFinalRevealPlan(sourcePlan, unit);
          DALI_TEST_CHECK(finalPlan.GetUnitCount() > 0u);
          std::vector<bool> scheduledBlurStarts(256u, false);
          for(float start : finalPlan.unitStart)
          {
            const uint32_t encoded = static_cast<uint32_t>(
              std::round(std::max(0.0f, std::min(1.0f, start)) * 65535.0f));
            const uint32_t key             = std::min(encoded, 65534u) + 1u;
            const uint8_t  quantized       = static_cast<uint8_t>(((key - 1u) * 255u + 65533u) / 65534u);
            scheduledBlurStarts[quantized] = true;
          }

          float     ordinaryDuration = 0.0f;
          PixelData ordinary         = typesetter->RenderTextRevealMetadata(
            layoutSize, UiText::Direction::LEFT_TO_RIGHT, finalPlan, ordinaryDuration);
          DALI_TEST_CHECK(ordinary);
          const Dali::Integration::PixelDataBuffer ordinaryPixels =
            Dali::Integration::GetPixelDataBuffer(ordinary);
          DALI_TEST_CHECK(ordinaryPixels.buffer);

          for(const Reveal::FadeBlurParameters parameters : {
                automatic,
                explicitMaximum})
          {
            float     fadeBlurDuration = 0.0f;
            PixelData fadeBlur         = typesetter->RenderTextRevealMetadata(
              layoutSize,
              UiText::Direction::LEFT_TO_RIGHT,
              finalPlan,
              fadeBlurDuration,
              0u,
              Size::ZERO,
              false,
              Size::ZERO,
              parameters.scale,
              parameters.targetRadius,
              true);
            DALI_TEST_CHECK(fadeBlur);
            DALI_TEST_EQUALS(fadeBlurDuration, ordinaryDuration, EPSILON, TEST_LOCATION);
            const Dali::Integration::PixelDataBuffer fadeBlurPixels =
              Dali::Integration::GetPixelDataBuffer(fadeBlur);
            DALI_TEST_CHECK(fadeBlurPixels.buffer);

            bool foundSharpOwnership = false;
            bool foundBlurCoverage   = false;
            for(uint32_t y = 0u; y < fadeBlur.GetHeight(); ++y)
            {
              const uint8_t* ordinaryRow = ordinaryPixels.buffer + static_cast<size_t>(y) * ordinary.GetStrideBytes();
              const uint8_t* fadeBlurRow = fadeBlurPixels.buffer + static_cast<size_t>(y) * fadeBlur.GetStrideBytes();
              for(uint32_t x = 0u; x < fadeBlur.GetWidth(); ++x)
              {
                const uint8_t* ordinaryPixel = ordinaryRow + static_cast<size_t>(x) * PIXEL_SIZE;
                const uint8_t* fadeBlurPixel = fadeBlurRow + static_cast<size_t>(x) * PIXEL_SIZE;
                if(fadeBlurPixel[3u] >= 2u)
                {
                  foundBlurCoverage = true;
                  DALI_TEST_CHECK(scheduledBlurStarts[fadeBlurPixel[2u]]);
                }
                if(ordinaryPixel[2u] != 0u)
                {
                  foundSharpOwnership = true;
                  DALI_TEST_EQUALS(fadeBlurPixel[0u], ordinaryPixel[0u], TEST_LOCATION);
                  DALI_TEST_EQUALS(fadeBlurPixel[1u], ordinaryPixel[1u], TEST_LOCATION);
                  const uint32_t sharpStart = static_cast<uint32_t>(ordinaryPixel[0u]) * 256u + ordinaryPixel[1u];
                  const uint32_t blurStart  = static_cast<uint32_t>(fadeBlurPixel[2u]) * 257u;
                  DALI_TEST_CHECK(blurStart >= sharpStart);
                }
              }
            }
            DALI_TEST_CHECK(foundSharpOwnership);
            DALI_TEST_CHECK(foundBlurCoverage);
          }
        }
      }
    }
  }

  END_TEST;
}

int UtcDaliTextRevealFadeBlurCorpusP(void)
{
  UiTestApplication  application;
  constexpr uint32_t PIXEL_SIZE = 4u;

  struct Case
  {
    const char* text;
    float       spacing;
  };
  const Case corpus[] = {
    {"AB", 0.0f},
    {"AV", -20.0f},
    {"VA", -12.0f},
    {"ffi", -4.0f},
    {"office", 0.0f},
    {"Hello", 0.0f},
    {"IIII", -4.0f},
    {"WWWW", -8.0f},
    {"A🦋B", 0.0f},
    {"🦜🦚🐙🦀", 0.0f},
    {"🍉🍭🎨🚀", 0.0f},
    {"DALi 🌈 UI 🔮 Text", 0.0f},
    {"안녕하세요 DALi UI", 0.0f},
    {"العربية Hello 123", 0.0f},
    {"Hello עברית 123", 0.0f},
  };

  TextAbstraction::Segmentation segmentation = TextAbstraction::Segmentation::New();
  for(const Case& item : corpus)
  {
    for(float fontSize : {16.0f, 20.0f, 24.0f, 32.0f, 40.0f, 56.0f, 84.0f})
    {
      UiText::ControllerPtr controller = UiText::Controller::New();
      controller->SetText(item.text);
      controller->SetDefaultFontSize(fontSize, UiText::Controller::PIXEL_SIZE);
      controller->SetCharacterSpacing(item.spacing);
      controller->SetMultiLineEnabled(true);
      const Vector2 layoutSize(480.0f, std::max(128.0f, fontSize * 2.0f));
      controller->Relayout(layoutSize);

      const UiText::ModelInterface* model = controller->GetRenderTextModel();
      DALI_TEST_CHECK(model);
      UiText::TypesetterPtr typesetter = UiText::Typesetter::New(model);
      for(Reveal::Unit unit : {Reveal::Unit::CHARACTER, Reveal::Unit::WORD})
      {
        const Reveal::Plan sourcePlan = unit == Reveal::Unit::CHARACTER
                                          ? Reveal::BuildCharacterPlan(*model, 0.25f)
                                          : Reveal::BuildPlan(*model, unit, 0.25f, segmentation);
        const Reveal::Plan finalPlan  = typesetter->CreateFinalRevealPlan(sourcePlan, unit);
        DALI_TEST_CHECK(finalPlan.GetUnitCount() > 0u);
        const Reveal::FadeBlurParameters fadeBlurParameters =
          Reveal::ResolveFadeBlurParameters(Reveal::ResolveFadeBlurReferencePixelSize(*model, false),
                                            UiText::Reveal::AUTO_BLUR_STRENGTH);

        float     ordinaryFadeDuration = 0.0f;
        PixelData ordinary             = typesetter->RenderTextRevealMetadata(layoutSize,
                                                                              UiText::Direction::LEFT_TO_RIGHT,
                                                                              finalPlan,
                                                                              ordinaryFadeDuration);
        float     fadeBlurDuration     = 0.0f;
        PixelData fadeBlur             = typesetter->RenderTextRevealMetadata(
          layoutSize,
          UiText::Direction::LEFT_TO_RIGHT,
          finalPlan,
          fadeBlurDuration,
          0u,
          Size::ZERO,
          false,
          Size::ZERO,
          fadeBlurParameters.scale,
          fadeBlurParameters.targetRadius,
          true);
        DALI_TEST_CHECK(ordinary && fadeBlur);
        DALI_TEST_EQUALS(fadeBlurDuration, ordinaryFadeDuration, EPSILON, TEST_LOCATION);

        const Dali::Integration::PixelDataBuffer ordinaryPixels = Dali::Integration::GetPixelDataBuffer(ordinary);
        const Dali::Integration::PixelDataBuffer fadeBlurPixels = Dali::Integration::GetPixelDataBuffer(fadeBlur);
        DALI_TEST_CHECK(ordinaryPixels.buffer && fadeBlurPixels.buffer);
        bool foundSharpOwnership = false;
        bool foundBlurCoverage   = false;
        for(uint32_t y = 0u; y < fadeBlur.GetHeight(); ++y)
        {
          const uint8_t* ordinaryRow = ordinaryPixels.buffer + static_cast<size_t>(y) * ordinary.GetStrideBytes();
          const uint8_t* fadeBlurRow = fadeBlurPixels.buffer + static_cast<size_t>(y) * fadeBlur.GetStrideBytes();
          for(uint32_t x = 0u; x < fadeBlur.GetWidth(); ++x)
          {
            const uint8_t* ordinaryPixel = ordinaryRow + static_cast<size_t>(x) * PIXEL_SIZE;
            const uint8_t* fadeBlurPixel = fadeBlurRow + static_cast<size_t>(x) * PIXEL_SIZE;
            foundBlurCoverage |= fadeBlurPixel[3u] != 0u;
            if(ordinaryPixel[2u] != 0u)
            {
              foundSharpOwnership = true;
              DALI_TEST_EQUALS(fadeBlurPixel[0u], ordinaryPixel[0u], TEST_LOCATION);
              DALI_TEST_EQUALS(fadeBlurPixel[1u], ordinaryPixel[1u], TEST_LOCATION);
              const uint32_t sharpStart = static_cast<uint32_t>(ordinaryPixel[0u]) * 256u + ordinaryPixel[1u];
              const uint32_t blurStart  = static_cast<uint32_t>(fadeBlurPixel[2u]) * 257u;
              DALI_TEST_CHECK(blurStart >= sharpStart);
            }
          }
        }
        DALI_TEST_CHECK(foundSharpOwnership);
        DALI_TEST_CHECK(foundBlurCoverage);
      }
    }
  }
  END_TEST;
}
