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
 */

#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali/devel-api/adaptor-framework/application.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace Dali;
using namespace Dali::Ui;

namespace
{
const char* const MEDIUM_TEXT =
  "아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 "
  "공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한";

const char* const LARGE_TEXT =
  "아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 "
  "공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 "
  "조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 "
  "내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 "
  "오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무";

const char* const FHD_TEXT =
  "아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 "
  "공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 "
  "조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 "
  "내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 "
  "오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 "
  "새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. "
  "작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 "
  "시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 "
  "느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 "
  "나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 "
  "들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 "
  "사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 "
  "줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 "
  "이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 "
  "확인합니다. 동네 공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, "
  "익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 "
  "바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 "
  "따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 "
  "여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 "
  "기울입니다. 작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 "
  "천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 "
  "변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 "
  "음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 "
  "창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 "
  "걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 "
  "여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 "
  "즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 "
  "날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 "
  "펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 "
  "소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 "
  "함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. "
  "화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 "
  "하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 "
  "하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 공원을 따라 걸으며 나무 사이로 "
  "불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 조용한 휴식에 여유를 더해 줍니다. "
  "저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 내일을 기다리는 즐거움으로 이어집니다. "
  "아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한 차를 마시며 오늘의 소식과 날씨를 확인합니다. 동네 "
  "공원을 따라 걸으며 나무 사이로 불어오는 바람과 계절의 변화를 느껴 봅니다. 화면에서는 여행지의 풍경과 새로운 문화가 펼쳐지고, 익숙한 음악은 "
  "조용한 휴식에 여유를 더해 줍니다. 저녁에는 함께 준비한 음식을 나누고 서로의 하루에 귀를 기울입니다. 작은 일상을 소중하게 바라보는 마음이 "
  "내일을 기다리는 즐거움으로 이어집니다. 아침 햇살이 거실 창문을 통해 들어오면 하루의 이야기가 천천히 시작됩니다. 가족과 함께 따뜻한";
} // namespace

/**
 * Fixed-size workloads for comparing Reveal Blur builds on a target device.
 * Keys select the workload, quality and source format; no FPS instrumentation
 * or runtime optimization switch is required in the library.
 */
class TextRevealPerfExample : public ConnectionTracker
{
public:
  explicit TextRevealPerfExample(Application application)
  : mApplication(application)
  {
    mApplication.InitSignal().Connect(this, &TextRevealPerfExample::OnInit);
  }

  ~TextRevealPerfExample()
  {
    if(mAnimation)
    {
      mAnimation.Stop();
      mAnimation.Clear();
    }
  }

private:
  void OnInit(Application application)
  {
    mWindow = application.GetWindow();
    mWindow.SetBackgroundColor(Color::BLACK);
    mRoot = AbsoluteLayout::New();
    mRoot.SetRequestedWidth(MATCH_PARENT);
    mRoot.SetRequestedHeight(MATCH_PARENT);
    mWindow.Add(mRoot);
    mHelp = Label::New();
    mHelp.SetLayoutMode(LayoutMode::STANDALONE);
    mHelp.SetRequestedWidth(MATCH_PARENT);
    mHelp.SetRequestedHeight(64.0f);
    mHelp.SetFontSize(16.0f);
    mHelp.SetTextColor(UiColor(Color::WHITE));
    mHelp.SetBackgroundColor(UiColor(0x172033));
    mHelp.SetMultiLine(true);
    mWindow.Add(mHelp);
    mWindow.KeyEventSignal().Connect(this, &TextRevealPerfExample::OnKeyEvent);
    Recreate();
  }

  void OnKeyEvent(Window, KeyEvent event)
  {
    if(event.GetState() != KeyEvent::UP)
    {
      return;
    }
    const auto key = event.GetKeyName();
    if(IsKey(event, DALI_KEY_ESCAPE) || IsKey(event, DALI_KEY_BACK))
    {
      mApplication.Quit();
      return;
    }
    if(key == "1" || key == "2" || key == "3")
    {
      mWorkload = key == "1" ? 0u : (key == "2" ? 1u : 2u);
    }
    else if(key == "h" || key == "H")
    {
      mHigh = !mHigh;
    }
    else if(key == "g" || key == "G")
    {
      mColored = !mColored;
    }
    else if(key == "a" || key == "A")
    {
      mAsync = !mAsync;
    }
    else if(key == "space")
    {
      Replay();
      return;
    }
    else
    {
      return;
    }
    Recreate();
  }

  void Recreate()
  {
    if(mAnimation)
    {
      mAnimation.Stop();
      mAnimation.Clear();
      mAnimation.Reset();
    }
    for(auto label : mLabels)
    {
      label.Unparent();
    }
    mLabels.clear();

    const bool     fhd        = mWorkload == 2u;
    const float    width      = fhd ? 1920.0f : 500.0f;
    const float    height     = fhd ? 1080.0f : (mWorkload == 0u ? 350.0f : 500.0f);
    const uint32_t count      = fhd ? 1u : 8u;
    const char*    text       = fhd ? FHD_TEXT : (mWorkload == 0u ? MEDIUM_TEXT : LARGE_TEXT);
    const auto     windowSize = mWindow.GetPositionSize();
    // Do not scale the requested texture workloads to fit the window. On FHD,
    // a 2000px-wide grid is centered, clipping 40px at each outer screen edge.
    const float  left = (static_cast<float>(windowSize.width) - (fhd ? width : 4.0f * width)) * 0.5f;
    Text::Reveal reveal;
    reveal.SetUnit(Text::Reveal::Unit::PIXEL);
    reveal.SetSequence(Text::Reveal::Sequence::PER_LINE);
    reveal.SetFadeDurationRatio(0.0f);
    reveal.SetSequenceStaggerRatio(0.25f);
    reveal.SetBlurRadius(24.0f);
    reveal.SetBlurDurationRatio(0.5f);
    reveal.SetBlurQuality(mHigh ? Text::Reveal::BlurQuality::HIGH : Text::Reveal::BlurQuality::PERFORMANCE);
    for(uint32_t i = 0u; i < count; ++i)
    {
      Label label = Label::New(text);
      label.SetLayoutMode(LayoutMode::STANDALONE);
      label.SetRequestedWidth(width);
      label.SetRequestedHeight(height);
      label.SetRequestedX(left + static_cast<float>(i % 4u) * width);
      label.SetRequestedY(static_cast<float>(i / 4u) * height);
      label.SetFontSize(24.0f);
      label.SetMultiLine(true);
      label.SetTextOverflowMode(Text::OverflowMode::CLIP);
      label.SetTextColor(UiColor(Color::WHITE));
      label.SetAsyncRendering(mAsync);
      if(mColored)
      {
        auto builder = Text::StyledTextBuilder::New(text);
        builder.SetSpan(Text::ForegroundColorSpan::New(UiColor(0xFF8855)), 0u,
                        std::min(15u, builder.GetUtf32Length()));
        label.SetStyledText(builder.Build());
        Gradient::Linear gradient(Vector2(-0.5f, 0.0f), Vector2(0.5f, 0.0f));
        gradient.SetStopNodes({Gradient::StopNode(0.0f, UiColor(0xFFAA66)),
                               Gradient::StopNode(1.0f, UiColor(0x55CCFF))});
        label.SetTextGradient(gradient);
      }
      label.SetTextReveal(reveal);
      label.SetTextRevealProgress(0.0f);
      mRoot.Add(label);
      mLabels.push_back(label);
    }
    const std::string status = std::string(fhd ? "1920x1080 x1" : (mWorkload == 0u ? "500x350 x8" : "500x500 x8")) +
                               " | " + (mHigh ? "HIGH" : "PERFORMANCE") +
                               " | " + (mColored ? "RGBA + gradient" : "A8 white") +
                               " | " + (mAsync ? "Async" : "Sync") +
                               "\n1/2/3: workload   H: quality   G: color   A: async   Space: replay   Esc: exit";
    mHelp.SetText(status.c_str());
    mHelp.SetRequestedY(std::max(0.0f, static_cast<float>(windowSize.height) - 64.0f));
    Replay();
  }

  void Replay()
  {
    if(mAnimation)
    {
      mAnimation.Stop();
      mAnimation.Clear();
    }
    mAnimation = Animation::New(2.0f);
    mAnimation.SetLoopCount(Animation::INFINITE_LOOP);
    mAnimation.SetLoopingMode(Animation::AUTO_REVERSE);
    for(auto label : mLabels)
    {
      label.SetTextRevealProgress(0.0f);
      label.Animate(mAnimation).TextRevealProgress(1.0f, Duration(2.0f), AlphaFunction::LINEAR);
    }
    mAnimation.Play();
  }

  Application        mApplication;
  Window             mWindow;
  AbsoluteLayout     mRoot;
  Label              mHelp;
  std::vector<Label> mLabels;
  Animation          mAnimation;
  uint32_t           mWorkload{0u};
  bool               mHigh{false};
  bool               mColored{false};
  bool               mAsync{false};
};

int main(int argc, char** argv)
{
  Application           application = Application::New(&argc, &argv);
  TextRevealPerfExample sample(application);
  application.MainLoop();
  return 0;
}
