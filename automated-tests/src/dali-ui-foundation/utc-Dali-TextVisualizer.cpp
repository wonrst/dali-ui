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

#include <stdlib.h>
#include <dali.h>
#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali-ui-foundation/internal/text/text-visualizer/layout-engine.h>
#include <dali-ui-foundation/internal/text/text-visualizer/prepared-text.h>
#include <dali-ui-foundation/internal/text/text-visualizer/text-preparer.h>
#include <dali-ui-test-suite-utils.h>

using namespace Dali;
using namespace Dali::Ui;
using namespace Dali::Ui::Integration;

namespace
{
const char* const PROPERTY_NAME_TEXT        = "text";
const char* const PROPERTY_NAME_FONT_FAMILY = "fontFamily";
const char* const PROPERTY_NAME_FONT_SIZE   = "fontSize";
const char* const PROPERTY_NAME_TEXT_COLOR  = "textColor";

Dali::Ui::Internal::TextVisualizer::PreparedText CreatePreparedText(const char* text, float fontSize)
{
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text     = Dali::String(text);
  input.fontSize = fontSize;
  return Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);
}
} // namespace

void utc_dali_text_visualizer_startup(void)
{
  test_return_value = TET_UNDEF;
}

void utc_dali_text_visualizer_cleanup(void)
{
  test_return_value = TET_PASS;
}

int UtcDaliTextVisualizerConstructorP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer;
  DALI_TEST_CHECK(!textVisualizer);
  END_TEST;
}

int UtcDaliTextVisualizerNewP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);
  END_TEST;
}

int UtcDaliTextVisualizerDownCastP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  BaseHandle        object(textVisualizer);
  TextVisualizer    textVisualizer2 = TextVisualizer::DownCast(object);
  TextVisualizer    textVisualizer3 = DownCast<TextVisualizer>(object);
  DALI_TEST_CHECK(textVisualizer2);
  DALI_TEST_CHECK(textVisualizer3);
  END_TEST;
}

int UtcDaliTextVisualizerText(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetText("Hello world");
  DALI_TEST_EQUALS(textVisualizer.GetText(), std::string("Hello world"), TEST_LOCATION);

  textVisualizer.SetText("Updated text");
  DALI_TEST_EQUALS(textVisualizer.GetText(), std::string("Updated text"), TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerFontFamily(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetFontFamily("Arial");
  DALI_TEST_EQUALS(textVisualizer.GetFontFamily(), std::string("Arial"), TEST_LOCATION);

  textVisualizer.SetFontFamily("Roboto");
  DALI_TEST_EQUALS(textVisualizer.GetFontFamily(), std::string("Roboto"), TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerFontSize(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetFontSize(20.0f);
  DALI_TEST_EQUALS(textVisualizer.GetFontSize(), 20.0f, TEST_LOCATION);

  textVisualizer.SetFontSize(32.5f);
  DALI_TEST_EQUALS(textVisualizer.GetFontSize(), 32.5f, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerTextColor(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  UiColor color(Color::BLUE);
  textVisualizer.SetTextColor(color);
  DALI_TEST_EQUALS(textVisualizer.GetTextColor().Resolve(), Color::BLUE, TEST_LOCATION);

  UiColor color2(Color::RED);
  textVisualizer.SetTextColor(color2);
  DALI_TEST_EQUALS(textVisualizer.GetTextColor().Resolve(), Color::RED, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerSetProperty(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetProperty(TextVisualizer::Property::TEXT, "Property text");
  DALI_TEST_EQUALS(textVisualizer.GetText(), std::string("Property text"), TEST_LOCATION);

  textVisualizer.SetProperty(TextVisualizer::Property::FONT_FAMILY, "Sans");
  DALI_TEST_EQUALS(textVisualizer.GetFontFamily(), std::string("Sans"), TEST_LOCATION);

  textVisualizer.SetProperty(TextVisualizer::Property::FONT_SIZE, 18.0f);
  DALI_TEST_EQUALS(textVisualizer.GetFontSize(), 18.0f, TEST_LOCATION);

  textVisualizer.SetProperty(TextVisualizer::Property::TEXT_COLOR, Color::GREEN);
  DALI_TEST_EQUALS(textVisualizer.GetTextColor().Resolve(), Color::GREEN, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerGetProperty(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  DALI_TEST_CHECK(textVisualizer.GetPropertyIndex(PROPERTY_NAME_TEXT) == TextVisualizer::Property::TEXT);
  DALI_TEST_CHECK(textVisualizer.GetPropertyIndex(PROPERTY_NAME_FONT_FAMILY) == TextVisualizer::Property::FONT_FAMILY);
  DALI_TEST_CHECK(textVisualizer.GetPropertyIndex(PROPERTY_NAME_FONT_SIZE) == TextVisualizer::Property::FONT_SIZE);
  DALI_TEST_CHECK(textVisualizer.GetPropertyIndex(PROPERTY_NAME_TEXT_COLOR) == TextVisualizer::Property::TEXT_COLOR);

  textVisualizer.SetText("Property value");
  textVisualizer.SetFontFamily("Serif");
  textVisualizer.SetFontSize(24.0f);
  textVisualizer.SetTextColor(UiColor(Color::MAGENTA));

  DALI_TEST_EQUALS(textVisualizer.GetProperty<Dali::String>(TextVisualizer::Property::TEXT), Dali::String("Property value"), TEST_LOCATION);
  DALI_TEST_EQUALS(textVisualizer.GetProperty<Dali::String>(TextVisualizer::Property::FONT_FAMILY), Dali::String("Serif"), TEST_LOCATION);
  DALI_TEST_EQUALS(textVisualizer.GetProperty<float>(TextVisualizer::Property::FONT_SIZE), 24.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(textVisualizer.GetProperty<Vector4>(TextVisualizer::Property::TEXT_COLOR), Color::MAGENTA, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPrepareP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.Prepare();
  textVisualizer.Prepare();

  END_TEST;
}

int UtcDaliTextVisualizerExclusionRegions(void)
{
  UiTestApplication         application;
  TextVisualizer            textVisualizer = TextVisualizer::New();
  Dali::Vector<Rect<float>> regions;
  DALI_TEST_CHECK(textVisualizer);

  regions.PushBack(Rect<float>(0.0f, 0.0f, 100.0f, 30.0f));
  regions.PushBack(Rect<float>(50.0f, 40.0f, 80.0f, 20.0f));

  textVisualizer.SetExclusionRegions(regions);

  const Dali::Vector<Rect<float>> storedRegions = textVisualizer.GetExclusionRegions();
  DALI_TEST_EQUALS(storedRegions.Count(), regions.Count(), TEST_LOCATION);
  DALI_TEST_CHECK(storedRegions[0] == regions[0]);
  DALI_TEST_CHECK(storedRegions[1] == regions[1]);

  END_TEST;
}

int UtcDaliTextVisualizerClearExclusionRegions(void)
{
  UiTestApplication         application;
  TextVisualizer            textVisualizer = TextVisualizer::New();
  Dali::Vector<Rect<float>> regions;
  DALI_TEST_CHECK(textVisualizer);

  regions.PushBack(Rect<float>(10.0f, 20.0f, 30.0f, 40.0f));
  textVisualizer.SetExclusionRegions(regions);
  DALI_TEST_EQUALS(textVisualizer.GetExclusionRegions().Count(), 1u, TEST_LOCATION);

  textVisualizer.ClearExclusionRegions();
  DALI_TEST_EQUALS(textVisualizer.GetExclusionRegions().Count(), 0u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPrepareAfterStateChangesP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetText("Prepared text");
  textVisualizer.SetFontFamily("Roboto");
  textVisualizer.SetFontSize(28.0f);
  textVisualizer.Prepare();

  DALI_TEST_EQUALS(textVisualizer.GetText(), std::string("Prepared text"), TEST_LOCATION);
  DALI_TEST_EQUALS(textVisualizer.GetFontFamily(), std::string("Roboto"), TEST_LOCATION);
  DALI_TEST_EQUALS(textVisualizer.GetFontSize(), 28.0f, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerExclusionRegionsWithoutPrepareP(void)
{
  UiTestApplication         application;
  TextVisualizer            textVisualizer = TextVisualizer::New();
  Dali::Vector<Rect<float>> regions;
  DALI_TEST_CHECK(textVisualizer);

  regions.PushBack(Rect<float>(5.0f, 5.0f, 25.0f, 15.0f));
  textVisualizer.SetExclusionRegions(regions);
  textVisualizer.SetExclusionRegions(regions);
  textVisualizer.ClearExclusionRegions();

  DALI_TEST_EQUALS(textVisualizer.GetExclusionRegions().Count(), 0u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPrepareEmptyTextP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetText("");
  textVisualizer.Prepare();

  END_TEST;
}

int UtcDaliTextVisualizerPreparedAsciiCharacterCountP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "abc";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_EQUALS(preparedText.GetCharacterCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetClusterCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetLineBreakCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetParagraphCount(), 0u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPreparedKoreanCharacterCountP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "가나다";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_EQUALS(preparedText.GetCharacterCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetClusterCount(), 3u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPreparedEmojiCharacterCountP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "😀";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_CHECK(preparedText.GetCharacterCount() > 0u);
  DALI_TEST_CHECK(preparedText.GetCharacterCount() < input.text.Size());
  DALI_TEST_EQUALS(preparedText.GetClusterCount(), preparedText.GetCharacterCount(), TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPreparedMixedTextP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "A가😀";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_EQUALS(preparedText.GetCharacterCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetCharacters().Count(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetClusterCount(), 3u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPreparedEmptyTextP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);
  Dali::Vector<Rect<float>>                        exclusionRegions;
  Dali::Ui::Internal::TextVisualizer::LayoutResult layoutResult;

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_CHECK(preparedText.Empty());
  DALI_TEST_EQUALS(preparedText.GetCharacterCount(), 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetClusterCount(), 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetScriptRunCount(), 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(preparedText.GetFontRunCount(), 0u, TEST_LOCATION);

  Dali::Ui::Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(preparedText, 100.0f, 12.0f, exclusionRegions, layoutResult);
  DALI_TEST_CHECK(layoutResult.Empty());

  END_TEST;
}

int UtcDaliTextVisualizerPreparedLineBreakInfoSmokeP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "abc\ndef";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_CHECK(preparedText.GetLineBreakCount() > 0u);
  DALI_TEST_CHECK(preparedText.GetParagraphCount() > 0u);

  END_TEST;
}

int UtcDaliTextVisualizerPreparedAsciiScriptAndFontDataP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text     = "abc";
  input.fontSize = 10.0f;

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_EQUALS(preparedText.GetCharacterCount(), 3u, TEST_LOCATION);
  DALI_TEST_CHECK(preparedText.GetScriptRunCount() > 0u);
  DALI_TEST_EQUALS(preparedText.GetScriptRuns().Count(), preparedText.GetScriptRunCount(), TEST_LOCATION);

  if(preparedText.GetScriptRunCount() > 0u)
  {
    DALI_TEST_CHECK(preparedText.GetScriptRuns()[0].characterRun.numberOfCharacters > 0u);
  }

  DALI_TEST_EQUALS(preparedText.GetFontRuns().Count(), preparedText.GetFontRunCount(), TEST_LOCATION);
  if(preparedText.GetFontRunCount() > 0u)
  {
    DALI_TEST_CHECK(preparedText.GetFontRuns()[0].characterRun.numberOfCharacters > 0u);
  }

  END_TEST;
}

int UtcDaliTextVisualizerPreparedKoreanScriptAndFontDataP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text     = "가나다";
  input.fontSize = 10.0f;

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_EQUALS(preparedText.GetCharacterCount(), 3u, TEST_LOCATION);
  DALI_TEST_CHECK(preparedText.GetScriptRunCount() > 0u);

  if(preparedText.GetFontRunCount() > 0u)
  {
    DALI_TEST_CHECK(preparedText.GetFontRuns()[0].characterRun.numberOfCharacters > 0u);
  }

  END_TEST;
}

int UtcDaliTextVisualizerPreparedEmojiScriptAndFontDataP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "😀";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_CHECK(preparedText.GetScriptRunCount() > 0u);
  DALI_TEST_EQUALS(preparedText.GetScriptRuns().Count(), preparedText.GetScriptRunCount(), TEST_LOCATION);

  if(preparedText.GetFontRunCount() > 0u)
  {
    DALI_TEST_CHECK(preparedText.GetFontRuns()[0].characterRun.numberOfCharacters > 0u);
  }

  END_TEST;
}

int UtcDaliTextVisualizerPreparedMixedScriptDataP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::TextPreparer::Input input;
  input.text = "abc가나다";

  const auto preparedText = Dali::Ui::Internal::TextVisualizer::TextPreparer::Prepare(input);

  DALI_TEST_CHECK(preparedText.IsPrepared());
  DALI_TEST_CHECK(preparedText.GetScriptRunCount() > 0u);
  DALI_TEST_EQUALS(preparedText.GetScriptRuns().Count(), preparedText.GetScriptRunCount(), TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPrepareSimpleAsciiTextP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetText("Simple ASCII text");
  textVisualizer.Prepare();
  textVisualizer.Prepare();

  END_TEST;
}

int UtcDaliTextVisualizerPrepareKoreanTextP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetText("한글 테스트 문장");
  textVisualizer.Prepare();

  END_TEST;
}

int UtcDaliTextVisualizerPrepareEmojiTextP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetText("Emoji 😀👨‍👩‍👧‍👦");
  textVisualizer.Prepare();

  END_TEST;
}

int UtcDaliTextVisualizerLayoutIntervalsWithoutExclusionP(void)
{
  UiTestApplication         application;
  Dali::Vector<Rect<float>> exclusionRegions;

  const auto availableIntervals = Dali::Ui::Internal::TextVisualizer::LayoutEngine::BuildAvailableIntervals(100.0f, 0.0f, 20.0f, exclusionRegions);

  DALI_TEST_EQUALS(availableIntervals.Count(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].x, 0.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].width, 100.0f, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerLayoutIntervalsSingleExclusionP(void)
{
  UiTestApplication         application;
  Dali::Vector<Rect<float>> exclusionRegions;
  exclusionRegions.PushBack(Rect<float>(20.0f, 0.0f, 20.0f, 20.0f));

  const auto availableIntervals = Dali::Ui::Internal::TextVisualizer::LayoutEngine::BuildAvailableIntervals(100.0f, 0.0f, 20.0f, exclusionRegions);

  DALI_TEST_EQUALS(availableIntervals.Count(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].x, 0.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].width, 20.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[1].x, 40.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[1].width, 60.0f, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerLayoutIntervalsWithoutVerticalOverlapP(void)
{
  UiTestApplication         application;
  Dali::Vector<Rect<float>> exclusionRegions;
  exclusionRegions.PushBack(Rect<float>(20.0f, 30.0f, 20.0f, 10.0f));

  const auto availableIntervals = Dali::Ui::Internal::TextVisualizer::LayoutEngine::BuildAvailableIntervals(100.0f, 0.0f, 20.0f, exclusionRegions);

  DALI_TEST_EQUALS(availableIntervals.Count(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].x, 0.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].width, 100.0f, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerLayoutIntervalsMergeBlockedRangesP(void)
{
  UiTestApplication         application;
  Dali::Vector<Rect<float>> exclusionRegions;
  exclusionRegions.PushBack(Rect<float>(20.0f, 0.0f, 20.0f, 20.0f));
  exclusionRegions.PushBack(Rect<float>(30.0f, 0.0f, 30.0f, 20.0f));

  const auto availableIntervals = Dali::Ui::Internal::TextVisualizer::LayoutEngine::BuildAvailableIntervals(100.0f, 0.0f, 20.0f, exclusionRegions);

  DALI_TEST_EQUALS(availableIntervals.Count(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].x, 0.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].width, 20.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[1].x, 60.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[1].width, 40.0f, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerLayoutIntervalsClampOutsideWidthP(void)
{
  UiTestApplication         application;
  Dali::Vector<Rect<float>> exclusionRegions;
  exclusionRegions.PushBack(Rect<float>(-10.0f, 0.0f, 30.0f, 20.0f));
  exclusionRegions.PushBack(Rect<float>(80.0f, 0.0f, 60.0f, 20.0f));

  const auto availableIntervals = Dali::Ui::Internal::TextVisualizer::LayoutEngine::BuildAvailableIntervals(100.0f, 0.0f, 20.0f, exclusionRegions);

  DALI_TEST_EQUALS(availableIntervals.Count(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].x, 20.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(availableIntervals[0].width, 60.0f, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerLayoutIntervalsFullyBlockedP(void)
{
  UiTestApplication         application;
  Dali::Vector<Rect<float>> exclusionRegions;
  exclusionRegions.PushBack(Rect<float>(0.0f, 0.0f, 100.0f, 20.0f));

  const auto availableIntervals = Dali::Ui::Internal::TextVisualizer::LayoutEngine::BuildAvailableIntervals(100.0f, 0.0f, 20.0f, exclusionRegions);

  DALI_TEST_EQUALS(availableIntervals.Count(), 0u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPlaceholderLayoutBasicP(void)
{
  UiTestApplication application;
  auto              preparedText = CreatePreparedText("abcdefghij", 10.0f);
  Dali::Vector<Rect<float>> exclusionRegions;
  Dali::Ui::Internal::TextVisualizer::LayoutResult layoutResult;

  Dali::Ui::Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(preparedText, 20.0f, 12.0f, exclusionRegions, layoutResult);

  DALI_TEST_EQUALS(layoutResult.GetLineCount(), 3u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments.Count(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments[0].clusterStart, 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments[0].clusterEnd, 4u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[2].fragments[0].clusterEnd, 10u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.clusterPlacements.Count(), 10u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPlaceholderLayoutWithExclusionP(void)
{
  UiTestApplication application;
  auto              preparedText = CreatePreparedText("abcdefghij", 10.0f);
  Dali::Vector<Rect<float>> exclusionRegions;
  exclusionRegions.PushBack(Rect<float>(20.0f, 0.0f, 20.0f, 12.0f));

  Dali::Ui::Internal::TextVisualizer::LayoutResult layoutResult;
  Dali::Ui::Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(preparedText, 100.0f, 12.0f, exclusionRegions, layoutResult);

  DALI_TEST_EQUALS(layoutResult.GetLineCount(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments.Count(), 2u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments[0].clusterStart, 0u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments[0].clusterEnd, 4u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments[1].clusterStart, 4u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments[1].clusterEnd, 10u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPlaceholderLayoutEmptyTextP(void)
{
  UiTestApplication application;
  Dali::Ui::Internal::TextVisualizer::PreparedText preparedText;
  Dali::Vector<Rect<float>>                        exclusionRegions;
  Dali::Ui::Internal::TextVisualizer::LayoutResult layoutResult;

  Dali::Ui::Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(preparedText, 100.0f, 12.0f, exclusionRegions, layoutResult);

  DALI_TEST_CHECK(layoutResult.Empty());

  END_TEST;
}

int UtcDaliTextVisualizerPlaceholderLayoutWidthChangeP(void)
{
  UiTestApplication application;
  auto              preparedText = CreatePreparedText("abcdefghij", 10.0f);
  Dali::Vector<Rect<float>> exclusionRegions;
  Dali::Ui::Internal::TextVisualizer::LayoutResult narrowLayoutResult;
  Dali::Ui::Internal::TextVisualizer::LayoutResult wideLayoutResult;

  Dali::Ui::Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(preparedText, 25.0f, 12.0f, exclusionRegions, narrowLayoutResult);
  Dali::Ui::Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(preparedText, 50.0f, 12.0f, exclusionRegions, wideLayoutResult);

  DALI_TEST_CHECK(narrowLayoutResult.GetLineCount() > wideLayoutResult.GetLineCount());

  END_TEST;
}

int UtcDaliTextVisualizerPlaceholderLayoutFullBlockedLineP(void)
{
  UiTestApplication application;
  auto              preparedText = CreatePreparedText("abcdef", 10.0f);
  Dali::Vector<Rect<float>> exclusionRegions;
  exclusionRegions.PushBack(Rect<float>(0.0f, 0.0f, 30.0f, 12.0f));

  Dali::Ui::Internal::TextVisualizer::LayoutResult layoutResult;
  Dali::Ui::Internal::TextVisualizer::LayoutEngine::LayoutPlaceholder(preparedText, 30.0f, 12.0f, exclusionRegions, layoutResult);

  DALI_TEST_EQUALS(layoutResult.GetLineCount(), 1u, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].y, 12.0f, TEST_LOCATION);
  DALI_TEST_EQUALS(layoutResult.lines[0].fragments[0].clusterEnd, 6u, TEST_LOCATION);

  END_TEST;
}

int UtcDaliTextVisualizerPlaceholderRelayoutSmokeP(void)
{
  UiTestApplication application;
  TextVisualizer    textVisualizer = TextVisualizer::New();
  DALI_TEST_CHECK(textVisualizer);

  textVisualizer.SetText("abcdefghij");
  textVisualizer.SetFontSize(10.0f);
  textVisualizer.Prepare();
  textVisualizer.SetRequestedWidth(25.0f);

  application.GetScene().Add(textVisualizer);
  application.SendNotification();
  application.Render();

  const float narrowHeight = textVisualizer.GetMeasuredSize().GetHeight();

  textVisualizer.SetRequestedWidth(50.0f);
  application.SendNotification();
  application.Render();

  const float wideHeight = textVisualizer.GetMeasuredSize().GetHeight();
  DALI_TEST_CHECK(narrowHeight > wideHeight);

  END_TEST;
}
