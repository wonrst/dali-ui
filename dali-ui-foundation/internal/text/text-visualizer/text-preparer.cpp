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

// INTERNAL INCLUDES
#include <dali-ui-foundation/internal/text/character-set-conversion.h>
#include <dali-ui-foundation/internal/text/text-visualizer/text-preparer.h>

namespace Dali::Ui::Internal::TextVisualizer
{

PreparedText TextPreparer::Prepare(const Input& input)
{
  PreparedText preparedText;

  preparedText.SetOriginalText(input.text);
  preparedText.SetFontFamily(input.fontFamily);
  preparedText.SetFontSize(input.fontSize);
  preparedText.SetPrepared(true);

  if(!input.text.Empty())
  {
    const auto* const utf8          = reinterpret_cast<const uint8_t*>(input.text.CStr());
    const uint32_t    utf8ByteCount = input.text.Size();

    // TODO: Connect shaping/fallback using existing text infrastructure.
    preparedText.SetClusterCount(Text::GetNumberOfUtf8Characters(utf8, utf8ByteCount));
  }

  return preparedText;
}

} // namespace Dali::Ui::Internal::TextVisualizer
