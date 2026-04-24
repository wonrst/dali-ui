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

// EXTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/text-visualizer/atlas-view-adapter.h>
#include <dali-ui-foundation/internal/text/text-visualizer/layout-types.h>
#include <dali-ui-foundation/internal/text/text-visualizer/prepared-text.h>
#include <dali-ui-foundation/public-api/text/text-visualizer-properties.h>
#include <dali-ui-foundation/public-api/ui-color.h>
#include <dali-ui-foundation/public-api/view-impl.h>
#include <dali/public-api/math/rect.h>

namespace Dali
{

namespace Ui
{

namespace Integration
{

class TextVisualizerImpl;
using TextVisualizerImplPtr = IntrusivePtr<TextVisualizerImpl>;

/**
 * @brief This is the internal implementation class for TextVisualizer.
 *
 * This commit intentionally keeps the implementation minimal.
 */
class DALI_UI_API TextVisualizerImpl : public ViewImpl
{
public:
  /**
   * @brief Creates a new TextVisualizer.
   */
  static TextVisualizerImplPtr New();

protected:
  /**
   * A reference counted object may only be deleted by calling Unreference()
   */
  virtual ~TextVisualizerImpl();

public:
  // API

  /**
   * @copydoc Dali::Ui::TextVisualizer::SetText
   */
  void SetText(const Dali::String& text);

  /**
   * @copydoc Dali::Ui::TextVisualizer::GetText
   */
  Dali::String GetText() const;

  /**
   * @copydoc Dali::Ui::TextVisualizer::SetFontFamily
   */
  void SetFontFamily(const Dali::String& fontFamily);

  /**
   * @copydoc Dali::Ui::TextVisualizer::GetFontFamily
   */
  Dali::String GetFontFamily() const;

  /**
   * @copydoc Dali::Ui::TextVisualizer::SetFontSize
   */
  void SetFontSize(float fontSize);

  /**
   * @copydoc Dali::Ui::TextVisualizer::GetFontSize
   */
  float GetFontSize() const;

  /**
   * @copydoc Dali::Ui::TextVisualizer::SetTextColor
   */
  void SetTextColor(const UiColor& color);

  /**
   * @copydoc Dali::Ui::TextVisualizer::GetTextColor
   */
  UiColor GetTextColor();

  /**
   * @copydoc Dali::Ui::TextVisualizer::Prepare
   */
  void Prepare();

  /**
   * @copydoc Dali::Ui::TextVisualizer::SetExclusionRegions
   */
  void SetExclusionRegions(const Dali::Vector<Rect<float>>& regions);

  /**
   * @copydoc Dali::Ui::TextVisualizer::GetExclusionRegions
   */
  Dali::Vector<Rect<float>> GetExclusionRegions() const;

  /**
   * @copydoc Dali::Ui::TextVisualizer::ClearExclusionRegions
   */
  void ClearExclusionRegions();

public: // From ViewImpl
  /**
   * @copydoc ViewImpl::OnInitialize
   */
  void OnInitialize() override;

  /**
   * @copydoc ViewImpl::OnRelayout()
   */
  void OnRelayout(const Vector2& size, RelayoutContainer& container) override;

  /**
   * @copydoc ViewImpl::OnMeasure
   */
  MeasuredSize OnMeasure(float widthConstraint, float heightConstraint) override;

  /**
   * @copydoc ViewImpl::OnPropertySet
   */
  void OnPropertySet(Dali::Property::Index index, const Dali::Property::Value& propertyValue) override;

public: // Property helpers
  static void                  SetProperty(BaseObject* object, Dali::Property::Index index, const Dali::Property::Value& value);
  static Dali::Property::Value GetProperty(BaseObject* object, Dali::Property::Index index);

protected:
  /**
   * @brief TextVisualizerImpl constructor.
   */
  TextVisualizerImpl();

private:
  // Not copyable or movable
  TextVisualizerImpl(const TextVisualizerImpl&)            = delete;
  TextVisualizerImpl(TextVisualizerImpl&&)                 = delete;
  TextVisualizerImpl& operator=(const TextVisualizerImpl&) = delete;
  TextVisualizerImpl& operator=(TextVisualizerImpl&&)      = delete;

  struct PropertyHandler;

private:
  void MarkPrepareDirty();
  void MarkLayoutDirty();
  void MarkRenderDirty();
  void ClearPrepareDirty();
  void ClearLayoutDirty();
  bool AreExclusionRegionsEqual(const Dali::Vector<Rect<float>>& regions) const;
  void UpdateLayout(float layoutWidth, Internal::TextVisualizer::LayoutResult& result);

private:
  Dali::String                               mText;
  Dali::String                               mFontFamily;
  float                                      mFontSize;
  UiColor                                    mTextColor;
  Dali::Vector<Rect<float>>                  mExclusionRegions;
  Internal::TextVisualizer::PreparedText     mPreparedText;
  Internal::TextVisualizer::LayoutResult     mLayoutResult;
  Internal::TextVisualizer::AtlasViewAdapter mAtlasViewAdapter;
  Vector2                                    mLastLayoutSize;
  bool                                       mPrepareDirty;
  bool                                       mLayoutDirty;
  bool                                       mRenderDirty;
};

} // namespace Integration

} // namespace Ui

} // namespace Dali
