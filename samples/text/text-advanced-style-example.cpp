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
#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/integration-api/text/reveal-integ.h>
#include <dali-ui-foundation/public-api/render-effects/gaussian-blur-effect.h>
#include <dali/devel-api/adaptor-framework/application.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

using namespace Dali;
using namespace Dali::Ui;
namespace RevealBlur = Dali::Ui::Integration::Text::Reveal;

namespace
{
enum class Scenario
{
  STATIC,
  OVERFLOW,
  GRADIENT,
  EMBOSS,
  MARQUEE,
  REVEAL,
  SCALE,
  LIFECYCLE,
  COUNT
};
struct ScenarioInfo
{
  const char* name;
  unsigned    presets;
};
constexpr ScenarioInfo SCENARIOS[]       = {{"Static paint", 13}, {"CLIP / ELLIPSIS", 6}, {"Gradient", 6}, {"Emboss", 5}, {"Marquee", 9}, {"Reveal", 11}, {"Async / scale", 4}, {"Lifecycle", 6}};
const char*            STATIC_NAMES[]    = {"Plain", "Shadow (-30, 0)", "Shadow (+30, 0)", "Shadow (0, -30)", "Shadow (0, +30)",
                                            "Shadow (-30, +25), blur 30", "Outline 12, offset (+15, -12), blur 12", "Solid underline 25",
                                            "Dashed underline 25, dash 12/8", "Shadow + outline + double underline", "Realistic shadow (+3,+3), blur 3",
                                            "Line-through 3", "Span background + underline + line-through"};
const char*            GRADIENT_NAMES[]  = {"Global gradient", "Gradient overlay", "GradientSpan: SPAN_BOUND", "GradientSpan: CONTENT_BOUND", "GradientSpan: VIEW_BOUND", "Global + overlay; Space animates offset"};
const char*            EMBOSS_NAMES[]    = {"Plain Emboss", "Native emoji Emboss", "Emboss + static shadow/outline", "Emboss + gradient overlay", "Strong displacement 24"};
const char*            MARQUEE_NAMES[]   = {"Seam: short source, gap 0, shadow/outline", "Seam: normal gap 24", "RTL", "Vertical multiline", "Global gradient", "Emboss seam", "Slow seam (12 px/s)", "Horizontal: vertical Shadow overflow", "Vertical: horizontal Shadow overflow"};
const char*            REVEAL_NAMES[]    = {"Static decorations at progress 0", "CHARACTER", "WORD", "LINE", "PIXEL", "PER_LINE", "Foreground blur", "Global gradient + overlay", "Emboss (blur intentionally unavailable)", "ImageSpan timing", "GradientSpan timing"};
const char*            LIFECYCLE_NAMES[] = {"Non-empty", "Empty", "Resize", "Decoration off", "Text replacement", "Disconnect / reconnect"};
const char*            CORPUS[]          = {"Hello text", "한글 텍스트", "مرحبا بالعالم", "á ë ñ ffi", "Hi <font family='Noto Color Emoji'>😀🌈</font>", "First line\nSecond line\nThird line"};
const char*            UNIT_NAMES[]      = {"CHARACTER", "WORD", "LINE", "PIXEL"};
const char*            EDGE_NAMES[]      = {"Center", "Left halo", "Right halo", "Top halo", "Bottom halo", "Offscreen"};
constexpr float        SCALES[]          = {1.f, 1.5f, 2.f};

void Place(View view, float x, float y, float width, float height)
{
  view.SetRequestedX(x);
  view.SetRequestedY(y);
  view.SetRequestedWidth(width);
  view.SetRequestedHeight(height);
}

Label Caption(const char* text, float x, float y, float width, float height, float size = 17.f)
{
  auto label = Label::New(text);
  Place(label, x, y, width, height);
  label.SetFontSize(size);
  label.SetTextColor(UiColor(0xEAF0FF));
  label.SetAsyncRendering(false);
  label.SetSystemFontSizeScaleEnabled(false);
  label.SetUiScalePolicy(UiScalePolicy::DISABLED);
  return label;
}

View CreateBounds(View parent, Actor target, uint32_t color, bool followPosition)
{
  auto marker = View::New();
  marker.SetUiScalePolicy(UiScalePolicy::DISABLED);
  marker.SetBackgroundColor(UiColor(Color::TRANSPARENT));
  marker.SetBorderlineColor(UiColor(color));
  marker.SetBorderlineWidth(1.f);
  marker.SetBorderlineOffset(-1.f);
  Place(marker, 0, 0, 1, 1);
  parent.Add(marker);
  // Track actual scene geometry, including UI scale; no Label background or
  // layout property is changed to visualize its bounds.
  for(auto property : {Actor::Property::SIZE, Actor::Property::POSITION})
  {
    if(property == Actor::Property::POSITION && !followPosition) continue;
    auto constraint = Constraint::New<Vector3>(marker, property, [](Vector3& value, const PropertyInputContainer& input)
    { value = input[0]->GetVector3(); });
    constraint.AddSource(Source(target, property));
    constraint.Apply();
  }
  return marker;
}

Gradient::Linear DiagnosticGradient()
{
  Gradient::Linear gradient(Vector2(-.5f, 0), Vector2(.5f, 0));
  gradient.SetStopNodes({{0.f, UiColor(Color::RED)}, {.5f, UiColor(Color::GREEN)}, {1.f, UiColor(Color::BLUE)}});
  return gradient;
}

void ApplyShadow(Label label, Vector2 offset, float blur)
{
  Text::Shadow style;
  style.SetOffset(offset);
  style.SetBlurRadius(blur);
  style.SetColor(UiColor(0x3985FF));
  label.SetTextShadow(style);
}

void Outline(Label label, unsigned width, Vector2 offset = Vector2::ZERO, float blur = 0.f)
{
  Text::Outline style;
  style.SetWidth(width);
  style.SetOffset(offset);
  style.SetBlurRadius(blur);
  style.SetColor(UiColor(0xFF384A));
  label.SetTextOutline(style);
}

void Underline(Label label, Text::Underline::Type type, float thickness)
{
  Text::Underline style;
  style.SetType(type);
  style.SetThickness(thickness);
  style.SetDashLength(12);
  style.SetDashGap(8);
  style.SetColor(UiColor(0xFFE84A));
  label.SetTextUnderline(style);
}
} // namespace

class AdvancedTextStyle : public ConnectionTracker
{
public:
  AdvancedTextStyle(Application& application, bool autoSmoke)
  : mApplication(application),
    mAutoSmoke(autoSmoke)
  {
    application.InitSignal().Connect(this, &AdvancedTextStyle::OnInit);
  }

private:
  void OnInit(Application application)
  {
    auto window   = application.GetWindow();
    auto position = window.GetPositionSize();
    window.SetPositionSize(PositionSize(position.x, position.y, 1200, 760));
    window.SetBackgroundColor(UiColor(0x101723));
    mRoot = View::New();
    mRoot.SetUiScalePolicy(UiScalePolicy::DISABLED);
    Place(mRoot, 0, 0, MATCH_PARENT, MATCH_PARENT);
    window.Add(mRoot);
    mRoot.Add(Caption("Advanced Text Style", 24, 12, 1152, 42, 28));
    mTitle = Caption("", 24, 60, 1152, 38, 21);
    mNote  = Caption("", 24, 102, 1152, 60, 16);
    mNote.SetMultiLine(true);
    mRoot.Add(mTitle);
    mRoot.Add(mNote);
    for(unsigned i = 0; i < 2; ++i)
    {
      mSideNames[i] = Caption(i ? "SHARED (opt-in)" : "BASELINE", 48 + i * 600, 170, 510, 32, 20);
      mRoot.Add(mSideNames[i]);
      mEdgePanels[i] = View::New();
      Place(mEdgePanels[i], i * 600.f, 220, 600, 280);
      mRoot.Add(mEdgePanels[i]);
      mParents[i] = View::New();
      mParents[i].SetUiScalePolicy(UiScalePolicy::DISABLED);
      mRoot.Add(mParents[i]);
      mLabels[i] = Caption("", 15, 15, 260, 76, 40);
      mLabels[i].SetUiScalePolicy(UiScalePolicy::ENABLED);
      mLabels[i].SetFontFamily("DejaVu Sans");
      mLabels[i].SetTextColor(UiColor(Color::WHITE));
      mParents[i].Add(mLabels[i]);
      mBounds[i]       = CreateBounds(mParents[i], mLabels[i], 0x8197B6, true);
      mParentBounds[i] = CreateBounds(mParents[i], mParents[i], 0x66AC70, false);
    }
    mStatus = Caption("", 24, 510, 1152, 104, 16);
    mStatus.SetMultiLine(true);
    mRoot.Add(mStatus);
    mHelp = Caption(
      "Left/Right: scenario   Up/Down: preset   Space: play/stop   R: reset/restart   I: Marquee stop mode\n"
      "Y: Sync/Async   T: CLIP/ELLIPSIS   C/P: Label/Parent clip   O: shared opt-in   Tab: split/baseline/shared\n"
      "Q: RenderScale   W: UI scale   S: scene scale   A: alignment   J: corpus   M: multiline\n"
      "Reveal: 0/1/2/3/4 = 0/.25/.5/.75/1   U: unit   K: sequence   L: blur   V: blur quality/performance\n"
      "Marquee: H: orientation   D: gap 0/24   B/G: Label/Parent bounds   E: culling edge   F: RenderEffect\n"
      "Z: scripted smoke   Esc/Back: quit   Font fallback depends on installed fonts.",
      24, 620, 1152, 132, 15);
    mHelp.SetMultiLine(true);
    mRoot.Add(mHelp);
    window.KeyEventSignal().Connect(this, &AdvancedTextStyle::OnKeyEvent);
    mHudTimer = Timer::New(200);
    mHudTimer.TickSignal().Connect(this, &AdvancedTextStyle::UpdateHud);
    mHudTimer.Start();
    Apply();
    if(mAutoSmoke) StartSmoke();
  }

  void UpdateEdgeViewport(unsigned index)
  {
    auto  window = mApplication.GetWindow();
    auto& task   = mEdgeTasks[index];
    if(mEdgeMode == 0)
    {
      if(task)
      {
        window.Remove(task.GetCameraActor());
        window.GetRenderTaskList().RemoveTask(task);
        task.Reset();
      }
    }
    else if(!task)
    {
      task = window.GetRenderTaskList().CreateTask();
      task.SetSourceActor(mEdgePanels[index]);
      task.SetExclusive(true);
      task.SetInputEnabled(false);
      task.SetClearEnabled(false);
      task.SetViewport(Viewport(index * 600, 220, 600, 280));
      auto camera = CameraActor::New(Vector2(600, 280));
      camera.SetProperty(Actor::Property::PARENT_ORIGIN, ParentOrigin::TOP_LEFT);
      camera.SetProperty(Actor::Property::POSITION_X, index * 600.f + 300.f);
      camera.SetProperty(Actor::Property::POSITION_Y, 360.f);
      window.Add(camera);
      task.SetCameraActor(camera);
    }
  }

  const char* PresetName() const
  {
    switch(mScenario)
    {
      case Scenario::STATIC:
        return STATIC_NAMES[mPreset];
      case Scenario::GRADIENT:
        return GRADIENT_NAMES[mPreset];
      case Scenario::EMBOSS:
        return EMBOSS_NAMES[mPreset];
      case Scenario::MARQUEE:
        return MARQUEE_NAMES[mPreset];
      case Scenario::REVEAL:
        return REVEAL_NAMES[mPreset];
      case Scenario::LIFECYCLE:
        return LIFECYCLE_NAMES[mPreset];
      case Scenario::OVERFLOW:
        return "START / CENTER / END, large effects";
      case Scenario::SCALE:
        return "Same controls: UI/RenderScale 1 -> 1.5 -> 2 -> 1";
      default:
        return "";
    }
  }

  void StaticStyle(Label label, unsigned preset)
  {
    if((preset >= 1 && preset <= 5) || preset == 9)
    {
      const Vector2 offsets[] = {Vector2(-30, 0), Vector2(30, 0), Vector2(0, -30), Vector2(0, 30), Vector2(-30, 25)};
      ApplyShadow(label, offsets[preset == 9 ? 4 : preset - 1], preset >= 5 ? 30.f : 0.f);
    }
    if(preset == 6 || preset == 9) Outline(label, 12, Vector2(15, -12), 12);
    if(preset >= 7 && preset <= 9) Underline(label, static_cast<Text::Underline::Type>(preset - 7), 25);
    if(preset == 10) ApplyShadow(label, Vector2(3, 3), 3);
    if(preset == 11)
    {
      Text::LineThrough style;
      style.SetThickness(3);
      style.SetColor(UiColor(Color::YELLOW));
      label.SetTextLineThrough(style);
    }
    if(preset == 12) label.SetStyledText(Text::StyledText::FromMarkup("<background color='#374151'><u height='3'><s height='2'>Styled text</s></u></background>"));
  }

  void SpanText(Label label, bool image)
  {
    auto builder = Text::StyledTextBuilder::New("Text ");
    if(image)
    {
      builder.AppendText(Text::ReplacementSpan::OBJECT_REPLACEMENT_CHARACTER);
      Text::ImageAttributes attributes(RESOURCES_DIR "flag_kr.png", Vector2(44, 30));
      attributes.SetAlignment(Text::ImageAttributes::InlineAlignment::TEXT_CENTER);
      builder.SetSpan(Text::ImageSpan::New(attributes), 5, 6);
      builder.AppendText(" image text");
    }
    else
    {
      builder.AppendText("gradient span");
      const unsigned mode = mScenario == Scenario::GRADIENT ? mPreset - 2 : 0;
      builder.SetSpan(Text::GradientSpan::New(DiagnosticGradient(), static_cast<Text::GradientSpan::BoundsMode>(mode)), 5, builder.GetUtf32Length());
    }
    label.SetStyledText(builder.Build());
  }

  void Configure(Label label)
  {
    label.StopMarquee();
    label.SetTextReveal(Text::Reveal::None());
    label.SetTextShadow(Text::Shadow::None());
    label.SetTextOutline(Text::Outline::None());
    label.SetTextUnderline(Text::Underline::None());
    label.SetTextLineThrough(Text::LineThrough::None());
    label.SetTextBevel(Text::Bevel::None());
    label.SetTextGradient(Gradient::Base::None());
    label.SetTextGradientOverlay(Gradient::Base::None());
    label.SetMultiLine(mMultiline || mCorpus == 5 || (mScenario == Scenario::MARQUEE && mVertical));
    label.SetStyledText(Text::StyledText::FromMarkup(CORPUS[mCorpus]));
    label.SetLayoutDirectionMode(Text::LayoutDirectionMode::CONTENTS);
    label.SetTextOverflowMode(mEllipsis ? Text::OverflowMode::ELLIPSIS : Text::OverflowMode::CLIP);
    label.SetHorizontalTextAlignment(static_cast<Text::Alignment>(mAlignment % 3));
    label.SetVerticalTextAlignment(static_cast<Text::Alignment>(mAlignment / 3));
    auto gradient = DiagnosticGradient();
    switch(mScenario)
    {
      case Scenario::STATIC:
        StaticStyle(label, mPreset);
        break;
      case Scenario::OVERFLOW:
        label.SetText("Long text crosses the content boundary");
        ApplyShadow(label, Vector2(-24, 18), 10);
        Outline(label, 8, Vector2(10, -6));
        break;
      case Scenario::GRADIENT:
        ApplyShadow(label, Vector2(-20, 12), 4);
        if(mPreset == 0 || mPreset == 5) label.SetTextGradient(gradient);
        if(mPreset == 1 || mPreset == 5) label.SetTextGradientOverlay(gradient);
        if(mPreset >= 2 && mPreset <= 4) SpanText(label, false);
        break;
      case Scenario::EMBOSS:
      {
        Text::Bevel bevel;
        bevel.SetIntensity(mPreset == 4 ? 24 : 4);
        bevel.SetDirection(Vector2(-1, 1));
        label.SetTextBevel(bevel);
        if(mPreset == 1) label.SetStyledText(Text::StyledText::FromMarkup(CORPUS[4]));
        if(mPreset == 2)
        {
          ApplyShadow(label, Vector2(-18, 10), 4);
          Outline(label, 4);
        }
        if(mPreset == 3) label.SetTextGradientOverlay(gradient);
        break;
      }
      case Scenario::MARQUEE:
        label.SetText(mVertical ? "First line\nSecond line\nThird line" : mPreset == 2 ? "مرحبا"
                                                                                       : "Hi");
        label.SetMultiLine(mVertical);
        ApplyShadow(label, Vector2(-22, 10), 5);
        Outline(label, 5);
        if(mPreset >= 7)
        {
          label.SetText(mVertical ? "MM\nMM\nMM" : "MMMMMMMM");
          label.SetTextOutline(Text::Outline::None());
          ApplyShadow(label, mVertical ? Vector2(24, 0) : Vector2(0, 24), 5);
        }
        label.SetMarqueeTriggerPolicy(Text::MarqueeTriggerPolicy::MANUAL);
        label.SetMarqueeOrientation(mVertical ? Text::MarqueeOrientation::VERTICAL : Text::MarqueeOrientation::HORIZONTAL);
        label.SetMarqueeGap(mGap ? 24 : 0);
        label.SetMarqueeSpeed(mPreset == 6 ? 12 : 40);
        label.SetMarqueeLoopCount(0);
        label.SetMarqueeStopMode(mStopMode);
        if(mPreset == 4) label.SetTextGradient(gradient);
        if(mPreset == 5)
        {
          Text::Bevel bevel;
          bevel.SetIntensity(10);
          label.SetTextBevel(bevel);
        }
        break;
      case Scenario::REVEAL:
      {
        if(mPreset == 0)
        {
          label.SetStyledText(Text::StyledText::FromMarkup("<background color='#374151'>Reveal text</background>"));
          Outline(label, 5);
          ApplyShadow(label, Vector2(12, 8), 3);
          Underline(label, Text::Underline::Type::DASHED, 4);
        }
        if(mPreset == 3 || mPreset == 5)
        {
          label.SetText("First line\nSecond line\nThird line");
          label.SetMultiLine(true);
        }
        if(mPreset == 7)
        {
          label.SetTextGradient(gradient);
          label.SetTextGradientOverlay(gradient);
        }
        if(mPreset == 8)
        {
          Text::Bevel bevel;
          bevel.SetIntensity(5);
          label.SetTextBevel(bevel);
        }
        if(mPreset >= 9) SpanText(label, mPreset == 9);
        Text::Reveal reveal;
        reveal.SetUnit(static_cast<Text::Reveal::Unit>(mUnit));
        reveal.SetSequence(mPerLine ? Text::Reveal::Sequence::PER_LINE : Text::Reveal::Sequence::WHOLE_TEXT);
        reveal.SetFadeDurationRatio(.2f);
        reveal.SetSequenceStaggerRatio(.25f);
        RevealBlur::SetBlurRadius(reveal, mBlur ? 6.f : 0.f);
        RevealBlur::SetBlurDurationRatio(reveal, .3f);
        RevealBlur::SetBlurMode(reveal, mQuality ? RevealBlur::BlurMode::QUALITY : RevealBlur::BlurMode::PERFORMANCE);
        label.SetTextReveal(reveal);
        label.SetTextRevealProgress(mProgress);
        break;
      }
      case Scenario::SCALE:
        Outline(label, 3);
        ApplyShadow(label, Vector2(5, 5), 3);
        label.SetStyledText(Text::StyledText::FromMarkup(CORPUS[4]));
        break;
      case Scenario::LIFECYCLE:
        if(mPreset == 1) label.SetText("");
        if(mPreset == 4) label.SetStyledText(Text::StyledText::FromMarkup(CORPUS[(mCorpus + 1) % 6]));
        if(mPreset != 3)
        {
          Outline(label, 3);
          ApplyShadow(label, Vector2(-10, 8), 4);
        }
        break;
      default:
        break;
    }
  }

  Vector2 PreviewSize() const
  {
    if(mScenario == Scenario::MARQUEE && mPreset >= 7) return mVertical ? Vector2(80, 98) : Vector2(190, 44);
    return Vector2(mScenario == Scenario::LIFECYCLE && mPreset == 2 ? 140.f : 260.f,
                   mScenario == Scenario::REVEAL || mMultiline || mCorpus == 5 ? 140.f : 76.f);
  }

  void ArrangePreview(unsigned index)
  {
    UpdateEdgeViewport(index);
    const bool visible = mComparison == 0 || (mComparison == 1 ? index == 0 : index == 1);
    mParents[index].SetProperty(Actor::Property::VISIBLE, visible);
    mSideNames[index].SetProperty(Actor::Property::VISIBLE, visible);
    mSideNames[index].SetRequestedX(mComparison == 0 ? 48.f + index * 600.f : 348.f);
    const float ui         = SCALES[mUiScale];
    const auto  size       = PreviewSize() * ui;
    const auto  parentSize = size + Vector2(30.f * ui, 30.f * ui);
    auto        parent     = mParents[index];
    if(mEdgeMode == 0)
    {
      mRoot.Add(parent);
      const float centerX = mComparison == 0 ? 300.f + index * 600.f : 600.f;
      Place(parent, centerX - parentSize.x * .5f, 350.f - parentSize.y * .5f, parentSize.x, parentSize.y);
    }
    else
    {
      mEdgePanels[index].Add(parent);
      const float sceneScale = std::array<float, 3>{1.f, .75f, 1.5f}[mSceneScale];
      Vector2     center(300, 140);
      if(mEdgeMode == 1 || mEdgeMode == 5) center.x = -(size.x * .5f + (mEdgeMode == 5 ? 100.f : 8.f)) * sceneScale;
      if(mEdgeMode == 2) center.x = 600.f + (size.x * .5f + 8.f) * sceneScale;
      if(mEdgeMode == 3) center.y = -(size.y * .5f + 8.f) * sceneScale;
      if(mEdgeMode == 4) center.y = 280.f + (size.y * .5f + 8.f) * sceneScale;
      Place(parent, center.x - parentSize.x * .5f + .25f, center.y - parentSize.y * .5f + .5f, parentSize.x, parentSize.y);
    }
  }

  void Apply()
  {
    if(mPlaying && mScenario == Scenario::REVEAL) mProgress = mLabels[0].GetTextRevealProgress();
    if(mAnimation)
    {
      mAnimation.Stop();
      mAnimation.Reset();
    }
    const auto  size  = PreviewSize();
    const float width = size.x, height = size.y;
    const float sceneScale = std::array<float, 3>{1.f, .75f, 1.5f}[mSceneScale];
    for(unsigned i = 0; i < 2; ++i)
    {
      ArrangePreview(i);
      auto parent = mParents[i];
      auto label  = mLabels[i];
      if(mScenario == Scenario::LIFECYCLE && mPreset == 5) label.Unparent();
      parent.Add(label);
      Place(label, 15, 15, width, height);
      Configure(label);
      label.SetAsyncRendering(mAsync);
      label.SetRenderScale(SCALES[mRenderScale]);
      static_cast<Ui::Integration::LabelImpl&>(GetImpl(label)).SetEffectOverflowEnabled(i == 1 && mAdvanced);
      label.SetProperty(Actor::Property::CLIPPING_MODE, mLabelClip ? ClippingMode::CLIP_TO_BOUNDING_BOX : ClippingMode::DISABLED);
      parent.SetProperty(Actor::Property::CLIPPING_MODE, mParentClip ? ClippingMode::CLIP_TO_BOUNDING_BOX : ClippingMode::DISABLED);
      parent.SetProperty(Actor::Property::SCALE, Vector3(sceneScale, sceneScale, 1));
      if(mRenderEffect && !label.GetRenderEffect())
      {
        auto effect = GaussianBlurEffect::New(4u);
        effect.SetBlurDownscaleFactor(1.f);
        effect.SetDitherNoiseStrength(0.f);
        label.SetRenderEffect(effect);
      }
      else if(!mRenderEffect && label.GetRenderEffect())
        label.ClearRenderEffect();
      mBounds[i].SetProperty(Actor::Property::VISIBLE, mShowBounds);
      mParentBounds[i].SetProperty(Actor::Property::VISIBLE, mShowParentBounds);
    }
    if(mPlaying) StartPlayback();
    UpdateStatus(true);
  }

  void SelectPreset()
  {
    mProgress = 0.f;
    mPlaying  = false;
    mForward  = true;
    mUnit     = mScenario == Scenario::REVEAL && mPreset >= 1 && mPreset <= 4 ? mPreset - 1 : 0;
    mPerLine  = mScenario == Scenario::REVEAL && mPreset == 5;
    mBlur     = mScenario == Scenario::REVEAL && mPreset == 6;
    mVertical = mScenario == Scenario::MARQUEE && (mPreset == 3 || mPreset == 8);
    mGap      = mScenario == Scenario::MARQUEE && mPreset == 1;
    if(mScenario == Scenario::OVERFLOW)
    {
      mAlignment = mPreset % 3;
      mEllipsis  = mPreset >= 3;
    }
    if(mScenario == Scenario::STATIC && mPreset == 10) mAlignment = 8;
    if(mScenario == Scenario::SCALE)
    {
      mRenderScale = mUiScale = mPreset % 3;
      UiScaleManager::Get().SetScale(SCALES[mUiScale]);
    }
    Apply();
  }

  void StartPlayback()
  {
    if(mScenario == Scenario::MARQUEE)
    {
      for(auto label : mLabels) label.StartMarquee();
      return;
    }
    if(mScenario != Scenario::REVEAL && mScenario != Scenario::GRADIENT) return;
    if(mAnimation)
    {
      mAnimation.Play();
      return;
    }
    mAnimation = Animation::New(3.f);
    for(auto label : mLabels)
    {
      if(mScenario == Scenario::REVEAL)
        label.Animate(mAnimation).TextRevealProgress(mForward ? 1.f : 0.f, Duration(3.f));
      else
      {
        if(mPreset == 0 || mPreset == 5) label.Animate(mAnimation).TextGradientStartOffset(mForward ? 1.f : 0.f, Duration(3.f));
        if(mPreset == 1 || mPreset == 5) label.Animate(mAnimation).TextGradientOverlayStartOffset(mForward ? 1.f : 0.f, Duration(3.f));
      }
    }
    mAnimation.FinishedSignal().Connect(this, &AdvancedTextStyle::AnimationFinished);
    mAnimation.Play();
  }

  void AnimationFinished(Animation)
  {
    mAnimation.Reset();
    mForward = !mForward;
    if(mPlaying) StartPlayback();
  }

  void UpdateStatus(bool log)
  {
    char        text[1200];
    const auto& info = SCENARIOS[static_cast<unsigned>(mScenario)];
    std::snprintf(text, sizeof(text), "%s  |  %u/%u: %s", info.name, mPreset + 1, info.presets, PresetName());
    mTitle.SetText(text);
    const char* note = mScenario == Scenario::REVEAL    ? "Expected: foreground timing only; decorations stay STATIC at progress 0. Reveal uses bounded paint.\nEmboss uses source timing at all three samples; Emboss + Reveal blur remains unsupported."
                       : mScenario == Scenario::MARQUEE ? (mVertical ? "Shared Marquee clips Y only; X effects may overflow. Actor/parent clip is applied last.\nSpace starts/stops; R restarts without reconfiguration. I: IMMEDIATE / FINISH_LOOP."
                                                                     : "Shared Marquee clips X only; Y effects may overflow. Actor/parent clip is applied last.\nSpace starts/stops; R restarts without reconfiguration. I: IMMEDIATE / FINISH_LOOP.")
                                                        : "Expected difference: shared static effects may extend beyond bounds. Actor/parent clip is applied last.\nBaseline is a temporary comparison. RenderEffect capture retains its existing bounded policy.";
    mNote.SetText(note);
    if(mScenario == Scenario::REVEAL && mPlaying) mProgress = mLabels[0].GetTextRevealProgress();
    std::snprintf(text, sizeof(text),
                  "%s | View: %s | Shared opt-in: %s | Overflow: %s | Label/Parent clip: %d/%d\n"
                  "RenderScale %.2f (Async) | UI %.2f | Scene %.2f | Alignment %u/%u | Edge: %s | RenderEffect %d\n"
                  "Reveal %s / %s | Progress %.2f | Blur %s (%s) | Marquee %s / %s / gap %d / %s\n"
                  "Shadow %d Outline %d Gradient %d Overlay %d Emboss %d | Bounds %d/%d | Playback %s | Z: smoke %s",
                  mAsync ? "ASYNC" : "SYNC", mComparison == 0 ? "side by side" : mComparison == 1 ? "baseline"
                                                                                                  : "shared",
                  mAdvanced ? "ON" : "OFF",
                  mEllipsis ? "ELLIPSIS" : "CLIP", mLabelClip, mParentClip, SCALES[mRenderScale], SCALES[mUiScale], std::array<float, 3>{1.f, .75f, 1.5f}[mSceneScale],
                  mAlignment % 3, mAlignment / 3, EDGE_NAMES[mEdgeMode], mRenderEffect, UNIT_NAMES[mUnit], mPerLine ? "PER_LINE" : "WHOLE_TEXT", mProgress,
                  mBlur ? "requested" : "OFF", mQuality ? "quality" : "performance", (mLabels[0].IsMarqueeRunning() || mLabels[1].IsMarqueeRunning()) ? (mPlaying ? "running" : "finishing loop") : "stopped",
                  mVertical ? "vertical" : "horizontal", mGap ? 24 : 0, mStopMode == Text::MarqueeStopMode::IMMEDIATE ? "IMMEDIATE" : "FINISH_LOOP",
                  mLabels[0].GetTextShadow() != Text::Shadow::None(), mLabels[0].GetTextOutline() != Text::Outline::None(),
                  mLabels[0].GetTextGradient().GetType() != Gradient::Type::NONE, mLabels[0].GetTextGradientOverlay().GetType() != Gradient::Type::NONE,
                  mLabels[0].GetTextBevel() != Text::Bevel::None(), mShowBounds, mShowParentBounds, mPlaying ? "playing" : "paused", mSmokeStep >= 0 ? "RUNNING" : "idle");
    mStatus.SetText(text);
    if(log)
    {
      for(char* c = text; *c; ++c)
        if(*c == '\n') *c = ' ';
      std::printf("[ADV-TEXT] scenario=%u preset=%u %s\n", static_cast<unsigned>(mScenario), mPreset, text);
      std::fflush(stdout);
    }
  }

  bool UpdateHud()
  {
    if((mPlaying && mScenario == Scenario::REVEAL) || mScenario == Scenario::MARQUEE) UpdateStatus(false);
    return true;
  }

  void StartSmoke()
  {
    if(mSmokeTimer) mSmokeTimer.Stop();
    mEdgeMode = mComparison = mSceneScale = mCorpus = mAlignment = 0;
    mLabelClip = mParentClip = mRenderEffect = mMultiline = mEllipsis = false;
    mAdvanced = mShowBounds = mQuality = true;
    mShowParentBounds                  = false;
    mSmokeStep                         = 0;
    mSmokeTimer                        = Timer::New(1200);
    mSmokeTimer.TickSignal().Connect(this, &AdvancedTextStyle::SmokeTick);
    mSmokeTimer.Start();
    SmokeTick();
  }

  bool SmokeTick()
  {
    if(mSmokeStep >= 38)
    {
      mSmokeStep  = -1;
      mScenario   = Scenario::STATIC;
      mPreset     = 0;
      mAsync      = false;
      mAdvanced   = true;
      mComparison = 0;
      mUiScale = mRenderScale = mSceneScale = mEdgeMode = mAlignment = 0;
      mLabelClip = mParentClip = mRenderEffect = mMultiline = mEllipsis = false;
      mCorpus                                                           = 0;
      UiScaleManager::Get().SetScale(1.f);
      SelectPreset();
      std::printf("[ADV-TEXT] smoke=complete final=STATIC/plain/SYNC/scale1\n");
      if(mAutoSmoke) mApplication.Quit();
      return false;
    }
    const unsigned step = mSmokeStep++;
    if(step >= 28)
    {
      const unsigned phase = (step - 28) % 5;
      if(phase == 0)
      {
        mScenario = Scenario::MARQUEE;
        mPreset = step < 33 ? 7 : 8;
        mAsync = step >= 33;
        mEllipsis = false;
        mRenderScale = mUiScale = 0;
        UiScaleManager::Get().SetScale(1.f);
        mStopMode = Text::MarqueeStopMode::IMMEDIATE;
        SelectPreset();
      }
      else
      {
        mPlaying = phase % 2;
        if(mPlaying) StartPlayback();
        else for(auto label : mLabels) label.StopMarquee();
        UpdateStatus(true);
      }
      return true;
    }
    mScenario           = static_cast<Scenario>((step / 2) % static_cast<unsigned>(Scenario::COUNT));
    mPreset             = mScenario == Scenario::STATIC ? 9 : mScenario == Scenario::REVEAL ? 0
                                                                                            : step % SCENARIOS[static_cast<unsigned>(mScenario)].presets;
    mAsync              = step % 2;
    mRenderScale        = step % 3;
    mUiScale            = (step / 3) % 3;
    UiScaleManager::Get().SetScale(SCALES[mUiScale]);
    SelectPreset();
    if(mScenario == Scenario::MARQUEE)
    {
      mPlaying = true;
      StartPlayback();
      UpdateStatus(true);
    }
    if(step >= 16)
    {
      mScenario = Scenario::REVEAL;
      mPreset   = step >= 23 ? 6 : 0;
      mUnit     = step % 4;
      mPerLine  = step % 2;
      mBlur     = mPreset == 6;
      mProgress = float(step % 5) * .25f;
      Apply();
    }
    return true;
  }

  void OnKeyEvent(Window, KeyEvent event)
  {
    if(event.GetState() != KeyEvent::UP) return;
    if(IsKey(event, DALI_KEY_ESCAPE) || IsKey(event, DALI_KEY_BACK))
    {
      mApplication.Quit();
      return;
    }
    if(mSmokeStep >= 0)
    {
      mSmokeTimer.Stop();
      mSmokeStep = -1;
    }
    const auto key = event.GetKeyName();
    if(key == "Left" || key == "Right")
    {
      const unsigned count = static_cast<unsigned>(Scenario::COUNT);
      mScenario            = static_cast<Scenario>((static_cast<unsigned>(mScenario) + (key == "Right" ? 1 : count - 1)) % count);
      mPreset = mEdgeMode = 0;
      SelectPreset();
      return;
    }
    if(key == "Up" || key == "Down")
    {
      const unsigned count = SCENARIOS[static_cast<unsigned>(mScenario)].presets;
      mPreset              = (mPreset + (key == "Up" ? 1 : count - 1)) % count;
      SelectPreset();
      return;
    }
    if(key == "space" || key == "Return")
    {
      if(mScenario != Scenario::REVEAL && mScenario != Scenario::MARQUEE &&
         !(mScenario == Scenario::GRADIENT && (mPreset == 0 || mPreset == 1 || mPreset == 5))) return;
      mPlaying = !mPlaying;
      if(mPlaying)
        StartPlayback();
      else
      {
        if(mAnimation) mAnimation.Pause();
        for(auto label : mLabels) label.StopMarquee();
      }
      UpdateStatus(true);
      return;
    }
    if(key == "Tab")
    {
      mComparison = (mComparison + 1) % 3;
      Apply();
      return;
    }
    if(key.Size() != 1) return;
    char c = key[0];
    if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if(c >= '0' && c <= '4')
    {
      mPlaying = false;
      if(mAnimation)
      {
        mAnimation.Stop();
        mAnimation.Reset();
      }
      mProgress = (c - '0') * .25f;
      for(auto label : mLabels) label.SetTextRevealProgress(mProgress);
      UpdateStatus(true);
      return;
    }
    switch(c)
    {
      case 'i':
        mStopMode = mStopMode == Text::MarqueeStopMode::IMMEDIATE ? Text::MarqueeStopMode::FINISH_LOOP : Text::MarqueeStopMode::IMMEDIATE;
        for(auto label : mLabels) label.SetMarqueeStopMode(mStopMode);
        UpdateStatus(true);
        return;
      case 'r':
        if(mScenario == Scenario::MARQUEE)
        {
          for(auto label : mLabels)
          {
            label.SetMarqueeStopMode(Text::MarqueeStopMode::IMMEDIATE);
            label.StopMarquee();
            label.SetMarqueeStopMode(mStopMode);
          }
          mPlaying = true;
          StartPlayback();
          UpdateStatus(true);
        }
        else SelectPreset();
        return;
      case 'z':
        StartSmoke();
        return;
      case 'q':
        mRenderScale = (mRenderScale + 1) % 3;
        for(auto label : mLabels) label.SetRenderScale(SCALES[mRenderScale]);
        UpdateStatus(true);
        return;
      case 'w':
        mUiScale = (mUiScale + 1) % 3;
        UiScaleManager::Get().SetScale(SCALES[mUiScale]);
        for(unsigned i = 0; i < 2; ++i) ArrangePreview(i);
        UpdateStatus(true);
        return;
      case 'y':
        mAsync = !mAsync;
        break;
      case 't':
        mEllipsis = !mEllipsis;
        break;
      case 'c':
        mLabelClip = !mLabelClip;
        break;
      case 'p':
        mParentClip = !mParentClip;
        break;
      case 'o':
        mAdvanced = !mAdvanced;
        break;
      case 's':
        mSceneScale = (mSceneScale + 1) % 3;
        break;
      case 'a':
        mAlignment = (mAlignment + 1) % 9;
        break;
      case 'j':
        mCorpus = (mCorpus + 1) % 6;
        break;
      case 'm':
        mMultiline = !mMultiline;
        break;
      case 'u':
        mUnit = (mUnit + 1) % 4;
        break;
      case 'k':
        mPerLine = !mPerLine;
        break;
      case 'l':
        mBlur = !mBlur;
        break;
      case 'v':
        mQuality = !mQuality;
        break;
      case 'h':
        mVertical = !mVertical;
        break;
      case 'd':
        mGap = !mGap;
        break;
      case 'b':
        mShowBounds = !mShowBounds;
        break;
      case 'g':
        mShowParentBounds = !mShowParentBounds;
        break;
      case 'f':
        mRenderEffect = !mRenderEffect;
        break;
      case 'e':
      {
        mEdgeMode = (mEdgeMode + 1) % 6;
        if(mEdgeMode)
        {
          const unsigned presets[] = {0, 2, 1, 4, 3, 2}, alignments[] = {0, 5, 3, 7, 1, 5};
          mScenario  = Scenario::STATIC;
          mPreset    = presets[mEdgeMode];
          mAlignment = alignments[mEdgeMode];
        }
        break;
      }
      default:
        return;
    }
    Apply();
  }

  Application&              mApplication;
  View                      mRoot;
  std::array<Label, 2>      mLabels, mSideNames;
  std::array<View, 2>       mParents, mBounds, mParentBounds, mEdgePanels;
  std::array<RenderTask, 2> mEdgeTasks;
  Label                     mTitle, mNote, mStatus, mHelp;
  Timer                     mHudTimer, mSmokeTimer;
  Animation                 mAnimation;
  Scenario                  mScenario{Scenario::STATIC};
  Text::MarqueeStopMode      mStopMode{Text::MarqueeStopMode::IMMEDIATE};
  unsigned                  mPreset{5}, mAlignment{0}, mSceneScale{0}, mEdgeMode{0}, mComparison{0};
  unsigned                  mCorpus{0}, mRenderScale{0}, mUiScale{0}, mUnit{0};
  float                     mProgress{0.f};
  int                       mSmokeStep{-1};
  bool                      mAutoSmoke{false}, mAdvanced{true}, mAsync{false}, mEllipsis{false};
  bool                      mRenderEffect{false}, mLabelClip{false}, mParentClip{false}, mMultiline{false};
  bool                      mShowBounds{true}, mShowParentBounds{false}, mPerLine{false}, mBlur{false}, mQuality{true};
  bool                      mVertical{false}, mGap{false}, mPlaying{false}, mForward{true};
};

int main(int argc, char** argv)
{
  bool smoke = false;
  for(int i = 1; i < argc; ++i)
  {
    if(std::strcmp(argv[i], "--auto-smoke") == 0)
    {
      smoke = true;
      for(int j = i; j < argc; ++j) argv[j] = argv[j + 1];
      --argc;
      --i;
    }
  }
  Application       app = Application::New(&argc, &argv);
  AdvancedTextStyle controller(app, smoke);
  app.MainLoop();
  return 0;
}
