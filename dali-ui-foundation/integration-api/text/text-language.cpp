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

#include <dali-ui-foundation/integration-api/input-editor-impl.h>
#include <dali-ui-foundation/integration-api/input-field-impl.h>
#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/integration-api/text/text-language.h>

namespace DALI_NAMESPACE::Ui::Integration::Text
{
bool SetTextLanguage(Ui::Label control, const Dali::String& language)
{
  DALI_ASSERT_ALWAYS(control);
  return static_cast<Integration::LabelImpl&>(control.GetImplementation()).SetTextLanguage(language);
}

Dali::String GetTextLanguage(Ui::Label control)
{
  DALI_ASSERT_ALWAYS(control);
  return static_cast<const Integration::LabelImpl&>(control.GetImplementation()).GetTextLanguage();
}

bool SetTextLanguage(Ui::InputField control, const Dali::String& language)
{
  DALI_ASSERT_ALWAYS(control);
  return static_cast<Integration::InputFieldImpl&>(control.GetImplementation()).SetTextLanguage(language);
}

Dali::String GetTextLanguage(Ui::InputField control)
{
  DALI_ASSERT_ALWAYS(control);
  return static_cast<const Integration::InputFieldImpl&>(control.GetImplementation()).GetTextLanguage();
}

bool SetTextLanguage(Ui::InputEditor control, const Dali::String& language)
{
  DALI_ASSERT_ALWAYS(control);
  return static_cast<Integration::InputEditorImpl&>(control.GetImplementation()).SetTextLanguage(language);
}

Dali::String GetTextLanguage(Ui::InputEditor control)
{
  DALI_ASSERT_ALWAYS(control);
  return static_cast<const Integration::InputEditorImpl&>(control.GetImplementation()).GetTextLanguage();
}

} // namespace DALI_NAMESPACE::Ui::Integration::Text
