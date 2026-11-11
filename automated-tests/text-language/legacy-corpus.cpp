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
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#ifdef TEXT_LANGUAGE_BASELINE_SHAPING_HEADER
#include TEXT_LANGUAGE_BASELINE_SHAPING_HEADER
#else
#include <dali/devel-api/text-abstraction/shaping.h>
#endif
using namespace Dali::TextAbstraction;
int main(int argc, char** argv)
{
  if(argc != 3)
  {
    std::cerr << "Usage: legacy-corpus FONT_FILE CORPUS_FILE\n";
    return 1;
  }
  std::vector<std::vector<Character>> cases;
  std::ifstream                       input(argv[2]);
  if(!input)
  {
    std::cerr << "Cannot open corpus: " << argv[2] << '\n';
    return 1;
  }
  std::string line;
  while(std::getline(input, line))
  {
    std::istringstream     in(line);
    unsigned               cp;
    std::vector<Character> text;
    while(in >> std::hex >> cp) text.push_back(cp);
    cases.push_back(text);
  }
  uint64_t digest = 1469598103934665603ull;
  auto     hash   = [&](uint32_t value)
  {digest^=value;digest*=1099511628211ull; };
  auto bits = [](float v)
  {uint32_t n;std::memcpy(&n,&v,4);return n; };
  auto client  = FontClient::New(96, 96);
  auto id      = client.GetFontId(argv[1], 24 * 64);
  auto shaping = Shaping::New();
  if(!id || cases.empty())
  {
    std::cerr << "Font or corpus is empty\n";
    return 1;
  }
#ifdef TEXT_LANGUAGE_BASELINE_SHAPING_HEADER
  // This source is deliberately compiled with the old header for the ABI check.
  auto legacyMember = &Shaping::Shape;
#else
  // The updated header exposes two overloads; select the old signature explicitly.
  using LegacyShape        = Length (Shaping::*)(FontClient&, const Character*, Length, FontId, Script);
  LegacyShape legacyMember = &Shaping::Shape;
#endif
  for(const auto* locale : {"ru_RU.UTF-8", "uk_UA.UTF-8", "sr_RS.UTF-8", "mk_MK.UTF-8", "bg_BG.UTF-8", "C", ""})
  {
    SetLocale(locale);
    for(auto& text : cases)
    {
      auto n = (shaping.*legacyMember)(client, text.data(), text.size(), id, CYRILLIC);
      hash(n);
      std::vector<GlyphInfo>      glyphs(n);
      std::vector<CharacterIndex> map(n);
      shaping.GetGlyphs(glyphs.data(), map.data());
      for(unsigned i = 0; i < n; ++i)
      {
        auto& g = glyphs[i];
        hash(g.index);
        hash(map[i]);
        hash(bits(g.advance));
        hash(bits(g.xBearing));
        hash(bits(g.yBearing));
      }
#ifndef TEXT_LANGUAGE_BASELINE_SHAPING_HEADER
      // Empty language must produce the same data as the original overload.
      auto explicitCount = shaping.Shape(client, text.data(), text.size(), id, CYRILLIC, "");
      if(explicitCount != n)
      {
        return 2;
      }
      std::vector<GlyphInfo>      explicitGlyphs(n);
      std::vector<CharacterIndex> explicitMap(n);
      shaping.GetGlyphs(explicitGlyphs.data(), explicitMap.data());
      for(unsigned i = 0; i < n; ++i)
      {
        const auto& a = glyphs[i];
        const auto& b = explicitGlyphs[i];
        if(a.index != b.index || map[i] != explicitMap[i] ||
           bits(a.advance) != bits(b.advance) || bits(a.xBearing) != bits(b.xBearing) ||
           bits(a.yBearing) != bits(b.yBearing))
        {
          return 3;
        }
      }
#endif
    }
  }
  std::cout << "LEGACY " << cases.size() << " " << digest << " sizeof=" << sizeof(Shaping) << "\n";
}
