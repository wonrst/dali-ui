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
#include <dali/public-api/adaptor-framework/timer.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace Dali;
using namespace Dali::Ui;

namespace
{
constexpr float       GAP        = 8.f;
constexpr float       ALPHAS[]   = {0.f, .5f, 1.f};
constexpr uint32_t    COLORS[]   = {0x243B53, 0xB83240, 0x147D64, 0x3056B3};
constexpr const char* CASES[]    = {"Basic", "Empty", "Multiline", "Long text", "RTL / BiDi", "Multiple colors",
                                    "Emoji", "Text background", "Markup background", "Decorations", "MaximumLines CLIP",
                                    "MaximumLines ELLIPSIS", "TextFit Range", "TextFit Candidates", "Hyphenation"};
constexpr unsigned    CASE_COUNT = sizeof(CASES) / sizeof(CASES[0]);
enum Action
{
  MODE,
  CUTOUT,
  STYLE,
  STYLE_PRESET,
  TEXT_ALPHA,
  TEXT_RGB,
  BG_COLOR,
  BG_ALPHA,
  BG_CLEAR,
  BG_RECREATE,
  NEXT,
  PREVIOUS,
  HORIZONTAL,
  VERTICAL,
  PADDING,
  SIZE,
  FONT,
  MULTILINE,
  MAX_LINES,
  OVERFLOW,
  FIT,
  WRAP,
  UNDERLAY,
  UI_SCALE,
  RENDER_SCALE,
  LINE_HEIGHT,
  PROPERTY_ROUTE,
  RESET,
  ACTION_COUNT
};
constexpr const char* ACTION_NAMES[] = {"S: Sync / Async", "C: Cutout", "Style (1-4)", "Style settings", "A: Text alpha", "Text RGB",
                                        "B: Background color", "Background alpha", "Clear background", "Recreate background",
                                        "T: Next content", "Previous content", "Horizontal alignment", "Vertical alignment", "Padding", "Label size",
                                        "Font size", "Multiline", "Maximum lines", "CLIP / ELLIPSIS", "TextFit", "Word wrapping",
                                        "Underlay", "UI scale", "Render scale", "Line height", "TextColor API / Property", "R: Reset"};

Label MakeLabel(const char* text, float fontSize = 15.f)
{
  auto label = Label::New(text);
  label.SetFontSize(fontSize);
  label.SetTextColor(UiColor(0xF4F7FB));
  label.SetVerticalTextAlignment(Text::Alignment::CENTER);
  return label;
}

void Place(View view, float x, float y, float width, float height)
{
  view.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(x, y, width, height)));
}
} // namespace

class TextCutoutController : public ConnectionTracker
{
public:
  explicit TextCutoutController(Application& application)
  : mApplication(application)
  {
    application.InitSignal().Connect(this, &TextCutoutController::OnInit);
  }

private:
  void OnInit(Application application)
  {
    auto window = application.GetWindow();
    window.SetBackgroundColor(UiColor(0x101B2A));
    mRoot = AbsoluteLayout::New();
    mRoot.SetUiScalePolicy(UiScalePolicy::DISABLED);
    mRoot.SetRequestedWidth(MATCH_PARENT);
    mRoot.SetRequestedHeight(MATCH_PARENT);
    window.Add(mRoot);
    mTitle  = MakeLabel("Text Cutout", 24.f);
    mStatus = MakeLabel("", 14.f);
    mStatus.SetMultiLine(true);
    mStatus.SetTextFit(Text::Fit::Range(9.f, 14.f, 1.f));
    mGuide = MakeLabel("Click a control or use arrows + Enter. 1-4 style | S mode | C cutout | A alpha | B background | T content | R reset\nGradientSpan, Marquee, ImageSpan and Reveal are outside this sample.", 12.f);
    mGuide.SetMultiLine(true);
    mPreview = AbsoluteLayout::New();
    mPreview.SetProperty(Actor::Property::CLIPPING_MODE, ClippingMode::CLIP_CHILDREN);
    for(unsigned i = 0; i < 96; ++i)
    {
      auto tile = View::New();
      mTiles.push_back(tile);
      mPreview.Add(tile);
    }
    mLabel = Label::New();
    mLabel.SetUiScalePolicy(UiScalePolicy::ENABLED);
    mPreview.Add(mLabel);
    mControls = AbsoluteLayout::New();
    mScroll   = ScrollView::New();
    mScroll.SetScrollDirection(ScrollDirection::Vertical);
    mScroll.SetContent(mControls);
    for(unsigned i = 0; i < ACTION_COUNT; ++i)
    {
      auto button = MakeLabel(ACTION_NAMES[i], 14.f);
      button.SetPadding(Insets(8, 8, 0, 0));
      button.AsInteractive().ClickedSignal().Connect(this, [this, i](View, InputEvent) -> bool
      {
        mSelection = i;
        Activate(static_cast<Action>(i));
        return true;
      });
      mButtons.push_back(button);
      mControls.Add(button);
    }
    mRoot.Add(mTitle);
    mRoot.Add(mPreview);
    mRoot.Add(mStatus);
    mRoot.Add(mScroll);
    mRoot.Add(mGuide);
    window.KeyEventSignal().Connect(this, &TextCutoutController::OnKeyEvent);
    window.ResizedSignal().Connect(this, &TextCutoutController::OnResize);
    const auto bounds = window.GetPositionSize();
    mWidth            = bounds.width;
    mHeight           = bounds.height;
    Reset();
    mTimer = Timer::New(350);
    mTimer.TickSignal().Connect(this, &TextCutoutController::OnTick);
    mTimer.Start();
    HighlightSelection();
  }

  void Reset()
  {
    mAsync         = false;
    mCutout        = true;
    mStyle         = 0;
    mStylePreset   = 0;
    mTextAlpha     = 0;
    mTextRgb       = 0;
    mBgColor       = 0;
    mBgAlpha       = 2;
    mHasBackground = true;
    mCase          = 0;
    mHorizontal    = 1;
    mVertical      = 1;
    mPadding       = 1;
    mSize          = 2;
    mFont          = 1;
    mMultiline     = false;
    mMaxLines      = 0;
    mEllipsis      = true;
    mFit           = 0;
    mWrap          = 0;
    mUnderlay      = 0;
    mUiScale       = 0;
    mRenderScale   = 0;
    mLineHeight    = 0;
    mPropertyRoute = false;
    UiScaleManager::Get().SetScale(1.f);
    mLabel.SetAsyncRendering(false);
    mLabel.SetTextCutoutEnabled(true);
    mLabel.SetFontSize(60.f);
    mLabel.SetRenderScale(1.f);
    mLabel.SetPadding(Insets(12, 12, 12, 12));
    mLabel.SetHorizontalTextAlignment(Text::Alignment::CENTER);
    mLabel.SetVerticalTextAlignment(Text::Alignment::CENTER);
    ApplyBackground();
    ApplyTextColor();
    ApplyStyle();
    ApplyContent();
    Layout();
  }

  void ApplyTextColor()
  {
    constexpr Vector3 RGB[] = {Vector3(.9f, .25f, .12f), Vector3(.1f, .85f, .45f), Vector3(.8f, .3f, 1.f)};
    const Vector4     color(RGB[mTextRgb], ALPHAS[mTextAlpha]);
    if(mPropertyRoute)
      mLabel.SetProperty(Text::LabelPropertyIndex::TEXT_COLOR, color);
    else
      mLabel.SetTextColor(UiColor(color));
  }

  void ApplyBackground()
  {
    if(mHasBackground)
      mLabel.SetBackgroundColor(UiColor(COLORS[mBgColor]).WithAlpha(ALPHAS[mBgAlpha]));
    else
      mLabel.ClearBackground();
  }

  void ApplyStyle()
  {
    Text::Shadow  shadow;
    Text::Outline outline;
    if(mStyle & 1)
    {
      shadow.SetColor(UiColor(mStylePreset ? 0xCC44FF : 0xF79D32));
      shadow.SetOffset(mStylePreset ? Vector2(-7.f, -4.f) : Vector2(5.f, 5.f));
      shadow.SetBlurRadius(mStylePreset ? 7.f : 3.f);
    }
    if(mStyle & 2)
    {
      outline.SetColor(UiColor(mStylePreset ? 0x40DAFF : 0x56E39F));
      outline.SetWidth(mStylePreset ? 4.f : 2.f);
    }
    mLabel.SetTextShadow((mStyle & 1) ? shadow : Text::Shadow::None());
    mLabel.SetTextOutline((mStyle & 2) ? outline : Text::Outline::None());
  }

  void ApplyFit()
  {
    if(mFit == 0)
      mLabel.SetTextFit(Text::Fit::None());
    else if(mFit == 1)
      mLabel.SetTextFit(Text::Fit::Range(16.f, 72.f, 4.f));
    else
    {
      Dali::Vector<Text::Fit::Candidate> candidates;
      for(float size : {18.f, 28.f, 40.f, 56.f, 72.f}) candidates.PushBack(Text::Fit::Candidate(size, size * 1.2f));
      mLabel.SetTextFit(candidates);
    }
  }

  void ApplyContent()
  {
    mLabel.ClearTextBackgroundColor();
    mLabel.SetTextUnderline(Text::Underline::None());
    mLabel.SetTextLineThrough(Text::LineThrough::None());
    mMultiline = mCase == 2 || mCase == 3 || mCase >= 10;
    mMaxLines  = mCase == 10 || mCase == 11 ? 2 : 0;
    mEllipsis  = mCase != 10 && mCase != 14;
    mFit       = mCase == 12 ? 1 : mCase == 13 ? 2
                                               : 0;
    mWrap      = mCase == 14 ? 1 : 0;
    mLabel.SetMultiLine(mMultiline);
    mLabel.SetMaximumLines(mMaxLines);
    mLabel.SetTextOverflowMode(mEllipsis ? Text::OverflowMode::ELLIPSIS : Text::OverflowMode::CLIP);
    mLabel.SetLineWrapMode(mWrap ? Text::LineWrapMode::HYPHENATION : Text::LineWrapMode::WORD);
    ApplyFit();
    const char* text = "CUTOUT";
    if(mCase == 1)
      text = "";
    else if(mCase == 2)
      text = "Text Cutout\nSecond line\nThird line";
    else if(mCase == 3 || mCase >= 10)
      text = "Characteristically extraordinary incomprehensibility. A long line wraps across the label and continues beyond the maximum lines.";
    else if(mCase == 4)
      text = "مرحبا بالعالم  שלום  DALi 123";
    else if(mCase == 6)
      text = "DALi 😀 🌍";
    mLabel.SetText(text);
    if(mCase == 5 || mCase == 7)
    {
      auto builder = Text::StyledTextBuilder::New("Color Text Cutout");
      if(mCase == 5)
      {
        builder.SetSpan(Text::ForegroundColorSpan::New(UiColor(0xFF574D)), 0u, 5u);
        builder.SetSpan(Text::ForegroundColorSpan::New(UiColor(0x48D1FF)), 6u, 10u);
      }
      else
      {
        mLabel.SetTextBackgroundColor(UiColor(0xF89428));
        builder.SetSpan(Text::BackgroundColorSpan::New(UiColor(0x9B55D4)), 6u, 10u);
      }
      mLabel.SetStyledText(builder.Build());
    }
    if(mCase == 8) mLabel.SetStyledText(Text::StyledText::FromMarkup("Text <background color='#B14DCE'>background</background> Cutout"));
    if(mCase == 9)
    {
      Text::Underline underline;
      underline.SetType(Text::Underline::Type::DOUBLE);
      underline.SetThickness(3.f);
      mLabel.SetTextUnderline(underline);
      Text::LineThrough strike;
      strike.SetThickness(2.f);
      mLabel.SetTextLineThrough(strike);
    }
  }

  void Activate(Action action)
  {
    switch(action)
    {
      case MODE:
        mAsync = !mAsync;
        mLabel.SetAsyncRendering(mAsync);
        break;
      case CUTOUT:
        mCutout = !mCutout;
        mLabel.SetTextCutoutEnabled(mCutout);
        break;
      case STYLE:
        mStyle = (mStyle + 1) % 4;
        ApplyStyle();
        break;
      case STYLE_PRESET:
        mStylePreset ^= 1;
        ApplyStyle();
        break;
      case TEXT_ALPHA:
        mTextAlpha = (mTextAlpha + 1) % 3;
        ApplyTextColor();
        break;
      case TEXT_RGB:
        mTextRgb = (mTextRgb + 1) % 3;
        ApplyTextColor();
        break;
      case BG_COLOR:
        mBgColor       = (mBgColor + 1) % 4;
        mHasBackground = true;
        ApplyBackground();
        break;
      case BG_ALPHA:
        mBgAlpha = (mBgAlpha + 1) % 3;
        ApplyBackground();
        break;
      case BG_CLEAR:
        mHasBackground = false;
        ApplyBackground();
        break;
      case BG_RECREATE:
        mHasBackground = true;
        mBgColor       = 3;
        ApplyBackground();
        break;
      case NEXT:
        mCase = (mCase + 1) % CASE_COUNT;
        ApplyContent();
        break;
      case PREVIOUS:
        mCase = (mCase + CASE_COUNT - 1) % CASE_COUNT;
        ApplyContent();
        break;
      case HORIZONTAL:
        mHorizontal = (mHorizontal + 1) % 3;
        mLabel.SetHorizontalTextAlignment(static_cast<Text::Alignment>(mHorizontal));
        break;
      case VERTICAL:
        mVertical = (mVertical + 1) % 3;
        mLabel.SetVerticalTextAlignment(static_cast<Text::Alignment>(mVertical));
        break;
      case PADDING:
        mPadding = (mPadding + 1) % 3;
        mLabel.SetPadding(Insets(mPadding * 12.f, mPadding * 12.f, mPadding * 8.f, mPadding * 8.f));
        break;
      case SIZE:
        mSize = (mSize + 1) % 3;
        Layout();
        break;
      case FONT:
        mFont = (mFont + 1) % 3;
        mLabel.SetFontSize(mFont == 0 ? 32.f : mFont == 1 ? 60.f
                                                          : 84.f);
        break;
      case MULTILINE:
        mMultiline = !mMultiline;
        mLabel.SetMultiLine(mMultiline);
        break;
      case MAX_LINES:
        mMaxLines = (mMaxLines + 1) % 4;
        mLabel.SetMaximumLines(mMaxLines);
        break;
      case OVERFLOW:
        mEllipsis = !mEllipsis;
        mLabel.SetTextOverflowMode(mEllipsis ? Text::OverflowMode::ELLIPSIS : Text::OverflowMode::CLIP);
        break;
      case FIT:
        mFit = (mFit + 1) % 3;
        ApplyFit();
        break;
      case WRAP:
        mWrap = (mWrap + 1) % 3;
        mLabel.SetLineWrapMode(mWrap == 0 ? Text::LineWrapMode::WORD : mWrap == 1 ? Text::LineWrapMode::HYPHENATION
                                                                                  : Text::LineWrapMode::MIXED);
        break;
      case UNDERLAY:
        mUnderlay = (mUnderlay + 1) % 3;
        Layout();
        break;
      case UI_SCALE:
        mUiScale = (mUiScale + 1) % 3;
        UiScaleManager::Get().SetScale(mUiScale == 0 ? 1.f : mUiScale == 1 ? 1.25f
                                                                           : 1.5f);
        Layout();
        break;
      case RENDER_SCALE:
        mRenderScale ^= 1;
        mLabel.SetRenderScale(mRenderScale ? 2.f : 1.f);
        break;
      case LINE_HEIGHT:
        mLineHeight = (mLineHeight + 1) % 3;
        mLabel.SetLineHeight(1.f + mLineHeight * .25f);
        break;
      case PROPERTY_ROUTE:
        mPropertyRoute = !mPropertyRoute;
        ApplyTextColor();
        break;
      case RESET:
        Reset();
        break;
      default:
        break;
    }
    HighlightSelection();
    RefreshStatus();
  }

  void Layout()
  {
    const float width         = std::max(1.f, mWidth - 24.f);
    const float previewHeight = std::clamp(mHeight * .4f, 110.f, 350.f);
    Place(mTitle, 12, 4, width, 34);
    Place(mPreview, 12, 42, width, previewHeight);
    const float labelWidth  = width * (.5f + mSize * .24f);
    const float labelHeight = previewHeight * (.5f + mSize * .24f);
    const float scale       = mUiScale == 0 ? 1.f : mUiScale == 1 ? 1.25f
                                                                  : 1.5f;
    Place(mLabel, (width - labelWidth) * .5f / scale, (previewHeight - labelHeight) * .5f / scale, labelWidth / scale, labelHeight / scale);
    for(unsigned i = 0; i < mTiles.size(); ++i)
    {
      const unsigned x = i % 16, y = i / 16;
      Place(mTiles[i], x * width / 16.f, y * previewHeight / 6.f, width / 16.f + .5f, previewHeight / 6.f + .5f);
      const uint32_t color = mUnderlay == 1 ? 0xE4C76A : mUnderlay == 2 ? ((x + y) % 2 ? 0xFFF4D1 : 0x142A46)
                                                                        : ((x + y) % 2 ? 0xF6CC56 : 0x286AC6);
      mTiles[i].SetBackgroundColor(UiColor(color));
    }
    const float statusY = 50 + previewHeight;
    Place(mStatus, 12, statusY, width, 86);
    Place(mGuide, 12, std::max(statusY + 90.f, mHeight - 54.f), width, 50);
    Place(mScroll, 12, statusY + 94, width, std::max(40.f, mHeight - statusY - 156.f));
    mColumns              = width < 600.f ? 2 : width < 1100.f ? 3
                                                               : 4;
    const float cellWidth = (width - (mColumns - 1) * GAP) / mColumns;
    mControls.SetRequestedWidth(width);
    mControls.SetRequestedHeight(((ACTION_COUNT + mColumns - 1) / mColumns) * 44.f);
    for(unsigned i = 0; i < mButtons.size(); ++i) Place(mButtons[i], (i % mColumns) * (cellWidth + GAP), (i / mColumns) * 44.f, cellWidth, 36);
  }

  void RefreshStatus()
  {
    TextureSet  textures;
    std::string sizes;
    for(unsigned r = 0; r < mLabel.GetRendererCount(); ++r)
    {
      auto set = mLabel.GetRendererAt(r).GetTextures();
      if(r == 0) textures = set;
      for(unsigned t = 0; set && t < set.GetTextureCount(); ++t)
      {
        auto texture = set.GetTexture(t);
        sizes += std::to_string(texture.GetWidth()) + "x" + std::to_string(texture.GetHeight()) + " ";
      }
    }
    if(textures != mTextures)
    {
      ++mTextureChanges;
      mTextures = textures;
    }
    char status[768];
    std::snprintf(status, sizeof(status), "%s | %s | Cutout %s | style %u | text alpha %.1f | BG %s / alpha %.1f\nH/V %u/%u | padding %u | multiline %s | max lines %u | %s | fit %u | wrap %u | color via %s\nRenderers %u | textures %s| texture-set changes %u",
                  CASES[mCase], mAsync ? "Async" : "Sync", mCutout ? "ON" : "OFF", mStyle, ALPHAS[mTextAlpha], mHasBackground ? "present" : "cleared", ALPHAS[mBgAlpha],
                  mHorizontal, mVertical, mPadding, mMultiline ? "ON" : "OFF", mMaxLines, mEllipsis ? "ELLIPSIS" : "CLIP", mFit, mWrap, mPropertyRoute ? "Property" : "API",
                  mLabel.GetRendererCount(), sizes.c_str(), mTextureChanges);
    if(mStatusText != status)
    {
      mStatusText = status;
      mStatus.SetText(status);
    }
  }

  void HighlightSelection()
  {
    for(unsigned i = 0; i < mButtons.size(); ++i) mButtons[i].SetBackgroundColor(UiColor(i == mSelection ? 0x305D96 : 0x243448));
  }

  bool OnTick()
  {
    RefreshStatus();
    return true;
  }

  void OnResize(Window, Window::WindowSize size)
  {
    mWidth  = size.GetWidth();
    mHeight = size.GetHeight();
    Layout();
  }

  void OnKeyEvent(Window, KeyEvent event)
  {
    if(event.GetState() != KeyEvent::UP) return;
    const auto key = event.GetKeyName();
    if(key == "Escape" || key == "BackSpace")
    {
      mApplication.Quit();
      return;
    }
    if(key == "Left" || key == "Right" || key == "Up" || key == "Down")
    {
      const unsigned delta = key == "Up" || key == "Down" ? mColumns : 1;
      mSelection           = (mSelection + ACTION_COUNT + (key == "Left" || key == "Up" ? -static_cast<int>(delta) : static_cast<int>(delta))) % ACTION_COUNT;
      mScroll.ScrollTo(mButtons[mSelection]);
      HighlightSelection();
      RefreshStatus();
      return;
    }
    if(key == "Return" || key == "KP_Enter")
      Activate(static_cast<Action>(mSelection));
    else if(key == "s" || key == "S")
      Activate(MODE);
    else if(key == "c" || key == "C")
      Activate(CUTOUT);
    else if(key == "a" || key == "A")
      Activate(TEXT_ALPHA);
    else if(key == "b" || key == "B")
      Activate(BG_COLOR);
    else if(key == "t" || key == "T")
      Activate(NEXT);
    else if(key == "r" || key == "R")
      Activate(RESET);
    else if(key == "1" || key == "2" || key == "3" || key == "4")
    {
      mStyle = key == "1" ? 0 : key == "2" ? 1
                              : key == "3" ? 2
                                           : 3;
      ApplyStyle();
      RefreshStatus();
    }
  }

  Application&       mApplication;
  AbsoluteLayout     mRoot, mPreview, mControls;
  ScrollView         mScroll;
  Label              mTitle, mLabel, mStatus, mGuide;
  std::vector<View>  mTiles;
  std::vector<Label> mButtons;
  Timer              mTimer;
  TextureSet         mTextures;
  std::string        mStatusText;
  float              mWidth{1280.f}, mHeight{720.f};
  unsigned           mCase{0}, mStyle{0}, mStylePreset{0}, mTextAlpha{0}, mTextRgb{0}, mBgColor{0}, mBgAlpha{2};
  unsigned           mHorizontal{1}, mVertical{1}, mPadding{1}, mSize{2}, mFont{1}, mMaxLines{0}, mFit{0}, mWrap{0};
  unsigned           mUnderlay{0}, mUiScale{0}, mRenderScale{0}, mLineHeight{0}, mSelection{0}, mColumns{4}, mTextureChanges{0};
  bool               mAsync{false}, mCutout{true}, mHasBackground{true}, mMultiline{false}, mEllipsis{true}, mPropertyRoute{false};
};

int main(int argc, char** argv)
{
  auto application = Application::New(&argc, &argv);
  UiConfig::New().Apply();
  TextCutoutController controller(application);
  application.MainLoop();
  return 0;
}
