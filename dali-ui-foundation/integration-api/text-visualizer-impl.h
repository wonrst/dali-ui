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
#include <dali-ui-foundation/public-api/view-impl.h>

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
};

} // namespace Integration

} // namespace Ui

} // namespace Dali
