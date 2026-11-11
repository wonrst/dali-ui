#ifndef DALI_UI_INTEGRATION_TEXT_LANGUAGE_H
#define DALI_UI_INTEGRATION_TEXT_LANGUAGE_H

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

#include <dali-ui-foundation/public-api/views/text-controls/input-editor.h>
#include <dali-ui-foundation/public-api/views/text-controls/input-field.h>
#include <dali-ui-foundation/public-api/views/text-controls/label.h>

namespace DALI_NAMESPACE::Ui::Integration::Text
{
/**
 * @brief Sets the language used to shape the entire control, including placeholder text.
 *
 * Accepts one well-formed ASCII BCP 47 language tag of at most 255 bytes.
 * Tags are case-insensitive and normalized to lowercase. Registry membership
 * and deprecated aliases are not resolved. Extensions and private-use subtags
 * are retained. Underscores, POSIX locale names, whitespace, encoding suffixes,
 * embedded NUL characters and language lists are rejected. Invalid input
 * leaves the previous setting unchanged.
 *
 * Empty removes the control's shaping-language override. This is the default
 * and currently follows the system shaping language. "und" explicitly requests
 * undetermined language and does not follow the system language.
 *
 * This integration API selects localized forms supplied by the chosen font.
 * It does not select fallback fonts, translate text, select an input method,
 * override Unicode script/direction, or change locale-specific line-breaking.
 * It applies to sync rendering, async rendering and size measurement. Font
 * support determines whether two languages produce different glyphs.
 *
 * This API does not set a View's content language or propagate to child controls.
 * There is currently no ancestor inheritance.
 *
 * @param[in] control The text control to update.
 * @param[in] language The language tag, or empty to remove the override.
 * @return True on success, false on invalid input.
 * @pre The control must be initialized.
 * @note This function must be called on the event thread.
 */
DALI_UI_API bool SetTextLanguage(Ui::Label control, const Dali::String& language);

/**
 * @copydoc SetTextLanguage(Ui::Label, const Dali::String&)
 */
DALI_UI_API bool SetTextLanguage(Ui::InputField control, const Dali::String& language);

/**
 * @copydoc SetTextLanguage(Ui::Label, const Dali::String&)
 */
DALI_UI_API bool SetTextLanguage(Ui::InputEditor control, const Dali::String& language);

/**
 * @brief Gets the authored text shaping language.
 *
 * Returns the normalized tag set on the control. The resolved system language
 * is not returned when the control has no override.
 *
 * @param[in] control The text control to query.
 * @return The lowercase language tag, or empty if no override is set.
 * @pre The control must be initialized.
 * @note This function must be called on the event thread.
 */
DALI_UI_API Dali::String GetTextLanguage(Ui::Label control);

/**
 * @copydoc GetTextLanguage(Ui::Label)
 */
DALI_UI_API Dali::String GetTextLanguage(Ui::InputField control);

/**
 * @copydoc GetTextLanguage(Ui::Label)
 */
DALI_UI_API Dali::String GetTextLanguage(Ui::InputEditor control);

} // namespace DALI_NAMESPACE::Ui::Integration::Text

#endif // DALI_UI_INTEGRATION_TEXT_LANGUAGE_H
