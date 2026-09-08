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

#include <dali-ui-foundation/dali-ui-foundation.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

using namespace Dali;
using namespace Dali::Ui;

namespace
{
constexpr float       CONTROLS_PANEL_HEIGHT       = 612.0f;
constexpr float       CONTROL_HEIGHT              = 34.0f;
constexpr float       CONTROL_SPACING             = 6.0f;
constexpr float       MENU_TITLE_WIDTH            = 86.0f;
constexpr float       STATUS_HEIGHT               = 148.0f;
constexpr std::size_t ALPHA_CASE_COUNT            = 7u;
constexpr std::size_t FADE_CASE_COUNT             = 7u;
constexpr std::size_t DURATION_CASE_COUNT         = 5u;
constexpr std::size_t TEXT_CASE_COUNT             = 8u;
constexpr std::size_t SEQUENCE_CASE_COUNT         = 2u;
constexpr std::size_t STAGGER_CASE_COUNT          = 8u;
constexpr std::size_t BLUR_RADIUS_CASE_COUNT      = 8u;
constexpr std::size_t BLUR_END_CASE_COUNT         = 9u;
constexpr std::size_t DEFAULT_DURATION_CASE_INDEX = 3u;
constexpr std::size_t DEFAULT_TEXT_CASE_INDEX     = 1u;
constexpr std::size_t LOCAL_IMAGE_CASE_INDEX      = 5u;
constexpr std::size_t REMOTE_IMAGE_CASE_INDEX     = 6u;
constexpr uint32_t    PANEL_COLOR                 = 0x111827;
constexpr uint32_t    BUTTON_COLOR                = 0x1E293B;
constexpr uint32_t    BUTTON_BORDER_COLOR         = 0x475569;
constexpr uint32_t    SELECTED_BUTTON_COLOR       = 0x1D4ED8;
constexpr uint32_t    SELECTED_BORDER_COLOR       = 0x93C5FD;
constexpr const char* REMOTE_IMAGE_URL =
  "https://raw.githubusercontent.com/dalihub/dali-ui/devel/samples/text/res/flag_us_alt.png";

enum class FillMode : uint8_t
{
  SOLID,
  WHITE_ON_BLACK,
  TEXT_GRADIENT,
  GRADIENT_SPAN
};

enum class SpanGradientKind : uint8_t
{
  LINEAR,
  RADIAL,
  CONIC
};

enum class UxPlaybackPhase : uint8_t
{
  NONE,
  ENTERING,
  WAITING,
  EXITING
};

// FadeDurationRatio controls the unit transition duration;
// SequenceStaggerRatio controls the spacing between independent sequence starts.
const float FADE_DURATION_RATIOS[FADE_CASE_COUNT] = {
  Text::Reveal::AUTO_FADE_DURATION_RATIO,
  0.0f,
  0.10f,
  0.25f,
  0.50f,
  0.75f,
  1.0f};

const char* const FADE_BUTTON_LABELS[FADE_CASE_COUNT] = {
  "Auto",
  "0",
  "0.10",
  "0.25",
  "0.50",
  "0.75",
  "1.00"};

const char* const FADE_STATUS_LABELS[FADE_CASE_COUNT] = {
  "Auto",
  "Step (0)",
  "Short (0.10)",
  "Smooth (0.25)",
  "Overlap (0.50)",
  "Long (0.75)",
  "Full Fade (1.00)"};

// Wall-clock duration is application-owned; Reveal consumes normalized progress.
const float DURATION_SECONDS[DURATION_CASE_COUNT] = {
  0.5f,
  1.0f,
  2.0f,
  4.0f,
  8.0f};

const char* const DURATION_BUTTON_LABELS[DURATION_CASE_COUNT] = {
  "0.5 s",
  "1 s",
  "2 s",
  "4 s",
  "8 s"};

const AlphaFunction::BuiltinFunction ALPHA_FUNCTIONS[ALPHA_CASE_COUNT] = {
  AlphaFunction::LINEAR,
  AlphaFunction::EASE_IN_SQUARE,
  AlphaFunction::EASE_OUT_SQUARE,
  AlphaFunction::EASE_IN,
  AlphaFunction::EASE_OUT,
  AlphaFunction::EASE_IN_OUT,
  AlphaFunction::EASE_IN_OUT_SINE};

const char* const ALPHA_FUNCTION_BUTTON_LABELS[ALPHA_CASE_COUNT] = {
  "Linear",
  "In Square",
  "Out Square",
  "In Cubic",
  "Out Cubic",
  "In-Out",
  "In-Out Sine"};

const char* const TEXT_BUTTON_LABELS[TEXT_CASE_COUNT] = {
  "English",
  "Korean",
  "Bidi",
  "Emoji",
  "Long",
  "Image Local",
  "Image Remote",
  "Short Tail"};

const Text::Reveal::Sequence SEQUENCES[SEQUENCE_CASE_COUNT] = {
  Text::Reveal::Sequence::WHOLE_TEXT,
  Text::Reveal::Sequence::PER_LINE};

const char* const SEQUENCE_BUTTON_LABELS[SEQUENCE_CASE_COUNT] = {
  "Whole Text",
  "Per Line"};

const char* const SEQUENCE_STATUS_LABELS[SEQUENCE_CASE_COUNT] = {
  "WHOLE_TEXT",
  "PER_LINE"};

const float STAGGER_RATIOS[STAGGER_CASE_COUNT] = {
  0.0f,
  0.05f,
  0.10f,
  0.20f,
  0.25f,
  0.50f,
  0.75f,
  1.0f};

const char* const STAGGER_BUTTON_LABELS[STAGGER_CASE_COUNT] = {
  "0",
  "0.05",
  "0.10",
  "0.20",
  "0.25",
  "0.50",
  "0.75",
  "1.00"};

// Every sequence gets the same blur interval relative to the common sequence reference.
// Its actual final end is fitted to DURATION, without reserving an idle tail.
const uint32_t    BLUR_RADII[BLUR_RADIUS_CASE_COUNT]         = {4u, 8u, 12u, 16u, 24u, 32u, 48u, 64u};
const char* const BLUR_RADIUS_LABELS[BLUR_RADIUS_CASE_COUNT] = {
  "4 px", "8 px", "12 px", "16 px", "24 px", "32 px", "48 px", "64 px"};
const float       BLUR_END_PROGRESS[BLUR_END_CASE_COUNT] = {0.0f, 0.05f, 0.10f, 0.20f, 0.25f, 0.40f, 0.50f, 0.75f, 1.0f};
const char* const BLUR_END_LABELS[BLUR_END_CASE_COUNT]   = {
  "0", "0.05", "0.10", "0.20", "0.25", "0.40", "0.50", "0.75", "1.00"};

const char* const TEXT_CASES[TEXT_CASE_COUNT] = {
  "Good morning. Start the day with cafe\u0301 by the window, "
  "a design review at 11:00, and a quiet walk by the river before sunset.",
  "오늘의 일정이 준비되었습니다.\n"
  "오전에는 천천히 커피를 즐기고, 오후 3시에는 Atlas 룸에서 "
  "디자인 리뷰를 진행한 뒤 해 질 무렵 산책을 떠나보세요.",
  "Flight SK204 to دبي departs at 18:30.\n"
  "Dinner in תל אביב begins at 20:10.",
  "Build complete 👩‍💻✨ — time to close the laptop.\n"
  "Dinner 🍜, a high-five 👍🏽, then a family walk 👨‍👩‍👧‍👦 under the city lights 🌙.",
  "Saturday is yours to explore. Start with brunch at 11:30, wander through "
  "the museum at 14:00, and reach the riverside just before sunset. Keep the "
  "evening open for a small jazz bar tucked behind the old market.\n"
  "토요일은 천천히 즐겨보세요. 오전 11시 30분 브런치로 시작해 오후 2시 "
  "전시를 보고, 해 질 무렵에는 강변을 걸어보세요.",
  "Tonight in Seoul: warm lights, late cafés, and a quiet walk along the Han River.",
  "Next stop, San Francisco: morning coffee, cool fog, and a sunset walk along the waterfront.",
  "A quiet morning brings warm coffee and a little time to breathe.\n"
  "The afternoon is yours to explore, one small discovery at a time.\n"
  "Go."};

// Each GradientSpan fill keeps the ordinary foreground color outside these
// words, and applies Linear, Radial and Conic SPAN_BOUND gradients in order.
const char* const GRADIENT_SPAN_WORDS[TEXT_CASE_COUNT][3u] = {
  {"morning", "design", "river"},
  {"오전", "Atlas", "산책"},
  {"SK204", "دبي", "תל אביב"},
  {"Build", "Dinner", "city"},
  {"Saturday", "museum", "강변"},
  {"Seoul", "cafés", "Han River"},
  {"San Francisco", "coffee", "waterfront"},
  {"morning", "afternoon", "Go"}};

Gradient::Base CreateGradientSpanGradient(SpanGradientKind kind)
{
  Gradient::Base gradient;
  switch(kind)
  {
    case SpanGradientKind::RADIAL:
    {
      gradient = Gradient::Radial(Vector2::ZERO, 0.55f);
      gradient.SetStopNodes({Gradient::StopNode(0.0f, UiColor(0xFACC15)),
                             Gradient::StopNode(0.45f, UiColor(0x16A34A)),
                             Gradient::StopNode(1.0f, UiColor(0x0891B2))});
      break;
    }
    case SpanGradientKind::CONIC:
    {
      gradient = Gradient::Conic(Vector2::ZERO, Radian(0.0f));
      gradient.SetStopNodes({Gradient::StopNode(0.0f, UiColor(0xDB2777)),
                             Gradient::StopNode(0.34f, UiColor(0x7C3AED)),
                             Gradient::StopNode(0.68f, UiColor(0x2563EB)),
                             Gradient::StopNode(1.0f, UiColor(0xDB2777))});
      break;
    }
    case SpanGradientKind::LINEAR:
    default:
    {
      gradient = Gradient::Linear(Vector2(-0.5f, 0.0f), Vector2(0.5f, 0.0f));
      gradient.SetStopNodes({Gradient::StopNode(0.0f, UiColor(0xDC2626)),
                             Gradient::StopNode(0.5f, UiColor(0xF97316)),
                             Gradient::StopNode(1.0f, UiColor(0xEAB308))});
      break;
    }
  }
  gradient.SetUnits(Gradient::Units::OBJECT_BOUNDING_BOX);
  return gradient;
}

Label NewLabel(const char* text, float size, uint32_t color)
{
  Label label = Label::New(text);
  label.SetFontSize(size);
  label.SetFontFamily("SamsungOneUI_500");
  label.SetTextColor(UiColor(color));
  label.SetMultiLine(true);
  label.SetRequestedWidth(MATCH_PARENT);
  return label;
}

Label NewButton(const char* text)
{
  Label button = NewLabel(text, 12.0f, 0xCBD5E1);
  button.SetBackgroundColor(UiColor(BUTTON_COLOR));
  button.SetBorderlineWidth(1.0f);
  button.SetBorderlineOffset(-1.0f);
  button.SetBorderlineColor(UiColor(BUTTON_BORDER_COLOR));
  button.SetPadding(Insets(4.0f, 4.0f, 0.0f, 0.0f));
  button.SetMultiLine(false);
  button.SetHorizontalTextAlignment(Text::Alignment::CENTER);
  button.SetVerticalTextAlignment(Text::Alignment::CENTER);
  button.SetRequestedHeight(CONTROL_HEIGHT);
  button.SetCornerRadius(7.0f);
  button.SetLayoutParams(StackLayoutParams::New().SetWeight(1.0f).SetAlignment(LayoutAlignment::FILL));
  return button;
}

Label NewMenuTitle(const char* text)
{
  Label title = NewLabel(text, 12.0f, 0xF8FAFC);
  title.SetRequestedWidth(MENU_TITLE_WIDTH);
  title.SetRequestedHeight(CONTROL_HEIGHT);
  title.SetBackgroundColor(UiColor(SELECTED_BUTTON_COLOR));
  title.SetBorderlineWidth(1.0f);
  title.SetBorderlineOffset(-1.0f);
  title.SetBorderlineColor(UiColor(SELECTED_BORDER_COLOR));
  title.SetPadding(Insets(8.0f, 8.0f, 0.0f, 0.0f));
  title.SetMultiLine(false);
  title.SetHorizontalTextAlignment(Text::Alignment::START);
  title.SetVerticalTextAlignment(Text::Alignment::CENTER);
  title.SetCornerRadius(7.0f);
  return title;
}

StackLayout NewMenuRow(const char* title)
{
  StackLayout row = StackLayout::New(StackOrientation::HORIZONTAL);
  row.SetRequestedWidth(MATCH_PARENT);
  row.SetRequestedHeight(CONTROL_HEIGHT);
  row.SetSpacing(CONTROL_SPACING);
  row.Add(NewMenuTitle(title));
  return row;
}

void SetButtonSelected(Label button, bool selected)
{
  button.SetTextColor(UiColor(selected ? 0xF8FAFC : 0xCBD5E1));
  button.SetBackgroundColor(UiColor(selected ? SELECTED_BUTTON_COLOR : BUTTON_COLOR));
  button.SetBorderlineColor(UiColor(selected ? SELECTED_BORDER_COLOR : BUTTON_BORDER_COLOR));
}
} // unnamed namespace

class TextRevealController : public ConnectionTracker
{
public:
  explicit TextRevealController(Application& application)
  : mApplication(application)
  {
    mApplication.InitSignal().Connect(this, &TextRevealController::OnInit);
  }

  ~TextRevealController()
  {
    StopAnimation();
  }

private:
  void OnInit(Application application)
  {
    Window window = application.GetWindow();
    window.SetBackgroundColor(UiColor(0xF8FAFC));
    window.KeyEventSignal().Connect(this, &TextRevealController::OnKeyEvent);
    window.ResizedSignal().Connect(this, [this](Window, Window::WindowSize)
    {
      UpdateControlsViewport();
    });
    UiScaleManager::Get().SetScale(mUiScale);

    StackLayout root = StackLayout::New(StackOrientation::VERTICAL);
    root.SetRequestedWidth(MATCH_PARENT);
    root.SetRequestedHeight(MATCH_PARENT);
    root.SetSpacing(0.0f);
    root.SetBackgroundColor(UiColor(0xF8FAFC));

    StackLayout content = StackLayout::New(StackOrientation::VERTICAL);
    content.SetRequestedWidth(MATCH_PARENT);
    content.SetRequestedHeight(MATCH_PARENT);
    content.SetPadding(Insets(16.0f, 16.0f, 16.0f, 16.0f));
    content.SetLayoutParams(StackLayoutParams::New().SetWeight(1.0f).SetAlignment(LayoutAlignment::FILL));

    mPreview = NewLabel(TEXT_CASES[mTextCaseIndex], 28.0f, 0x0F172A);
    mPreview.SetLineHeight(1.25f);
    mPreview.SetPadding(Insets(18.0f, 18.0f, 16.0f, 16.0f));
    mPreview.SetBackgroundColor(UiColor(0xFFFFFF));
    mPreview.SetBorderlineWidth(1.0f);
    mPreview.SetBorderlineOffset(-1.0f);
    mPreview.SetBorderlineColor(UiColor(0xCBD5E1));
    mPreview.SetCornerRadius(8.0f);
    mPreview.SetTextOverflowMode(Text::OverflowMode::ELLIPSIS);
    mPreview.SetLayoutParams(StackLayoutParams::New().SetWeight(1.0f).SetAlignment(LayoutAlignment::FILL));
    content.Add(mPreview);

    StackLayout configurationControls = NewMenuRow("CONFIG");
    mUnitButton                       = NewButton("");
    mAsyncButton                      = NewButton("");
    mFillButton                       = NewButton("");
    mRevealButton                     = NewButton("");
    configurationControls.Add(mUnitButton);
    configurationControls.Add(mAsyncButton);
    configurationControls.Add(mFillButton);
    configurationControls.Add(mRevealButton);

    StackLayout uxControls = NewMenuRow("PRESET");
    Label       firstUx    = NewButton("Preset 1");
    Label       secondUx   = NewButton("Preset 2");
    uxControls.Add(firstUx);
    uxControls.Add(secondUx);
    firstUx.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      PlayUxTest(false);
    });
    secondUx.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      PlayUxTest(true);
    });

    StackLayout textControls = NewMenuRow("TEXT");
    for(std::size_t textIndex = 0u; textIndex < TEXT_CASE_COUNT; ++textIndex)
    {
      mTextButtons[textIndex] = NewButton(TEXT_BUTTON_LABELS[textIndex]);
      textControls.Add(mTextButtons[textIndex]);
      mTextButtons[textIndex].AsInteractive().ClickedSignal().Connect(this, [this, textIndex](View, InputEvent)
      {
        SetTextCase(textIndex);
      });
    }

    StackLayout sequenceControls = NewMenuRow("SEQUENCE");
    for(std::size_t sequenceIndex = 0u; sequenceIndex < SEQUENCE_CASE_COUNT; ++sequenceIndex)
    {
      mSequenceButtons[sequenceIndex] = NewButton(SEQUENCE_BUTTON_LABELS[sequenceIndex]);
      sequenceControls.Add(mSequenceButtons[sequenceIndex]);
      mSequenceButtons[sequenceIndex].AsInteractive().ClickedSignal().Connect(this, [this, sequenceIndex](View, InputEvent)
      {
        mSequenceIndex = sequenceIndex;
        ConfigureAndReplay();
      });
    }

    StackLayout fadeControls = NewMenuRow("FADE");
    for(std::size_t fadeIndex = 0u; fadeIndex < FADE_CASE_COUNT; ++fadeIndex)
    {
      mFadeButtons[fadeIndex] = NewButton(FADE_BUTTON_LABELS[fadeIndex]);
      fadeControls.Add(mFadeButtons[fadeIndex]);
      mFadeButtons[fadeIndex].AsInteractive().ClickedSignal().Connect(this, [this, fadeIndex](View, InputEvent)
      {
        mFadeDurationRatioIndex = fadeIndex;
        ConfigureAndReplay();
      });
    }

    StackLayout staggerControls = NewMenuRow("STAGGER");
    for(std::size_t staggerIndex = 0u; staggerIndex < STAGGER_CASE_COUNT; ++staggerIndex)
    {
      mStaggerButtons[staggerIndex] = NewButton(STAGGER_BUTTON_LABELS[staggerIndex]);
      staggerControls.Add(mStaggerButtons[staggerIndex]);
      mStaggerButtons[staggerIndex].AsInteractive().ClickedSignal().Connect(this, [this, staggerIndex](View, InputEvent)
      {
        mStaggerIndex = staggerIndex;
        ConfigureAndReplay();
      });
    }

    StackLayout durationControls = NewMenuRow("DURATION");
    for(std::size_t durationIndex = 0u; durationIndex < DURATION_CASE_COUNT; ++durationIndex)
    {
      mDurationButtons[durationIndex] = NewButton(DURATION_BUTTON_LABELS[durationIndex]);
      durationControls.Add(mDurationButtons[durationIndex]);
      mDurationButtons[durationIndex].AsInteractive().ClickedSignal().Connect(this, [this, durationIndex](View, InputEvent)
      {
        mDurationCaseIndex = durationIndex;
        UpdateDurationButtons();
        Replay();
      });
    }

    StackLayout alphaFunctionControls = NewMenuRow("ALPHA");
    for(std::size_t alphaFunctionIndex = 0u; alphaFunctionIndex < ALPHA_CASE_COUNT; ++alphaFunctionIndex)
    {
      mAlphaButtons[alphaFunctionIndex] = NewButton(ALPHA_FUNCTION_BUTTON_LABELS[alphaFunctionIndex]);
      alphaFunctionControls.Add(mAlphaButtons[alphaFunctionIndex]);
      mAlphaButtons[alphaFunctionIndex].AsInteractive().ClickedSignal().Connect(this, [this, alphaFunctionIndex](View, InputEvent)
      {
        mAlphaFunctionIndex = alphaFunctionIndex;
        UpdateAlphaFunctionButtons();
        Replay();
      });
    }

    StackLayout blurRadiusControls = NewMenuRow("BLUR px");
    mBlurButton                    = NewButton("Blur: Off");
    blurRadiusControls.Add(mBlurButton);
    mBlurButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      mBlurEnabled = !mBlurEnabled;
      UpdateBlurButtons();
      Replay();
    });
    for(std::size_t index = 0u; index < BLUR_RADIUS_CASE_COUNT; ++index)
    {
      mBlurRadiusButtons[index] = NewButton(BLUR_RADIUS_LABELS[index]);
      blurRadiusControls.Add(mBlurRadiusButtons[index]);
      mBlurRadiusButtons[index].AsInteractive().ClickedSignal().Connect(this, [this, index](View, InputEvent)
      {
        mBlurRadiusIndex = index;
        mBlurEnabled     = true;
        UpdateBlurButtons();
        Replay();
      });
    }

    StackLayout blurEndControls = NewMenuRow("BLUR TIME");
    for(std::size_t index = 0u; index < BLUR_END_CASE_COUNT; ++index)
    {
      mBlurEndButtons[index] = NewButton(BLUR_END_LABELS[index]);
      blurEndControls.Add(mBlurEndButtons[index]);
      mBlurEndButtons[index].AsInteractive().ClickedSignal().Connect(this, [this, index](View, InputEvent)
      {
        mBlurEndIndex = index;
        UpdateBlurButtons();
        Replay();
      });
    }

    StackLayout playbackControls = NewMenuRow("PLAYBACK");
    Label       replayButton     = NewButton("Replay");
    mStopPlayButton              = NewButton("Stop");
    Label reverseButton          = NewButton("Reverse");
    playbackControls.Add(replayButton);
    playbackControls.Add(mStopPlayButton);
    playbackControls.Add(reverseButton);
    const std::array<float, 5u>       fixedProgress{{0.0f, 0.25f, 0.50f, 0.75f, 1.0f}};
    const std::array<const char*, 5u> progressLabels{{"p 0", "p .25", "p .50", "p .75", "p 1"}};
    for(std::size_t index = 0u; index < fixedProgress.size(); ++index)
    {
      Label button = NewButton(progressLabels[index]);
      playbackControls.Add(button);
      button.AsInteractive().ClickedSignal().Connect(this, [this, progress = fixedProgress[index]](View, InputEvent)
      {
        StopAnimation();
        mPreview.SetTextRevealProgress(progress);
        RestoreUxPreview();
        mStopPlayButton.SetText("Play");
        std::ostringstream playback;
        playback << "Fixed progress " << std::fixed << std::setprecision(2) << progress << ".";
        mPlaybackStatus = playback.str();
        UpdateStatus();
      });
    }

    mStatus = NewLabel("", 12.0f, 0xCBD5E1);
    mStatus.SetBackgroundColor(UiColor(0x1E293B));
    mStatus.SetBorderlineWidth(1.0f);
    mStatus.SetBorderlineOffset(-1.0f);
    mStatus.SetBorderlineColor(UiColor(BUTTON_BORDER_COLOR));
    mStatus.SetPadding(Insets(10.0f, 10.0f, 6.0f, 6.0f));
    mStatus.SetRequestedHeight(WRAP_CONTENT);
    mStatus.SetMinimumHeight(STATUS_HEIGHT);
    mStatus.SetCornerRadius(7.0f);

    StackLayout controlsPanel = StackLayout::New(StackOrientation::VERTICAL);
    controlsPanel.SetRequestedWidth(MATCH_PARENT);
    controlsPanel.SetRequestedHeight(WRAP_CONTENT);
    controlsPanel.SetBackgroundColor(UiColor(PANEL_COLOR));
    controlsPanel.SetPadding(Insets(12.0f, 12.0f, 12.0f, 12.0f));
    controlsPanel.SetSpacing(CONTROL_SPACING);
    controlsPanel.Add(configurationControls);
    controlsPanel.Add(uxControls);
    controlsPanel.Add(textControls);
    controlsPanel.Add(sequenceControls);
    controlsPanel.Add(fadeControls);
    controlsPanel.Add(staggerControls);
    controlsPanel.Add(durationControls);
    controlsPanel.Add(alphaFunctionControls);
    controlsPanel.Add(blurRadiusControls);
    controlsPanel.Add(blurEndControls);
    controlsPanel.Add(playbackControls);
    controlsPanel.Add(mStatus);

    mControlsViewport = ScrollView::New();
    mControlsViewport.SetScrollDirection(ScrollDirection::Vertical);
    mControlsViewport.SetRequestedWidth(MATCH_PARENT);
    mControlsViewport.SetBackgroundColor(UiColor(PANEL_COLOR));
    mControlsViewport.SetContent(controlsPanel);
    UpdateControlsViewport();

    root.Add(content);
    root.Add(mControlsViewport);
    window.Add(root);

    mUnitButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      if(mUnit == Text::Reveal::Unit::CHARACTER)
      {
        mUnit = Text::Reveal::Unit::WORD;
      }
      else if(mUnit == Text::Reveal::Unit::WORD)
      {
        mUnit = Text::Reveal::Unit::LINE;
      }
      else if(mUnit == Text::Reveal::Unit::LINE)
      {
        mUnit = Text::Reveal::Unit::PIXEL;
      }
      else
      {
        mUnit = Text::Reveal::Unit::CHARACTER;
      }
      ConfigureAndReplay();
    });
    mAsyncButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      mAsync = !mAsync;
      ConfigureAndReplay();
    });
    mFillButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      switch(mFillMode)
      {
        case FillMode::TEXT_GRADIENT:
          mFillMode = FillMode::GRADIENT_SPAN;
          break;
        case FillMode::GRADIENT_SPAN:
          mFillMode = FillMode::SOLID;
          break;
        case FillMode::SOLID:
          mFillMode = FillMode::WHITE_ON_BLACK;
          break;
        case FillMode::WHITE_ON_BLACK:
          mFillMode = FillMode::TEXT_GRADIENT;
          break;
      }
      ConfigureAndReplay();
    });
    mRevealButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      if(mUxPhase != UxPlaybackPhase::NONE)
      {
        StopAnimation();
        mStopPlayButton.SetText("Play");
        mPlaybackStatus = "UX playback cancelled by Reveal configuration.";
      }
      mRevealEnabled = !mRevealEnabled;
      ApplyRevealConfiguration();
      if(mUxPreviewHidden)
      {
        mUxPreviewHidden = false;
        mPreview.SetProperty(Actor::Property::VISIBLE, true);
      }
      mRevealButton.SetText(mRevealEnabled ? "Reveal: On" : "Reveal: Off");
      SetButtonSelected(mRevealButton, mRevealEnabled);
      UpdateStatus();
    });
    replayButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      Replay();
    });
    mStopPlayButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      StopOrPlay();
    });
    reverseButton.AsInteractive().ClickedSignal().Connect(this, [this](View, InputEvent)
    {
      Reverse();
    });

    ConfigureAndReplay();
  }

  void ConfigureAndReplay()
  {
    ApplyRevealConfiguration();
    mPreview.SetAsyncRendering(mAsync);
    ApplyFill();

    mUnitButton.SetText(GetUnitButtonLabel());
    mAsyncButton.SetText(mAsync ? "Path: Async" : "Path: Sync");
    mFillButton.SetText(GetFillButtonLabel());
    mRevealButton.SetText(mRevealEnabled ? "Reveal: On" : "Reveal: Off");
    SetButtonSelected(mAsyncButton, mAsync);
    SetButtonSelected(mFillButton, mFillMode != FillMode::SOLID);
    SetButtonSelected(mRevealButton, mRevealEnabled);
    UpdateTextButtons();
    UpdateSequenceButtons();
    UpdateStaggerButtons();
    UpdateFadeButtons();
    UpdateDurationButtons();
    UpdateAlphaFunctionButtons();
    UpdateBlurButtons();
    Replay();
  }

  void SetTextCase(std::size_t textIndex)
  {
    mTextCaseIndex = textIndex % TEXT_CASE_COUNT;
    ApplyFill();
    UpdateTextButtons();
    Replay();
  }

  const char* GetUnitButtonLabel() const
  {
    switch(mUnit)
    {
      case Text::Reveal::Unit::WORD:
        return "Unit: Word";
      case Text::Reveal::Unit::LINE:
        return "Unit: Line";
      case Text::Reveal::Unit::PIXEL:
        return "Unit: Pixel";
      case Text::Reveal::Unit::CHARACTER:
      default:
        return "Unit: Character";
    }
  }

  const char* GetUnitStatusLabel() const
  {
    switch(mUnit)
    {
      case Text::Reveal::Unit::WORD:
        return "WORD";
      case Text::Reveal::Unit::LINE:
        return "LINE";
      case Text::Reveal::Unit::PIXEL:
        return "PIXEL";
      case Text::Reveal::Unit::CHARACTER:
      default:
        return "CHARACTER";
    }
  }

  const char* GetFillButtonLabel() const
  {
    switch(mFillMode)
    {
      case FillMode::SOLID:
        return "Fill: Solid";
      case FillMode::WHITE_ON_BLACK:
        return "Fill: White/Black";
      case FillMode::GRADIENT_SPAN:
        return "Fill: Span";
      case FillMode::TEXT_GRADIENT:
      default:
        return "Fill: Gradient";
    }
  }

  const char* GetFillStatusLabel() const
  {
    switch(mFillMode)
    {
      case FillMode::SOLID:
        return "Solid";
      case FillMode::WHITE_ON_BLACK:
        return "White on black";
      case FillMode::GRADIENT_SPAN:
        return "GradientSpan (Linear/Radial/Conic, SPAN_BOUND)";
      case FillMode::TEXT_GRADIENT:
      default:
        return "TextGradient";
    }
  }

  void ApplyRevealConfiguration()
  {
    if(!mRevealEnabled)
    {
      // Reveal::None() disables the effect while preserving authored progress.
      mPreview.SetTextReveal(Text::Reveal::None());
      return;
    }

    Text::Reveal reveal;
    reveal.SetUnit(mUnit);
    reveal.SetSequence(SEQUENCES[mSequenceIndex]);
    reveal.SetSequenceStaggerRatio(STAGGER_RATIOS[mStaggerIndex]);
    reveal.SetFadeDurationRatio(FADE_DURATION_RATIOS[mFadeDurationRatioIndex]);
    reveal.SetBlurRadius(mBlurEnabled ? static_cast<float>(BLUR_RADII[mBlurRadiusIndex]) : 0.0f);
    reveal.SetBlurDurationRatio(BLUR_END_PROGRESS[mBlurEndIndex]);
    mPreview.SetTextReveal(reveal);
  }

  void UpdateFadeButtons()
  {
    for(std::size_t fadeIndex = 0u; fadeIndex < FADE_CASE_COUNT; ++fadeIndex)
    {
      SetButtonSelected(mFadeButtons[fadeIndex], fadeIndex == mFadeDurationRatioIndex);
    }
  }

  const char* GetBlurBypassReason() const
  {
    if(BLUR_END_PROGRESS[mBlurEndIndex] == 0.0f)
    {
      return "blur duration is zero";
    }
    if(!mRevealEnabled)
    {
      return "Reveal is Off";
    }
    return nullptr;
  }

  void UpdateBlurButtons()
  {
    mBlurButton.SetText(mBlurEnabled ? "Blur: On" : "Blur: Off");
    SetButtonSelected(mBlurButton, mBlurEnabled);
    for(std::size_t index = 0u; index < BLUR_RADIUS_CASE_COUNT; ++index)
    {
      SetButtonSelected(mBlurRadiusButtons[index], mBlurEnabled && index == mBlurRadiusIndex);
    }
    for(std::size_t index = 0u; index < BLUR_END_CASE_COUNT; ++index)
    {
      SetButtonSelected(mBlurEndButtons[index], index == mBlurEndIndex);
    }
  }

  void UpdateSequenceButtons()
  {
    for(std::size_t sequenceIndex = 0u; sequenceIndex < SEQUENCE_CASE_COUNT; ++sequenceIndex)
    {
      SetButtonSelected(mSequenceButtons[sequenceIndex], sequenceIndex == mSequenceIndex);
    }
  }

  void UpdateStaggerButtons()
  {
    for(std::size_t staggerIndex = 0u; staggerIndex < STAGGER_CASE_COUNT; ++staggerIndex)
    {
      SetButtonSelected(mStaggerButtons[staggerIndex], staggerIndex == mStaggerIndex);
    }
  }

  void UpdateDurationButtons()
  {
    for(std::size_t durationIndex = 0u; durationIndex < DURATION_CASE_COUNT; ++durationIndex)
    {
      SetButtonSelected(mDurationButtons[durationIndex], durationIndex == mDurationCaseIndex);
    }
  }

  void UpdateAlphaFunctionButtons()
  {
    for(std::size_t alphaFunctionIndex = 0u; alphaFunctionIndex < ALPHA_CASE_COUNT; ++alphaFunctionIndex)
    {
      SetButtonSelected(mAlphaButtons[alphaFunctionIndex], alphaFunctionIndex == mAlphaFunctionIndex);
    }
  }

  void UpdateTextButtons()
  {
    for(std::size_t textIndex = 0u; textIndex < TEXT_CASE_COUNT; ++textIndex)
    {
      SetButtonSelected(mTextButtons[textIndex], textIndex == mTextCaseIndex);
    }
  }

  void ApplyFill()
  {
    const bool whiteOnBlack = mFillMode == FillMode::WHITE_ON_BLACK;
    mPreview.SetBackgroundColor(UiColor(whiteOnBlack ? 0x000000 : 0xFFFFFF));
    mPreview.SetTextColor(UiColor(whiteOnBlack ? 0xFFFFFF : 0x0F172A));
    mPreview.SetTextGradient(Gradient::Base::None());

    const bool isImageText = mTextCaseIndex == LOCAL_IMAGE_CASE_INDEX ||
                             mTextCaseIndex == REMOTE_IMAGE_CASE_INDEX;
    if(mFillMode == FillMode::GRADIENT_SPAN || isImageText)
    {
      mPreview.SetStyledText(BuildCurrentStyledText(mFillMode == FillMode::GRADIENT_SPAN));
    }
    else
    {
      mPreview.SetText(TEXT_CASES[mTextCaseIndex]);
    }

    if(mFillMode != FillMode::TEXT_GRADIENT)
    {
      return;
    }

    Gradient::Linear gradient(Vector2(-0.5f, 0.0f), Vector2(0.5f, 0.0f));
    gradient.SetUnits(Gradient::Units::OBJECT_BOUNDING_BOX);
    gradient.SetStopNodes({Gradient::StopNode(0.0f, UiColor(0x2563EB)),
                           Gradient::StopNode(0.5f, UiColor(0x7C3AED)),
                           Gradient::StopNode(1.0f, UiColor(0xEA580C))});
    mPreview.SetTextGradient(gradient);
  }

  Text::StyledText BuildCurrentStyledText(bool applyGradientSpans) const
  {
    const std::string       source(TEXT_CASES[mTextCaseIndex]);
    Text::StyledTextBuilder builder = Text::StyledTextBuilder::New(source.c_str());

    if(mTextCaseIndex == LOCAL_IMAGE_CASE_INDEX || mTextCaseIndex == REMOTE_IMAGE_CASE_INDEX)
    {
      builder.AppendText(" ");
      const uint32_t imageIndex = builder.GetUtf32Length();
      builder.AppendText(Text::ReplacementSpan::OBJECT_REPLACEMENT_CHARACTER);

      const bool            remoteImage = mTextCaseIndex == REMOTE_IMAGE_CASE_INDEX;
      Text::ImageAttributes imageAttributes(remoteImage ? REMOTE_IMAGE_URL : RESOURCES_DIR "flag_kr.png",
                                            remoteImage ? Vector2(96.0f, 69.0f) : Vector2(48.0f, 32.0f));
      imageAttributes.SetAlignment(Text::ImageAttributes::InlineAlignment::TEXT_CENTER);
      DALI_ASSERT_ALWAYS(builder.SetSpan(Text::ImageSpan::New(imageAttributes),
                                         imageIndex,
                                         imageIndex + 1u));
    }

    if(applyGradientSpans)
    {
      const SpanGradientKind kinds[3u] = {
        SpanGradientKind::LINEAR,
        SpanGradientKind::RADIAL,
        SpanGradientKind::CONIC};
      for(std::size_t index = 0u; index < 3u; ++index)
      {
        const char*       word      = GRADIENT_SPAN_WORDS[mTextCaseIndex][index];
        const std::size_t byteStart = source.find(word);
        DALI_ASSERT_ALWAYS(byteStart != std::string::npos);

        uint32_t utf32Start = 0u;
        uint32_t utf32End   = 0u;
        DALI_ASSERT_ALWAYS(Text::Utf8ToUtf32Range(Dali::StringView(source.data(), static_cast<uint32_t>(source.size())),
                                                  static_cast<uint32_t>(byteStart),
                                                  static_cast<uint32_t>(byteStart + std::string(word).size()),
                                                  utf32Start,
                                                  utf32End));
        DALI_ASSERT_ALWAYS(builder.SetSpan(Text::GradientSpan::New(CreateGradientSpanGradient(kinds[index]),
                                                                   Text::GradientSpan::BoundsMode::SPAN_BOUND),
                                           utf32Start,
                                           utf32End));
      }
    }

    return builder.Build();
  }

  float GetAnimationDuration() const
  {
    return DURATION_SECONDS[mDurationCaseIndex];
  }

  void ApplyPreset(bool exiting, bool secondEntrance)
  {
    // Preserve the selected text, fill, Unit and render path. The status still
    // reports configurations where this prototype cannot apply runtime blur.
    mRevealEnabled          = true;
    mBlurEnabled            = true;
    mSequenceIndex          = exiting ? 0u : 1u;                     // WHOLE_TEXT / PER_LINE
    mFadeDurationRatioIndex = exiting ? 6u : 1u;                     // Fade 1 / 0
    mStaggerIndex           = exiting || secondEntrance ? 0u : 4u;  // 0 / 0.25
    mDurationCaseIndex      = exiting || secondEntrance ? 1u : 2u;  // 1 s / 2 s
    mAlphaFunctionIndex     = exiting ? 0u : 2u;                     // LINEAR / EASE_OUT_SQUARE
    mBlurRadiusIndex        = exiting ? 6u : 4u;                     // 48 px / 24 px
    mBlurEndIndex           = exiting || secondEntrance ? 8u : 6u;  // 1.0 / 0.5
    ApplyRevealConfiguration();
    mRevealButton.SetText("Reveal: On");
    SetButtonSelected(mRevealButton, true);
    UpdateSequenceButtons();
    UpdateFadeButtons();
    UpdateStaggerButtons();
    UpdateDurationButtons();
    UpdateAlphaFunctionButtons();
    UpdateBlurButtons();
  }

  void PlayUxTest(bool secondEntrance)
  {
    StopAnimation();
    mUxSecondEntrance = secondEntrance;
    ApplyPreset(false, secondEntrance);
    mPreview.SetTextRevealProgress(0.0f);
    RestoreUxPreview();
    mUxPhase        = UxPlaybackPhase::ENTERING;
    mPlaybackStatus = secondEntrance ? "Preset 2: entering for 1 s (Out Square), then wait 1 s."
                                      : "Preset 1: entering for 2 s (Out Square), then wait 1 s.";
    StartAnimation(1.0f, GetAnimationDuration());
  }

  void RestoreUxPreview()
  {
    if(mUxPreviewHidden)
    {
      mUxPreviewHidden = false;
      mRevealEnabled  = true;
      ApplyRevealConfiguration();
      mRevealButton.SetText("Reveal: On");
      SetButtonSelected(mRevealButton, true);
      mPreview.SetProperty(Actor::Property::VISIBLE, true);
    }
  }

  void WaitForUxExit()
  {
    // Start the hold after actual animation completion, not an estimated
    // creation timestamp. A cancelled run must never start a delayed exit.
    mUxPhase        = UxPlaybackPhase::WAITING;
    mPlaybackStatus = "Preset: fully visible; waiting 1 s before changing to the exit settings.";
    mStopPlayButton.SetText("Stop");
    const uint32_t run = mUxRun;
    mUxWaitTimer       = Timer::New(1000u);
    mUxWaitTimer.TickSignal().Connect(this, [this, run]() -> bool
    {
      if(run != mUxRun || mUxPhase != UxPlaybackPhase::WAITING)
      {
        return false;
      }
      mUxPhase = UxPlaybackPhase::EXITING;
      ApplyPreset(true, mUxSecondEntrance);
      mPlaybackStatus = "Preset: exiting for 1 s (Linear), Whole Text, fade 1.0, blur 48 px / time 1.0.";
      StartAnimation(0.0f, 1.0f);
      return false;
    });
    mUxWaitTimer.Start();
    UpdateStatus();
  }

  void FinishUxTest()
  {
    mUxPhase = UxPlaybackPhase::NONE;
    mUxWaitTimer.Stop();
    mUxWaitTimer.Reset();
    // None restores ordinary text. Hide the preview first so cleanup does not
    // make the text reappear; Replay or a manual progress change restores it.
    mPreview.SetProperty(Actor::Property::VISIBLE, false);
    mUxPreviewHidden = true;
    mRevealEnabled   = false;
    ApplyRevealConfiguration();
    mRevealButton.SetText("Reveal: Off");
    SetButtonSelected(mRevealButton, false);
    mPlaybackStatus = "Preset finished: preview hidden and Reveal::None() applied. Replay or PRESET restores it.";
    UpdateStatus();
  }

  void Replay()
  {
    StopAnimation();
    ApplyRevealConfiguration();
    mPreview.SetTextRevealProgress(0.0f);
    RestoreUxPreview();
    const float duration = GetAnimationDuration();

    std::ostringstream playback;
    playback << "Replay: progress 0.00 -> 1.00 over " << std::fixed << std::setprecision(1)
             << duration << " s.";
    mPlaybackStatus = playback.str();
    StartAnimation(1.0f, duration);
  }

  void StopOrPlay()
  {
    if(mAnimationRunning || mUxPhase == UxPlaybackPhase::WAITING)
    {
      const float progress = mPreview.GetTextRevealProgress();
      StopAnimation();
      mPreview.SetTextRevealProgress(progress);
      mStopPlayButton.SetText("Play");

      std::ostringstream playback;
      playback << "Stopped at progress " << std::fixed << std::setprecision(2) << progress << ".";
      mPlaybackStatus = playback.str();
      UpdateStatus();
      return;
    }

    const float progress = mPreview.GetTextRevealProgress();
    if(progress >= 1.0f)
    {
      Replay();
      return;
    }

    // Scale the resume duration by the remaining progress.
    RestoreUxPreview();
    const float        remainingDuration = GetAnimationDuration() * (1.0f - progress);
    std::ostringstream playback;
    playback << "Play: progress " << std::fixed << std::setprecision(2) << progress
             << " -> 1.00 over " << std::setprecision(1) << remainingDuration << " s.";
    mPlaybackStatus = playback.str();
    StartAnimation(1.0f, remainingDuration);
  }

  void Reverse()
  {
    const float progress = mPreview.GetTextRevealProgress();
    StopAnimation();
    mPreview.SetTextRevealProgress(progress);
    RestoreUxPreview();

    if(progress <= 0.0f)
    {
      mStopPlayButton.SetText("Play");
      mPlaybackStatus = "Already hidden at progress 0.00.";
      UpdateStatus();
      return;
    }

    const float        reverseDuration = GetAnimationDuration() * progress;
    std::ostringstream playback;
    playback << "Reverse: progress " << std::fixed << std::setprecision(2) << progress
             << " -> 0.00 over " << std::setprecision(1) << reverseDuration << " s.";
    mPlaybackStatus = playback.str();
    StartAnimation(0.0f, reverseDuration);
  }

  void StartAnimation(float target, float duration)
  {
    mAnimationTarget  = target;
    mAnimationRunning = true;
    mStopPlayButton.SetText("Stop");
    mAnimation = Animation::New(duration);
    mPreview.Animate(mAnimation).TextRevealProgress(target, Duration(duration), AlphaFunction(ALPHA_FUNCTIONS[mAlphaFunctionIndex]));
    mAnimation.FinishedSignal().Connect(this, [this](Animation animation)
    {
      if(animation != mAnimation)
      {
        return;
      }

      mAnimationRunning = false;
      mAnimation.Reset();
      mStopPlayButton.SetText("Play");
      if(mUxPhase == UxPlaybackPhase::ENTERING)
      {
        WaitForUxExit();
        return;
      }
      if(mUxPhase == UxPlaybackPhase::EXITING)
      {
        FinishUxTest();
        return;
      }
      mPlaybackStatus = mAnimationTarget > 0.5f ? "Finished at progress 1.00."
                                                : "Reverse finished at progress 0.00.";
      UpdateStatus();
    });
    mAnimation.Play();
    UpdateStatus();
  }

  void StopAnimation()
  {
    ++mUxRun;
    mUxPhase = UxPlaybackPhase::NONE;
    if(mUxWaitTimer)
    {
      mUxWaitTimer.Stop();
      mUxWaitTimer.Reset();
    }
    if(mAnimation)
    {
      mAnimation.Stop();
      mAnimation.Reset();
    }
    mAnimationRunning = false;
  }

  const char* GetFadeDescription() const
  {
    switch(mFadeDurationRatioIndex)
    {
      case 0u:
        if(mUnit == Text::Reveal::Unit::PIXEL)
        {
          return "Auto: fade adapts to the visible spatial range and text scale.";
        }
        return mUnit == Text::Reveal::Unit::LINE
                 ? "Auto: fade adapts to the final visible line count."
                 : "Auto: fade adapts to the final visible character or word count.";
      case 1u:
        if(mUnit == Text::Reveal::Unit::PIXEL)
        {
          return "Fade 0.00: the foreground advances continuously as a hard reveal front.";
        }
        if(mUnit == Text::Reveal::Unit::CHARACTER)
        {
          return "Fade 0.00: each shaping cluster appears at its scheduled progress.";
        }
        return mUnit == Text::Reveal::Unit::LINE
                 ? "Fade 0.00: each final layout line appears at its scheduled progress."
                 : "Fade 0.00: each word appears at its scheduled progress.";
      case 2u:
        return "Fade 0.10: a short transition between reveal states.";
      case 3u:
        return "Fade 0.25: a smooth transition with moderate overlap.";
      case 4u:
        return "Fade 0.50: longer fades create stronger overlap.";
      case 5u:
        return "Fade 0.75: long transitions overlap across most of the reveal.";
      case 6u:
        return "Fade 1.00: units within each sequence share the same fade interval.";
    }
    return "";
  }

  void UpdateStatus()
  {
    const char*        revealDescription = mRevealEnabled
                                             ? GetFadeDescription()
                                             : "Reveal is unset with Text::Reveal::None(); progress is preserved.";
    std::ostringstream status;
    status << "TEXT " << TEXT_BUTTON_LABELS[mTextCaseIndex]
           << " | UNIT " << GetUnitStatusLabel()
           << " | SEQUENCE " << SEQUENCE_STATUS_LABELS[mSequenceIndex]
           << " | FADE " << FADE_STATUS_LABELS[mFadeDurationRatioIndex]
           << " | STAGGER " << STAGGER_BUTTON_LABELS[mStaggerIndex]
           << " | DURATION " << std::fixed << std::setprecision(1) << GetAnimationDuration() << " s"
           << " | ALPHA " << ALPHA_FUNCTION_BUTTON_LABELS[mAlphaFunctionIndex]
           << "\nPATH " << (mAsync ? "Async" : "Sync")
           << " | FILL " << GetFillStatusLabel()
           << " | REVEAL " << (mRevealEnabled ? "On" : "Off")
           << " | UI SCALE " << std::setprecision(1) << mUiScale << 'x'
           << "\n"
           << revealDescription
           << "\n"
           << mPlaybackStatus
           << "\nBLUR " << (mBlurEnabled ? "On" : "Off")
           << " | RADIUS " << BLUR_RADII[mBlurRadiusIndex] << " px"
           << " | TIME ratio " << BLUR_END_LABELS[mBlurEndIndex];
    if(mBlurEnabled && GetBlurBypassReason())
    {
      status << " | BYPASSED: " << GetBlurBypassReason();
    }
    else if(mBlurEnabled)
    {
      status << " | Shared sequence-relative blur interval"
             << " | TOTAL " << std::setprecision(2) << GetAnimationDuration() << " s";
    }
    status << "\nDURATION includes Reveal + blur. Their actual final end is fitted to this time; no extra tail is reserved.";
    status << "\nBLUR TIME is relative to one common sequence interval, not the full Animation. ALPHA affects both Reveal and blur.";
    status << "\np=1 is fully sharp. Reverse retraces the combined progress. Later units can enter sharp; reflow remaps the current clock to the new layout.";
    if(mTextCaseIndex == LOCAL_IMAGE_CASE_INDEX || mTextCaseIndex == REMOTE_IMAGE_CASE_INDEX)
    {
      status << " ImageSpan shares its sequence's Reveal and blur timing.";
    }
    status << "\nVIEW  Q/W/E/R/T: scale 0.8/1.0/1.2/1.4/2.0 | Scroll controls if clipped | ESC: Exit";
    mStatus.SetText(status.str().c_str());
  }

  void UpdateControlsViewport()
  {
    if(mControlsViewport)
    {
      // Keep a visible preview on small windows and at larger UI scales.
      const float height = static_cast<float>(mApplication.GetWindow().GetPositionSize().height);
      mControlsViewport.SetRequestedHeight(std::min(CONTROLS_PANEL_HEIGHT, height * 0.60f / mUiScale));
    }
  }

  void SetUiScale(float scale)
  {
    mUiScale = scale;
    UiScaleManager::Get().SetScale(scale);
    UpdateControlsViewport();
    if(mBlurEnabled)
    {
      Replay();
    }
    else
    {
      UpdateStatus();
    }
  }

  void OnKeyEvent(Window /*window*/, KeyEvent event)
  {
    if(event.GetState() != KeyEvent::UP)
    {
      return;
    }

    if(IsKey(event, Dali::DALI_KEY_ESCAPE) || IsKey(event, Dali::DALI_KEY_BACK))
    {
      StopAnimation();
      mApplication.Quit();
      return;
    }

    const Dali::String& keyName = event.GetKeyName();
    if(keyName == "q" || keyName == "Q")
    {
      SetUiScale(0.8f);
    }
    else if(keyName == "w" || keyName == "W")
    {
      SetUiScale(1.0f);
    }
    else if(keyName == "e" || keyName == "E")
    {
      SetUiScale(1.2f);
    }
    else if(keyName == "r" || keyName == "R")
    {
      SetUiScale(1.4f);
    }
    else if(keyName == "t" || keyName == "T")
    {
      SetUiScale(2.0f);
    }
  }

private:
  Application&                              mApplication;
  Label                                     mPreview;
  Label                                     mUnitButton;
  std::array<Label, TEXT_CASE_COUNT>        mTextButtons;
  std::array<Label, SEQUENCE_CASE_COUNT>    mSequenceButtons;
  std::array<Label, STAGGER_CASE_COUNT>     mStaggerButtons;
  std::array<Label, FADE_CASE_COUNT>        mFadeButtons;
  std::array<Label, DURATION_CASE_COUNT>    mDurationButtons;
  std::array<Label, ALPHA_CASE_COUNT>       mAlphaButtons;
  std::array<Label, BLUR_RADIUS_CASE_COUNT> mBlurRadiusButtons;
  std::array<Label, BLUR_END_CASE_COUNT>    mBlurEndButtons;
  Label                                     mBlurButton;
  ScrollView                                mControlsViewport;
  Label                                     mAsyncButton;
  Label                                     mFillButton;
  Label                                     mRevealButton;
  Label                                     mStopPlayButton;
  Label                                     mStatus;
  Animation                                 mAnimation;
  Timer                                     mUxWaitTimer;
  UxPlaybackPhase                           mUxPhase{UxPlaybackPhase::NONE};
  uint32_t                                  mUxRun{0u};
  bool                                      mUxSecondEntrance{false};
  bool                                      mUxPreviewHidden{false};
  std::string                               mPlaybackStatus;
  Text::Reveal::Unit                        mUnit{Text::Reveal::Unit::CHARACTER};
  std::size_t                               mSequenceIndex{0u};
  std::size_t                               mStaggerIndex{0u};
  std::size_t                               mTextCaseIndex{DEFAULT_TEXT_CASE_INDEX};
  std::size_t                               mFadeDurationRatioIndex{0u};
  std::size_t                               mDurationCaseIndex{DEFAULT_DURATION_CASE_INDEX};
  std::size_t                               mAlphaFunctionIndex{0u};
  std::size_t                               mBlurRadiusIndex{5u};
  std::size_t                               mBlurEndIndex{8u};
  bool                                      mBlurEnabled{false};
  bool                                      mAsync{false};
  FillMode                                  mFillMode{FillMode::TEXT_GRADIENT};
  bool                                      mRevealEnabled{true};
  bool                                      mAnimationRunning{false};
  float                                     mAnimationTarget{1.0f};
  float                                     mUiScale{1.0f};
};

int DALI_EXPORT_API main(int argc, char** argv)
{
  Application application = Application::New(&argc, &argv);
  UiConfig::New().Apply();
  TextRevealController controller(application);
  application.MainLoop();
  return 0;
}
