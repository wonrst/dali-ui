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
#include <dali/integration-api/debug.h>

#include <algorithm>
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

constexpr float ROOT_PADDING   = 24.0f;
constexpr float ROOT_SPACING   = 16.0f;
constexpr float HEADER_HEIGHT  = 92.0f;
constexpr float STATUS_HEIGHT  = 74.0f;
constexpr float CONTENT_WIDTH  = WINDOW_WIDTH - (ROOT_PADDING * 2.0f);
constexpr float CONTENT_HEIGHT = WINDOW_HEIGHT - (ROOT_PADDING * 2.0f) - HEADER_HEIGHT - STATUS_HEIGHT - (ROOT_SPACING * 2.0f);

constexpr float DEFAULT_FONT_SIZE = 28.0f;
constexpr float MIN_FONT_SIZE     = 18.0f;
constexpr float MAX_FONT_SIZE     = 40.0f;
constexpr float FONT_STEP         = 2.0f;
constexpr uint32_t MAX_BOUND_COUNT = 8u;
constexpr uint32_t MIN_BOUND_COUNT = 2u;
constexpr uint32_t DEFAULT_BOUND_COUNT = 6u;
constexpr uint32_t STATUS_UPDATE_INTERVAL = 12u;
constexpr uint32_t TIMER_INTERVAL_MS      = 16u;

const UiColor WINDOW_BG_COLOR(0x10151C);
const UiColor HEADER_BG_COLOR(0x1B2633);
const UiColor STATUS_BG_COLOR(0x16212C);
const UiColor CONTENT_BG_COLOR(0x0F1823);
const UiColor TEXT_AREA_BG_COLOR = UiColor(0xFFFFFF).WithAlpha(0.04f);
const UiColor HEADER_TEXT_COLOR(0xF4F7FB);
const UiColor STATUS_TEXT_COLOR(0xD9E4EF);

const UiColor TEXT_COLORS[] = {
  UiColor(0xF3F6FA),
  UiColor(0xFFDE59),
  UiColor(0x7CE7DD),
  UiColor(0x9FD1FF),
};

const char* TEXT_COLOR_NAMES[] = {
  "Paper",
  "Amber",
  "Mint",
  "Sky",
};

const UiColor BOUND_COLORS[] = {
  UiColor(0xFF6B6B).WithAlpha(0.70f),
  UiColor(0x4ECDC4).WithAlpha(0.70f),
  UiColor(0xFFD166).WithAlpha(0.70f),
  UiColor(0x5A9BFF).WithAlpha(0.70f),
  UiColor(0xC77DFF).WithAlpha(0.70f),
  UiColor(0x8AC926).WithAlpha(0.70f),
  UiColor(0xF72585).WithAlpha(0.70f),
  UiColor(0x48BFE3).WithAlpha(0.70f),
};

Dali::String BuildSampleText()
{
  std::ostringstream builder;
  builder
    << "The future of text layout is dynamic. Blocks of text should respond to objects, motion, and "
    << "changing composition without reshaping every frame. "
    << "텍스트는 더 이상 정적인 블록이 아니라 화면 위의 다른 요소와 함께 움직이는 유연한 재료여야 한다. "
    << "When bounds move, exclusion regions change, and the text should reflow around them while keeping shaping work cached. "
    << "Emoji also matters 😀🚀🌊 because mixed scripts reveal whether the render path stays stable under pressure.\n\n";

  builder
    << "This performance demo intentionally fills a large 1920x1080 canvas with long paragraphs, "
    << "moving placeholders, and repeated relayout work. "
    << "여기서는 여러 개의 object-like colored bounds가 계속 움직이고, TextVisualizer는 그 영역을 피해 다시 줄을 배치한다. "
    << "We are not profiling with exact GPU counters here; the sample is meant for visual and interactive observation. "
    << "Look for smooth movement, stable text color, and whether exclusion holes follow the moving objects consistently.\n\n";

  for(int paragraph = 0; paragraph < 8; ++paragraph)
  {
    builder
      << "Paragraph " << (paragraph + 1) << ". "
      << "Dynamic layout should preserve prepared shaping data, reuse glyph information, and only recompute placement when bounds change. "
      << "긴 문단이 화면을 가득 채울수록 exclusion region 처리와 relayout 안정성이 더 잘 드러난다. "
      << "If the moving bounds accelerate toward the edge, the text should still wrap around them naturally instead of freezing in the previous arrangement. "
      << "This is a synthetic editorial-style dummy text block made only for testing the TextVisualizer render pipeline and observing rough FPS, update counts, and stability over time. "
      << "Sample controls let us change font size, color, active bound count, and exclusion enable state while animation keeps running.\n\n";
  }

  const std::string text = builder.str();
  return Dali::String(text.c_str());
}

Label CreateInfoLabel(const char* text, float height, float fontSize)
{
  return Label::New(text)
    .SetRequestedWidth(MATCH_PARENT)
    .SetRequestedHeight(height)
    .SetFontSize(fontSize)
    .SetTextColor(HEADER_TEXT_COLOR)
    .SetMultiLine(true)
    .SetVerticalTextAlignment(Text::Alignment::CENTER)
    .SetPadding(Extents(16, 16, 12, 12))
    .SetBackgroundColor(HEADER_BG_COLOR);
}

struct MovingBound
{
  View    view;
  Vector2 position{Vector2::ZERO};
  Vector2 size{Vector2::ZERO};
  Vector2 velocity{Vector2::ZERO};
  UiColor color;
};
} // namespace

class TextVisualizerPerformanceController : public ConnectionTracker
{
public:
  explicit TextVisualizerPerformanceController(Application& application)
  : mApplication(application),
    mSampleText(BuildSampleText()),
    mTextLength(mSampleText.Size())
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
    CreateMovingBounds();
    ApplyTextStyle();
    ApplyBoundVisuals();
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
    mStatusLabel = CreateInfoLabel("", STATUS_HEIGHT, 16.0f);

    mTextVisualizer = TextVisualizer::New();
    mTextVisualizer.SetLayoutMode(LayoutMode::STANDALONE);
    mTextVisualizer.SetPositionX(0.0f);
    mTextVisualizer.SetPositionY(0.0f);
    mTextVisualizer.SetRequestedWidth(MATCH_PARENT);
    mTextVisualizer.SetRequestedHeight(MATCH_PARENT);
    mTextVisualizer.SetBackgroundColor(TEXT_AREA_BG_COLOR);
    mTextVisualizer.SetText(mSampleText);
    mTextVisualizer.SetFontSize(mFontSize);
    mTextVisualizer.SetTextColor(TEXT_COLORS[mCurrentTextColorIndex]);

    mContentArea = View::New();
    mContentArea.SetRequestedWidth(CONTENT_WIDTH);
    mContentArea.SetRequestedHeight(CONTENT_HEIGHT);
    mContentArea.SetBackgroundColor(CONTENT_BG_COLOR);
    mContentArea.SetProperty(Actor::Property::CLIPPING_MODE, ClippingMode::CLIP_CHILDREN);
    mContentArea.Add(mTextVisualizer);

    StackLayout root = StackLayout::New(StackOrientation::VERTICAL);
    root.SetRequestedWidth(MATCH_PARENT);
    root.SetRequestedHeight(MATCH_PARENT);
    root.SetPadding(Extents(ROOT_PADDING, ROOT_PADDING, ROOT_PADDING, ROOT_PADDING));
    root.SetSpacing(ROOT_SPACING);
    root.Add(CreateInfoLabel("TextVisualizer performance demo\nSpace: Pause/Resume   1/2: Bound count -/+   3/4: Font -/+   5: Toggle exclusion   6: Change text color   ESC/BACK: Quit",
                             HEADER_HEIGHT,
                             18.0f));
    root.Add(mStatusLabel);
    root.Add(mContentArea);

    window.Add(root);
  }

  void CreateMovingBounds()
  {
    mBounds.clear();
    mBounds.reserve(MAX_BOUND_COUNT);

    const Vector2 initialPositions[MAX_BOUND_COUNT] = {
      Vector2(120.0f, 80.0f),
      Vector2(520.0f, 180.0f),
      Vector2(920.0f, 120.0f),
      Vector2(1320.0f, 240.0f),
      Vector2(260.0f, 520.0f),
      Vector2(760.0f, 610.0f),
      Vector2(1180.0f, 500.0f),
      Vector2(1500.0f, 700.0f),
    };

    const Vector2 sizes[MAX_BOUND_COUNT] = {
      Vector2(180.0f, 120.0f),
      Vector2(240.0f, 160.0f),
      Vector2(150.0f, 220.0f),
      Vector2(210.0f, 140.0f),
      Vector2(260.0f, 110.0f),
      Vector2(180.0f, 200.0f),
      Vector2(220.0f, 150.0f),
      Vector2(160.0f, 170.0f),
    };

    const Vector2 velocities[MAX_BOUND_COUNT] = {
      Vector2(74.0f, 52.0f),
      Vector2(-58.0f, 76.0f),
      Vector2(66.0f, -62.0f),
      Vector2(-82.0f, 48.0f),
      Vector2(54.0f, -68.0f),
      Vector2(-70.0f, -46.0f),
      Vector2(62.0f, 58.0f),
      Vector2(-48.0f, 64.0f),
    };

    for(uint32_t index = 0u; index < MAX_BOUND_COUNT; ++index)
    {
      MovingBound bound;
      bound.view = View::New();
      bound.view.SetLayoutMode(LayoutMode::STANDALONE);
      bound.view.SetBackgroundColor(BOUND_COLORS[index]);
      bound.view.SetProperty(Actor::Property::CLIPPING_MODE, ClippingMode::CLIP_CHILDREN);
      bound.position = initialPositions[index];
      bound.size     = sizes[index];
      bound.velocity = velocities[index];
      bound.color    = BOUND_COLORS[index];

      mContentArea.Add(bound.view);
      mBounds.push_back(bound);
    }
  }

  void ApplyTextStyle()
  {
    mTextVisualizer.SetFontSize(mFontSize);
    mTextVisualizer.SetTextColor(TEXT_COLORS[mCurrentTextColorIndex]);
  }

  void ApplyBoundVisuals()
  {
    for(uint32_t index = 0u; index < mBounds.size(); ++index)
    {
      const bool active = index < mActiveBoundCount;
      mBounds[index].view.SetProperty(Actor::Property::VISIBLE, active);
      if(!active)
      {
        continue;
      }

      mBounds[index].view.SetPositionX(mBounds[index].position.x);
      mBounds[index].view.SetPositionY(mBounds[index].position.y);
      mBounds[index].view.SetRequestedWidth(mBounds[index].size.x);
      mBounds[index].view.SetRequestedHeight(mBounds[index].size.y);
    }
  }

  void ApplyExclusionRegions()
  {
    if(!mExclusionEnabled)
    {
      if(mExclusionApplied)
      {
        mTextVisualizer.ClearExclusionRegions();
        mExclusionApplied = false;
      }
      return;
    }

    Dali::Vector<Rect<float>> regions;
    for(uint32_t index = 0u; index < mActiveBoundCount; ++index)
    {
      regions.PushBack(Rect<float>(mBounds[index].position.x,
                                   mBounds[index].position.y,
                                   mBounds[index].size.x,
                                   mBounds[index].size.y));
    }

    mTextVisualizer.SetExclusionRegions(regions);
    mExclusionApplied = true;
    ++mRelayoutUpdateCount;
  }

  void UpdateBounds(float deltaSeconds)
  {
    for(uint32_t index = 0u; index < mActiveBoundCount; ++index)
    {
      MovingBound& bound = mBounds[index];
      bound.position += bound.velocity * deltaSeconds;

      if(bound.position.x < 0.0f)
      {
        bound.position.x = 0.0f;
        bound.velocity.x = std::abs(bound.velocity.x);
      }
      else if(bound.position.x + bound.size.x > CONTENT_WIDTH)
      {
        bound.position.x = CONTENT_WIDTH - bound.size.x;
        bound.velocity.x = -std::abs(bound.velocity.x);
      }

      if(bound.position.y < 0.0f)
      {
        bound.position.y = 0.0f;
        bound.velocity.y = std::abs(bound.velocity.y);
      }
      else if(bound.position.y + bound.size.y > CONTENT_HEIGHT)
      {
        bound.position.y = CONTENT_HEIGHT - bound.size.y;
        bound.velocity.y = -std::abs(bound.velocity.y);
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
      UpdateBounds(std::max(deltaSecs, 0.001f));
      ApplyBoundVisuals();
      ApplyExclusionRegions();
    }

    if((mFrameCount % STATUS_UPDATE_INTERVAL) == 0u)
    {
      UpdateStatusText(false);
    }

    return true;
  }

  void UpdateStatusText(bool force)
  {
    if(!force && (mFrameCount % STATUS_UPDATE_INTERVAL) != 0u)
    {
      return;
    }

    const auto  now        = std::chrono::steady_clock::now();
    const float elapsedSec = std::max(std::chrono::duration<float>(now - mStartTime).count(), 0.001f);
    const float fps        = static_cast<float>(mFrameCount) / elapsedSec;

    std::ostringstream builder;
    builder.setf(std::ios::fixed);
    builder.precision(1);
    builder << "FPS: " << fps
            << " | Frames: " << mFrameCount
            << " | Bounds: " << mActiveBoundCount
            << " | Updates: " << mRelayoutUpdateCount
            << " | Text length: " << mTextLength
            << " | Animation: " << (mAnimationEnabled ? "ON" : "OFF")
            << " | Exclusion: " << (mExclusionEnabled ? "ON" : "OFF")
            << " | Font: " << mFontSize
            << " | Color: " << TEXT_COLOR_NAMES[mCurrentTextColorIndex]
            << "\nMoving bounds use ContentArea local coordinates and are passed directly to SetExclusionRegions(). "
            << "This is a visual performance demo, not a precise profiler.";

    mStatusLabel.SetText(builder.str().c_str());
  }

  void ToggleAnimation()
  {
    mAnimationEnabled = !mAnimationEnabled;
    UpdateStatusText(true);
  }

  void SetBoundCount(uint32_t count)
  {
    mActiveBoundCount = std::min(std::max(count, MIN_BOUND_COUNT), MAX_BOUND_COUNT);
    ApplyBoundVisuals();
    ApplyExclusionRegions();
    UpdateStatusText(true);
  }

  void AdjustFontSize(float delta)
  {
    mFontSize = std::min(std::max(mFontSize + delta, MIN_FONT_SIZE), MAX_FONT_SIZE);
    ApplyTextStyle();
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

    if(event.GetKeyName() == "space" || event.GetKeyName() == "Space")
    {
      ToggleAnimation();
    }
    else if(event.GetKeyName() == "1")
    {
      SetBoundCount(mActiveBoundCount - 1u);
    }
    else if(event.GetKeyName() == "2")
    {
      SetBoundCount(mActiveBoundCount + 1u);
    }
    else if(event.GetKeyName() == "3")
    {
      AdjustFontSize(-FONT_STEP);
    }
    else if(event.GetKeyName() == "4")
    {
      AdjustFontSize(FONT_STEP);
    }
    else if(event.GetKeyName() == "5")
    {
      ToggleExclusion();
    }
    else if(event.GetKeyName() == "6")
    {
      CycleTextColor();
    }
  }

private:
  Application&                            mApplication;
  Dali::String                            mSampleText;
  size_t                                  mTextLength{0u};
  Label                                   mStatusLabel;
  View                                    mContentArea;
  TextVisualizer                          mTextVisualizer;
  Timer                                   mTimer;
  std::vector<MovingBound>                mBounds;
  bool                                    mAnimationEnabled{true};
  bool                                    mExclusionEnabled{true};
  bool                                    mExclusionApplied{false};
  uint32_t                                mActiveBoundCount{DEFAULT_BOUND_COUNT};
  uint32_t                                mCurrentTextColorIndex{0u};
  float                                   mFontSize{DEFAULT_FONT_SIZE};
  uint64_t                                mFrameCount{0u};
  uint64_t                                mRelayoutUpdateCount{0u};
  std::chrono::steady_clock::time_point   mStartTime;
  std::chrono::steady_clock::time_point   mLastTickTime;
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
