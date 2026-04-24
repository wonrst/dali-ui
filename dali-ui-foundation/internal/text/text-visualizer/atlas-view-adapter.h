#ifndef DALI_UI_TEXT_VISUALIZER_ATLAS_VIEW_ADAPTER_H
#define DALI_UI_TEXT_VISUALIZER_ATLAS_VIEW_ADAPTER_H

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
#include <dali-ui-foundation/internal/text/text-visualizer/layout-types.h>
#include <dali-ui-foundation/internal/text/text-visualizer/prepared-text.h>

namespace Dali::Ui::Internal::TextVisualizer
{
/**
 * @brief Non-owning adapter that exposes PreparedText/LayoutResult as render-ready glyph data.
 *
 * This intentionally does not implement Text::ViewInterface yet. It only groups the minimum
 * shaped glyph and placement data that a future AtlasRenderer bridge will need.
 *
 * @note LayoutResult currently stores temporary top-left glyph positions. Baseline and bearing
 * adjustment are deferred until renderer integration is implemented.
 */
class AtlasViewAdapter
{
public:
  AtlasViewAdapter();

  void SetPreparedText(const PreparedText* preparedText);
  void SetLayoutResult(const LayoutResult* layoutResult);
  void Clear();

  bool     HasRenderableGlyphs() const;
  bool     HasValidGlyphPlacementIndices() const;
  uint32_t GetGlyphCount() const;
  uint32_t GetGlyphPlacementCount() const;

  const Dali::Vector<Text::GlyphInfo>&      GetGlyphs() const;
  const Dali::Vector<GlyphPlacement>&       GetGlyphPlacements() const;
  const Dali::Vector<Text::GlyphIndex>&     GetNewParagraphGlyphs() const;
  const Dali::Vector<Text::CharacterIndex>& GetGlyphToCharacterMap() const;

private:
  const PreparedText* mPreparedText;
  const LayoutResult* mLayoutResult;
};

} // namespace Dali::Ui::Internal::TextVisualizer

#endif // DALI_UI_TEXT_VISUALIZER_ATLAS_VIEW_ADAPTER_H
