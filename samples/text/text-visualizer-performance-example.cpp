/* Copyright (c) 2026 Samsung Electronics Co., Ltd.
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
 */

#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali-ui-foundation/devel-api/ui-foundation-pre-initialize.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

using namespace Dali;
using namespace Dali::Ui;

namespace
{
constexpr float WINDOW_WIDTH  = 1920.0f;
constexpr float WINDOW_HEIGHT = 1080.0f;

constexpr float SIDE_MARGIN         = 86.0f;
constexpr float TITLE_TOP           = 58.0f;
constexpr float TITLE_HEIGHT        = 310.0f;
constexpr float CONTENT_TOP         = 400.0f;
constexpr float CONTENT_HEIGHT      = 620.0f;
constexpr float CONTENT_WIDTH       = WINDOW_WIDTH - (SIDE_MARGIN * 2.0f);
constexpr float COLUMN_GAP          = 58.0f;
constexpr float COLUMN_WIDTH        = (CONTENT_WIDTH - (COLUMN_GAP * 2.0f)) / 3.0f;
constexpr float COLUMN_SPLIT_WIDTH  = COLUMN_GAP;
constexpr float STATUS_HEIGHT       = 36.0f;
constexpr float STATUS_BOTTOM       = 16.0f;
constexpr float TITLE_FONT_SIZE     = 104.0f;
constexpr float BODY_FONT_SIZE      = 12.0f;
constexpr float QUOTE_FONT_SIZE     = 12.0f;
constexpr float BODY_LINE_HEIGHT    = 2.6f;
constexpr float QUOTE_LINE_HEIGHT   = 2.2f;
constexpr float MIN_FONT_SIZE       = 12.0f;
constexpr float MAX_FONT_SIZE       = 24.0f;
constexpr float FONT_STEP           = 2.0f;
constexpr uint32_t MAX_ORB_COUNT    = 6u;
constexpr uint32_t MIN_ORB_COUNT    = 2u;
constexpr uint32_t DEFAULT_ORB_COUNT = 5u;
constexpr uint32_t STATUS_INTERVAL  = 10u;
constexpr uint32_t TIMER_INTERVAL_MS = 16u;

const UiColor WINDOW_BG_COLOR(0x0B0B10);
const UiColor TITLE_COLOR(0xF4F1EC);
const UiColor ACCENT_COLOR(0xC9A45D);
const UiColor STATUS_TEXT_COLOR = UiColor(0xFFFFFF).WithAlpha(0.45f);
const UiColor QUOTE_BG_COLOR = UiColor(0x16161C).WithAlpha(0.70f);
const UiColor QUOTE_TEXT_COLOR(0xCFB06A);

const UiColor TEXT_COLORS[] = {
  UiColor(0xF1E8D2),
  UiColor(0xF2E7C6),
  UiColor(0xEDE5D4),
  UiColor(0xF0DEB5),
};

const char* TEXT_COLOR_NAMES[] = {
  "Ivory",
  "Warm",
  "Paper",
  "Amber",
};

struct OrbPreset
{
  Vector2 position;
  Vector2 size;
  Vector2 velocity;
  UiColor color;
};

const OrbPreset ORB_PRESETS[MAX_ORB_COUNT] = {
  {Vector2(168.0f, 218.0f),  Vector2(172.0f, 172.0f), Vector2(54.0f, 34.0f), UiColor(0x2D355C).WithAlpha(0.92f)},
  {Vector2(742.0f, 92.0f),   Vector2(238.0f, 238.0f), Vector2(-42.0f, 28.0f), UiColor(0x584D35).WithAlpha(0.90f)},
  {Vector2(1420.0f, 130.0f), Vector2(148.0f, 148.0f), Vector2(48.0f, 31.0f), UiColor(0x43315E).WithAlpha(0.92f)},
  {Vector2(1456.0f, 428.0f), Vector2(212.0f, 212.0f), Vector2(-30.0f, -24.0f), UiColor(0x6A3846).WithAlpha(0.92f)},
  {Vector2(108.0f, 442.0f),  Vector2(162.0f, 162.0f), Vector2(28.0f, -22.0f), UiColor(0x32524C).WithAlpha(0.90f)},
  {Vector2(914.0f, 382.0f),  Vector2(134.0f, 134.0f), Vector2(33.0f, 38.0f), UiColor(0x6B4C2F).WithAlpha(0.88f)},
};

Dali::String BuildTitleText()
{
  return Dali::String("THE FUTURE OF\nTEXT LAYOUT\nIS DYNAMIC");
}

Dali::String BuildBodyText()
{
  std::ostringstream builder;
  builder
    << "The rendering pipeline was designed for static pages, yet every modern interface asks text to respond to moving objects, live composition, and editorial pacing. "
    << "텍스트는 더 이상 사각형 안에 갇힌 데이터가 아니라 주변 요소와 함께 리듬을 만들어내는 시각 재료다. "
    << "When a system can prepare shaping once and then reflow around objects, it stops feeling like a rigid document engine and starts behaving like a layout instrument. "
    << "A browser still hides the real cost of measurement behind synchronous barriers. A paragraph looks effortless only because the engine measured, broke, aligned, and painted it before the reader noticed. "
    << "But when interactive objects move through the composition, the hidden pipeline becomes visible. The question is no longer whether text can wrap. The question is whether text can stay fluid without forcing the whole page to stall.\n\n"
    << "Design systems often treat text as the last step: render a block, then stack objects around it. Editorial interfaces reverse the rule. "
    << "Objects enter first, and the text must adapt in real time without losing its tone, measure, or cadence. "
    << "That means line height, baseline placement, and the geometry passed to the renderer all matter at once. "
    << "When the gap between lines is too tight, every moving obstacle feels heavier because more lines must be processed and more fragments must squeeze into the same height. "
    << "Once line metrics become more natural, the paragraphs breathe. The visual change looks subtle, but the engine also gains room: fewer visible lines, fewer interval scans, and less layout churn under pressure.\n\n"
    << "CSS taught the industry to think in rectangles, but the most interesting interfaces are rarely rectangular. Cards expand, images drift, subtitles appear, and notifications cut across the reading flow. "
    << "Developers end up guessing text height, over-measuring the DOM, or paying for expensive relayout loops because the engine exposes too little of its own structure. "
    << "A text visualizer should move in the opposite direction. It should prepare text once, expose stable layout inputs, and let the application reposition exclusion bounds as often as composition demands. "
    << "The renderer then needs to update cleanly when those placements change.\n\n"
    << "This sample keeps one large text surface and uses invisible fixed bounds to split the body into three reading lanes. "
    << "The moving orb should feel like an inserted image or a conversation bubble that the paragraph naturally avoids. "
    << "It is not enough for the box height to change. The glyphs must truly reflow and find a new path through the composition as the scene shifts. "
    << "If the text remains readable while the objects drift, the pipeline stops being a demo and starts becoming a usable editorial primitive.";

  const std::string text = builder.str();
  return Dali::String(text.c_str());
}

Property::Map CreateOrbShadowMap(const UiColor& color)
{
  Property::Map transform;
  transform.Add(Ui::Visual::Transform::Property::OFFSET, Vector2(14.0f, 16.0f));
  transform.Add(Ui::Visual::Transform::Property::OFFSET_POLICY,
                Vector2(static_cast<float>(Ui::Visual::Transform::Policy::ABSOLUTE),
                        static_cast<float>(Ui::Visual::Transform::Policy::ABSOLUTE)));
  transform.Add(Ui::Visual::Transform::Property::SIZE, Vector2(1.08f, 1.08f));
  transform.Add(Ui::Visual::Transform::Property::SIZE_POLICY,
                Vector2(static_cast<float>(Ui::Visual::Transform::Policy::RELATIVE),
                        static_cast<float>(Ui::Visual::Transform::Policy::RELATIVE)));

  Property::Map shadow;
  shadow.Add(Ui::Visual::Property::TYPE, Ui::Visual::COLOR);
  shadow.Add(Ui::Visual::Property::MIX_COLOR, color.Resolve());
  shadow.Add(Ui::Visual::Property::TRANSFORM, transform);
  return shadow;
}

Rect<float> InflateRect(const Vector2& position, const Vector2& size, float padding)
{
  return Rect<float>(position.x - padding,
                     position.y - padding,
                     size.x + (padding * 2.0f),
                     size.y + (padding * 2.0f));
}

struct MovingOrb
{
  View     view;
  Vector2  position{Vector2::ZERO};
  Vector2  size{Vector2::ZERO};
  Vector2  velocity{Vector2::ZERO};
  UiColor  color;
};

struct QuoteBlock
{
  View    accent;
  Label   label;
  Vector2 position{Vector2::ZERO};
  Vector2 size{Vector2::ZERO};
};
} // namespace

class TextVisualizerPerformanceController : public ConnectionTracker
{
public:
  explicit TextVisualizerPerformanceController(Application& application)
  : mApplication(application)
  {
    mApplication.InitSignal().Connect(this, &TextVisualizerPerformanceController::OnInit);
  }

private:
  void OnInit(Application& application)
  {
    Window window = application.GetWindow();
    window.SetBackgroundColor(WINDOW_BG_COLOR);
    window.SetSize(Dali::Window::WindowSize(static_cast<uint32_t>(WINDOW_WIDTH), static_cast<uint32_t>(WINDOW_HEIGHT)));
    window.KeyEventSignal().Connect(this, &TextVisualizerPerformanceController::OnKeyEvent);

    CreateScene(window);
    CreateTextSurface();
    CreateQuoteBlock();
    CreateMovingOrbs();
    ApplyTextStyle();
    ApplyOrbVisuals();
    ApplyExclusionRegions();
    UpdateStatusText(true);

    mStartTime    = std::chrono::steady_clock::now();
    mLastTickTime = mStartTime;

    mTimer = Timer::New(TIMER_INTERVAL_MS);
    mTimer.TickSignal().Connect(this, &TextVisualizerPerformanceController::OnTimerTick);
    mTimer.Start();
  }

  void CreateScene(Window& window)
  {
    mRoot = View::New();
    mRoot.SetRequestedWidth(MATCH_PARENT);
    mRoot.SetRequestedHeight(MATCH_PARENT);
    mRoot.SetBackgroundColor(WINDOW_BG_COLOR);

    mTitleLabel = Label::New(BuildTitleText())
                    .SetLayoutMode(LayoutMode::STANDALONE)
                    .SetPositionX(SIDE_MARGIN - 6.0f)
                    .SetPositionY(TITLE_TOP)
                    .SetRequestedWidth(WINDOW_WIDTH - (SIDE_MARGIN * 2.0f))
                    .SetRequestedHeight(TITLE_HEIGHT)
                    .SetFontSize(TITLE_FONT_SIZE)
                    .SetTextColor(TITLE_COLOR)
                    .SetMultiLine(true)
                    .SetLineHeight(0.88f)
                    .SetHorizontalTextAlignment(Text::Alignment::START)
                    .SetAsyncRendering(true);

    mContentArea = View::New();
    mContentArea.SetLayoutMode(LayoutMode::STANDALONE);
    mContentArea.SetPositionX(SIDE_MARGIN);
    mContentArea.SetPositionY(CONTENT_TOP);
    mContentArea.SetRequestedWidth(CONTENT_WIDTH);
    mContentArea.SetRequestedHeight(CONTENT_HEIGHT);
    mContentArea.SetBackgroundColor(UiColor(Color::TRANSPARENT));
    mContentArea.SetProperty(Actor::Property::CLIPPING_MODE, ClippingMode::CLIP_CHILDREN);

    mStatusLabel = Label::New("")
                     .SetLayoutMode(LayoutMode::STANDALONE)
                     .SetPositionX(SIDE_MARGIN)
                     .SetPositionY(WINDOW_HEIGHT - STATUS_HEIGHT - STATUS_BOTTOM)
                     .SetRequestedWidth(CONTENT_WIDTH)
                     .SetRequestedHeight(STATUS_HEIGHT)
                     .SetFontSize(15.0f)
                     .SetTextColor(STATUS_TEXT_COLOR)
                     .SetHorizontalTextAlignment(Text::Alignment::START)
                     .SetVerticalTextAlignment(Text::Alignment::CENTER)
                     .SetAsyncRendering(true);

    mRoot.Add(mTitleLabel);
    mRoot.Add(mContentArea);
    mRoot.Add(mStatusLabel);
    window.Add(mRoot);
  }

  void CreateTextSurface()
  {
    mTextVisualizer = TextVisualizer::New();
    mTextVisualizer.SetLayoutMode(LayoutMode::STANDALONE);
    mTextVisualizer.SetPositionX(0.0f);
    mTextVisualizer.SetPositionY(0.0f);
    mTextVisualizer.SetRequestedWidth(MATCH_PARENT);
    mTextVisualizer.SetRequestedHeight(MATCH_PARENT);
    mTextVisualizer.SetBackgroundColor(UiColor(Color::TRANSPARENT));
    mTextVisualizer.SetText(BuildBodyText());
    mTextVisualizer.SetFontSize(mBodyFontSize);
    mTextVisualizer.SetLineHeight(BODY_LINE_HEIGHT);
    mTextVisualizer.SetTextColor(TEXT_COLORS[mCurrentTextColorIndex]);

    mFixedColumnBounds.Clear();
    mFixedColumnBounds.PushBack(Rect<float>(COLUMN_WIDTH, 0.0f, COLUMN_SPLIT_WIDTH, CONTENT_HEIGHT));
    mFixedColumnBounds.PushBack(Rect<float>((COLUMN_WIDTH * 2.0f) + COLUMN_GAP, 0.0f, COLUMN_SPLIT_WIDTH, CONTENT_HEIGHT));

    mContentArea.Add(mTextVisualizer);
  }

  void CreateQuoteBlock()
  {
    mQuoteBlocks.clear();
    mQuoteBlocks.reserve(2u);

    auto addQuote = [&](const Dali::String& text, const Vector2& position, const Vector2& size, float accentHeight)
    {
      QuoteBlock block;
      block.position = position;
      block.size     = size;

      block.accent = View::New();
      block.accent.SetLayoutMode(LayoutMode::STANDALONE);
      block.accent.SetPositionX(position.x + 12.0f);
      block.accent.SetPositionY(position.y + 12.0f);
      block.accent.SetRequestedWidth(3.0f);
      block.accent.SetRequestedHeight(accentHeight);
      block.accent.SetBackgroundColor(ACCENT_COLOR.WithAlpha(0.65f));

      block.label = Label::New(text)
                      .SetLayoutMode(LayoutMode::STANDALONE)
                      .SetPositionX(position.x)
                      .SetPositionY(position.y)
                      .SetRequestedWidth(size.x)
                      .SetRequestedHeight(size.y)
                      .SetPadding(Extents(34, 12, 8, 8))
                      .SetFontSize(QUOTE_FONT_SIZE)
                      .SetLineHeight(QUOTE_LINE_HEIGHT)
                      .SetTextColor(QUOTE_TEXT_COLOR)
                      .SetFontSlant(Text::FontSlant::ITALIC)
                      .SetMultiLine(true)
                      .SetHorizontalTextAlignment(Text::Alignment::START)
                      .SetAsyncRendering(true);

      mContentArea.Add(block.accent);
      mContentArea.Add(block.label);
      mQuoteBlocks.push_back(block);
    };

    addQuote("“Text becomes a\nfirst-class participant\nin the visual composition\n— not a static block.”",
             Vector2(34.0f, 314.0f),
             Vector2(250.0f, 138.0f),
             116.0f);

    addQuote("“The layout engine stops\nfeeling like a text box\nand starts behaving like\nan editorial surface.”",
             Vector2(1456.0f, 318.0f),
             Vector2(238.0f, 134.0f),
             108.0f);
  }

  void CreateMovingOrbs()
  {
    mOrbs.clear();
    mOrbs.reserve(MAX_ORB_COUNT);

    for(uint32_t index = 0u; index < MAX_ORB_COUNT; ++index)
    {
      MovingOrb orb;
      orb.position    = ORB_PRESETS[index].position;
      orb.size        = ORB_PRESETS[index].size;
      orb.velocity    = ORB_PRESETS[index].velocity;
      orb.color       = ORB_PRESETS[index].color;

      orb.view = View::New();
      orb.view.SetLayoutMode(LayoutMode::STANDALONE);
      orb.view.SetBackgroundColor(orb.color);
      orb.view.SetCornerRadiusPolicyRelative();
      orb.view.SetCornerRadius(0.5f);
      orb.view.SetProperty(View::Property::SHADOW, CreateOrbShadowMap(orb.color.WithAlpha(0.28f)));

      mContentArea.Add(orb.view);
      mOrbs.push_back(orb);
    }
  }

  void ApplyTextStyle()
  {
    mTextVisualizer.SetFontSize(mBodyFontSize);
    mTextVisualizer.SetLineHeight(BODY_LINE_HEIGHT);
    mTextVisualizer.SetTextColor(TEXT_COLORS[mCurrentTextColorIndex]);
  }

  void ApplyOrbVisuals()
  {
    for(uint32_t index = 0u; index < mOrbs.size(); ++index)
    {
      const bool active = index < mActiveOrbCount;
      mOrbs[index].view.SetProperty(Actor::Property::VISIBLE, active);
      if(!active)
      {
        continue;
      }

      mOrbs[index].view.SetPositionX(mOrbs[index].position.x);
      mOrbs[index].view.SetPositionY(mOrbs[index].position.y);
      mOrbs[index].view.SetRequestedWidth(mOrbs[index].size.x);
      mOrbs[index].view.SetRequestedHeight(mOrbs[index].size.y);
    }
  }

  Rect<float> GetQuoteExclusionRect(const QuoteBlock& block) const
  {
    const Vector3 position = block.label.GetCurrentProperty<Vector3>(Actor::Property::POSITION);
    const Vector3 size     = block.label.GetCurrentProperty<Vector3>(Actor::Property::SIZE);
    return InflateRect(Vector2(position.x, position.y), Vector2(size.x, size.y), 24.0f);
  }

  void AppendOrbExclusionRegions(Dali::Vector<Rect<float>>& regions, const MovingOrb& orb) const
  {
    const float insetX = orb.size.x * 0.18f;
    const float topBandHeight = orb.size.y * 0.22f;
    const float middleBandHeight = orb.size.y * 0.28f;
    const float bottomBandHeight = orb.size.y * 0.22f;
    const float middleStartY = orb.position.y + (orb.size.y * 0.22f);

    regions.PushBack(Rect<float>(orb.position.x + insetX,
                                 orb.position.y,
                                 std::max(0.0f, orb.size.x - (insetX * 2.0f)),
                                 topBandHeight));
    regions.PushBack(Rect<float>(orb.position.x,
                                 middleStartY,
                                 orb.size.x,
                                 middleBandHeight));
    regions.PushBack(Rect<float>(orb.position.x,
                                 middleStartY + middleBandHeight,
                                 orb.size.x,
                                 middleBandHeight));
    regions.PushBack(Rect<float>(orb.position.x + insetX,
                                 orb.position.y + orb.size.y - bottomBandHeight,
                                 std::max(0.0f, orb.size.x - (insetX * 2.0f)),
                                 bottomBandHeight));
  }

  void ApplyExclusionRegions()
  {
    if(!mExclusionEnabled)
    {
      mTextVisualizer.ClearExclusionRegions();
      return;
    }

    Dali::Vector<Rect<float>> regions;
    for(uint32_t index = 0u; index < mFixedColumnBounds.Count(); ++index)
    {
      regions.PushBack(mFixedColumnBounds[index]);
    }

    for(uint32_t orbIndex = 0u; orbIndex < std::min<uint32_t>(mActiveOrbCount, static_cast<uint32_t>(mOrbs.size())); ++orbIndex)
    {
      const MovingOrb& orb = mOrbs[orbIndex];
      AppendOrbExclusionRegions(regions, orb);
    }

    for(uint32_t quoteIndex = 0u; quoteIndex < mQuoteBlocks.size(); ++quoteIndex)
    {
      regions.PushBack(GetQuoteExclusionRect(mQuoteBlocks[quoteIndex]));
    }
    mTextVisualizer.SetExclusionRegions(regions);

    if(mExclusionEnabled)
    {
      ++mRelayoutUpdateCount;
    }
  }

  void UpdateOrbPositions(float deltaSeconds)
  {
    for(uint32_t index = 0u; index < std::min<uint32_t>(mActiveOrbCount, static_cast<uint32_t>(mOrbs.size())); ++index)
    {
      MovingOrb& orb = mOrbs[index];
      orb.position += orb.velocity * deltaSeconds;

      const float maxX = std::max(0.0f, CONTENT_WIDTH - orb.size.x);
      const float maxY = std::max(0.0f, CONTENT_HEIGHT - orb.size.y);

      if(orb.position.x < 0.0f)
      {
        orb.position.x = 0.0f;
        orb.velocity.x = std::abs(orb.velocity.x);
      }
      else if(orb.position.x > maxX)
      {
        orb.position.x = maxX;
        orb.velocity.x = -std::abs(orb.velocity.x);
      }

      if(orb.position.y < 0.0f)
      {
        orb.position.y = 0.0f;
        orb.velocity.y = std::abs(orb.velocity.y);
      }
      else if(orb.position.y > maxY)
      {
        orb.position.y = maxY;
        orb.velocity.y = -std::abs(orb.velocity.y);
      }
    }
  }

  bool OnTimerTick()
  {
    const auto now        = std::chrono::steady_clock::now();
    const float deltaSecs = std::chrono::duration<float>(now - mLastTickTime).count();
    mLastTickTime         = now;
    ++mFrameCount;

    if(mAnimationEnabled)
    {
      UpdateOrbPositions(std::max(deltaSecs, 0.001f));
      ApplyOrbVisuals();
      ApplyExclusionRegions();
    }

    if((mFrameCount % STATUS_INTERVAL) == 0u)
    {
      UpdateStatusText(false);
    }

    return true;
  }

  void UpdateStatusText(bool force)
  {
    if(!force && (mFrameCount % STATUS_INTERVAL) != 0u)
    {
      return;
    }

    const auto  now        = std::chrono::steady_clock::now();
    const float elapsedSec = std::max(std::chrono::duration<float>(now - mStartTime).count(), 0.001f);
    const float fps        = static_cast<float>(mFrameCount) / elapsedSec;

    std::ostringstream builder;
    builder.setf(std::ios::fixed);
    builder.precision(1);
    builder << "FPS " << fps
            << "   RELOWS " << mRelayoutUpdateCount
            << "   ORBS " << mActiveOrbCount
            << "   FONT " << mBodyFontSize
            << "   LINE " << BODY_LINE_HEIGHT
            << "   COLOR " << TEXT_COLOR_NAMES[mCurrentTextColorIndex]
            << "   ANIM " << (mAnimationEnabled ? "ON" : "OFF")
            << "   EXCLUSION " << (mExclusionEnabled ? "ON" : "OFF")
            << "   Space pause · 1/2 orbs · 3/4 font · 5 exclusion · 6 color";

    mStatusLabel.SetText(builder.str().c_str());
  }

  void ToggleAnimation()
  {
    mAnimationEnabled = !mAnimationEnabled;
    UpdateStatusText(true);
  }

  void SetOrbCount(uint32_t count)
  {
    mActiveOrbCount = std::min(std::max(count, MIN_ORB_COUNT), MAX_ORB_COUNT);
    ApplyOrbVisuals();
    ApplyExclusionRegions();
    UpdateStatusText(true);
  }

  void AdjustFontSize(float delta)
  {
    mBodyFontSize = std::min(std::max(mBodyFontSize + delta, MIN_FONT_SIZE), MAX_FONT_SIZE);
    ApplyTextStyle();
    ApplyExclusionRegions();
    UpdateStatusText(true);
  }

  void ToggleExclusion()
  {
    mExclusionEnabled = !mExclusionEnabled;
    ApplyExclusionRegions();
    UpdateStatusText(true);
  }

  void CycleTextColor()
  {
    mCurrentTextColorIndex = (mCurrentTextColorIndex + 1u) % static_cast<uint32_t>(sizeof(TEXT_COLORS) / sizeof(TEXT_COLORS[0]));
    ApplyTextStyle();
    UpdateStatusText(true);
  }

  void OnKeyEvent(const KeyEvent& event)
  {
    if(event.GetState() != KeyEvent::UP)
    {
      return;
    }

    if(IsKey(event, Dali::DALI_KEY_ESCAPE) || IsKey(event, Dali::DALI_KEY_BACK))
    {
      mApplication.Quit();
      return;
    }

    const std::string keyName = event.GetKeyName().CStr();
    if(keyName == "space" || keyName == "Space")
    {
      ToggleAnimation();
    }
    else if(keyName == "1")
    {
      SetOrbCount(mActiveOrbCount - 1u);
    }
    else if(keyName == "2")
    {
      SetOrbCount(mActiveOrbCount + 1u);
    }
    else if(keyName == "3")
    {
      AdjustFontSize(-FONT_STEP);
    }
    else if(keyName == "4")
    {
      AdjustFontSize(FONT_STEP);
    }
    else if(keyName == "5")
    {
      ToggleExclusion();
    }
    else if(keyName == "6")
    {
      CycleTextColor();
    }
  }

private:
  Application&                          mApplication;
  View                                  mRoot;
  Label                                 mTitleLabel;
  Label                                 mStatusLabel;
  View                                  mContentArea;
  TextVisualizer                        mTextVisualizer;
  Dali::Vector<Rect<float>>             mFixedColumnBounds;
  std::vector<QuoteBlock>               mQuoteBlocks;
  std::vector<MovingOrb>                mOrbs;
  Timer                                 mTimer;
  bool                                  mAnimationEnabled{true};
  bool                                  mExclusionEnabled{true};
  uint32_t                              mActiveOrbCount{DEFAULT_ORB_COUNT};
  uint32_t                              mCurrentTextColorIndex{0u};
  float                                 mBodyFontSize{BODY_FONT_SIZE};
  uint64_t                              mFrameCount{0u};
  uint64_t                              mRelayoutUpdateCount{0u};
  std::chrono::steady_clock::time_point mStartTime;
  std::chrono::steady_clock::time_point mLastTickTime;
};

int DALI_EXPORT_API main(int argc, char** argv)
{
  DaliUiFoundationPreInitialize(nullptr, nullptr, nullptr);

  Application application = Application::New(&argc, &argv);
  UiConfig::New().Apply();

  TextVisualizerPerformanceController controller(application);
  application.MainLoop();

  return 0;
}
