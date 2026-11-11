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

#include <dali-ui-foundation/integration-api/text/text-language.h>
#include <dali-ui-test-suite-utils.h>
#include <dali/devel-api/text-abstraction/shaping.h>
#include <string>
#include <vector>

using namespace Dali;
using namespace Dali::Ui;
namespace Language = Dali::Ui::Integration::Text;

int UtcDaliTextLanguageInputContractP(void)
{
  UiTestApplication application;
  std::string       result;
  for(const char* valid : {"", "und", "SR-cYRL-rs", "uk", "bg", "mk", "en-u-ca-gregory", "sl-rozaj-biske-1994", "x-local", "i-klingon", "es-419"})
  {
    DALI_TEST_CHECK(TextAbstraction::NormalizeTextLanguage(valid, result));
  }
  DALI_TEST_CHECK(TextAbstraction::NormalizeTextLanguage("SR-cYRL-rs", result));
  DALI_TEST_EQUALS(result, std::string("sr-cyrl-rs"), TEST_LOCATION);
  for(const auto& invalid : std::vector<std::string>{"sr_RS", "sr_RS.UTF-8", " ru", "ru ", "ru,sr", "ru--RU", "x", "en-u", "en-a-foo-A-bar", "sl-rozaj-rozaj", std::string("sr\0RS", 5u), std::string(256u, 'a')})
  {
    result = "preserved";
    DALI_TEST_CHECK(!TextAbstraction::NormalizeTextLanguage(invalid, result));
    DALI_TEST_EQUALS(result, std::string("preserved"), TEST_LOCATION);
  }
  END_TEST;
}

int UtcDaliTextLanguageControlContractP(void)
{
  UiTestApplication application;
  Label             label  = Label::New("бгдпт");
  InputField        field  = InputField::New();
  InputEditor       editor = InputEditor::New();
  DALI_TEST_CHECK(Language::GetTextLanguage(label).Empty());
  DALI_TEST_CHECK(Language::GetTextLanguage(field).Empty());
  DALI_TEST_CHECK(Language::GetTextLanguage(editor).Empty());
  DALI_TEST_CHECK(Language::SetTextLanguage(label, "SR-Cyrl-RS"));
  DALI_TEST_CHECK(Language::SetTextLanguage(field, "mk"));
  DALI_TEST_CHECK(Language::SetTextLanguage(editor, "und"));
  DALI_TEST_CHECK(!Language::SetTextLanguage(label, "sr_RS"));
  DALI_TEST_CHECK(!Language::SetTextLanguage(field, "mk,ru"));
  DALI_TEST_CHECK(!Language::SetTextLanguage(editor, " uk"));
  DALI_TEST_CHECK(Language::GetTextLanguage(label) == "sr-cyrl-rs");
  DALI_TEST_CHECK(Language::GetTextLanguage(field) == "mk");
  DALI_TEST_CHECK(Language::GetTextLanguage(editor) == "und");
  const Dali::String embeddedNull(Dali::StringView("sr\0RS", 5u));
  DALI_TEST_CHECK(!Language::SetTextLanguage(label, embeddedNull));
  DALI_TEST_CHECK(Language::GetTextLanguage(label) == "sr-cyrl-rs");
  DALI_TEST_CHECK(Language::SetTextLanguage(label, ""));
  DALI_TEST_CHECK(Language::SetTextLanguage(field, ""));
  DALI_TEST_CHECK(Language::SetTextLanguage(editor, ""));
  DALI_TEST_CHECK(Language::GetTextLanguage(label).Empty());
  DALI_TEST_CHECK(Language::GetTextLanguage(field).Empty());
  DALI_TEST_CHECK(Language::GetTextLanguage(editor).Empty());
  END_TEST;
}
