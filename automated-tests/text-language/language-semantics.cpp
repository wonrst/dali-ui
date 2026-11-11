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

#include <dali/devel-api/text-abstraction/font-client.h>
#include <dali/devel-api/text-abstraction/glyph-info.h>
#include <dali/devel-api/text-abstraction/shaping.h>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Dali::TextAbstraction;
std::vector<unsigned> Shape(FontClient& client, Shaping& shaper, FontId id, const std::string& language, const std::vector<Character>& text, Script script)
{
  auto                        count = shaper.Shape(client, text.data(), text.size(), id, script, language);
  std::vector<GlyphInfo>      glyphs(count);
  std::vector<CharacterIndex> map(count);
  std::vector<unsigned>       result;
  shaper.GetGlyphs(glyphs.data(), map.data());
  for(auto& glyph : glyphs) result.push_back(glyph.index);
  return result;
}
void Check(bool ok, const char* why)
{
  if(!ok) throw std::runtime_error(why);
}
void Print(const char* font, const char* system, const char* input, const std::vector<unsigned>& ids)
{
  std::cout << "CASE " << font << " system=" << system << " input=" << (input[0] ? input : "<empty>") << " gids=";
  for(auto id : ids) std::cout << id << ',';
  std::cout << '\n';
}
int main(int argc, char** argv)
try
{
  Check(argc == 2, "usage: language-semantics FONT_DIRECTORY");
  const std::vector<Character> cyrillic = {0x431, 0x433, 0x434, 0x43f, 0x442};
  const std::vector<Character> latin    = {'f', 'f', 'i'};
  for(const char* file : {"NotoSans.ttf", "NotoSans-Italic.ttf"})
  {
    auto client = FontClient::New(96, 96);
    auto id     = client.GetFontId(std::string(argv[1]) + "/" + file, 24 * 64);
    Check(id, "font load failed");
    auto shaper  = Shaping::New();
    auto neutral = Shape(client, shaper, id, "und", cyrillic, CYRILLIC);
    auto sr      = Shape(client, shaper, id, "sr", cyrillic, CYRILLIC);
    auto ru      = Shape(client, shaper, id, "ru", cyrillic, CYRILLIC);
    Check(sr != ru, "reference font lacks expected Serbian substitution");
    for(const char* language : {"ru", "bg", "und"}) Print(file, "g+breve-test", language, Shape(client, shaper, id, language, {0x433, 0x306}, CYRILLIC));
    for(const char* system : {"sr_RS.UTF-8", "ru_RU.UTF-8"})
    {
      SetLocale(system);
      for(const char* input : {"", "und", "ru", "sr"}) Print(file, system, input, Shape(client, shaper, id, input, cyrillic, CYRILLIC));
      Check(Shape(client, shaper, id, "und", cyrillic, CYRILLIC) == neutral, "und followed system locale");
      Check(Shape(client, shaper, id, "", cyrillic, CYRILLIC) == (system[0] == 's' ? sr : ru), "empty did not follow system locale");
      auto ligature = Shape(client, shaper, id, "und", latin, LATIN);
      Check(ligature.size() < latin.size(), "und disabled ordinary ligature shaping");
      Print(file, system, "und-ffi-test", ligature);
    }
  }
  std::cout << "PASS empty follows system, und is stable, language-specific locl works, und still shapes ffi\n";
}
catch(const std::exception& e)
{
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
