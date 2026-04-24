#ifndef DALI_UI_TEXT_VISUALIZER_PREPARED_TEXT_H
#define DALI_UI_TEXT_VISUALIZER_PREPARED_TEXT_H

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
#include <cstdint>

// INTERNAL INCLUDES
#include <dali/public-api/common/dali-string.h>

namespace Dali::Ui::Internal::TextVisualizer
{
/**
 * @brief Minimal prepared text cache for TextVisualizer.
 *
 * This skeleton stores only stable input state and a minimal count field.
 * Actual shaping, fallback and cluster mapping will be connected later.
 */
class PreparedText
{
public:
  PreparedText();

  void                SetOriginalText(const Dali::String& text);
  const Dali::String& GetOriginalText() const;

  void                SetFontFamily(const Dali::String& fontFamily);
  const Dali::String& GetFontFamily() const;

  void  SetFontSize(float fontSize);
  float GetFontSize() const;

  void     SetClusterCount(uint32_t clusterCount);
  uint32_t GetClusterCount() const;

  void SetPrepared(bool prepared);
  bool IsPrepared() const;

  bool Empty() const;
  void Clear();

private:
  Dali::String mOriginalText;
  Dali::String mFontFamily;
  float        mFontSize;
  uint32_t     mClusterCount;
  bool         mPrepared;
};

} // namespace Dali::Ui::Internal::TextVisualizer

#endif // DALI_UI_TEXT_VISUALIZER_PREPARED_TEXT_H
