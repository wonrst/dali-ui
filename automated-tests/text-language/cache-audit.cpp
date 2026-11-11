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
#include <dali/devel-api/text-abstraction/glyph-buffer-data.h>
#include <dali/devel-api/text-abstraction/glyph-info.h>
#include <dali/devel-api/text-abstraction/shaping.h>
#include <harfbuzz/hb-ot.h>
#include <harfbuzz/hb.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace Dali;
using namespace Dali::TextAbstraction;
using TextRun = std::vector<Character>;
struct Snapshot
{
  std::vector<GlyphInfo>      glyphs;
  std::vector<CharacterIndex> clusters;
  std::vector<uint64_t>       pixels;
};
void Require(bool ok, const std::string& msg)
{
  if(!ok) throw std::runtime_error(msg);
}
uint64_t Hash(const GlyphBufferData& data)
{
  std::vector<uint8_t> pixels(data.width * data.height * Pixel::GetBytesPerPixel(data.format));
  if(!pixels.empty())
  {
    Require(data.buffer != nullptr, "missing bitmap");
    GlyphBufferData::Decompress(data, pixels.data());
  }
  uint64_t hash = 1469598103934665603ull;
  for(auto v : {data.width, data.height, static_cast<uint32_t>(data.format)})
  {
    hash ^= v;
    hash *= 1099511628211ull;
  }
  for(auto v : pixels)
  {
    hash ^= v;
    hash *= 1099511628211ull;
  }
  return hash;
}
Snapshot Shape(FontClient& client, Shaping& shaper, FontId id, const TextRun& text, const std::string& lang)
{
  auto     n = shaper.Shape(client, text.data(), text.size(), id, CYRILLIC, lang);
  Snapshot result;
  result.glyphs.resize(n);
  result.clusters.resize(n);
  shaper.GetGlyphs(result.glyphs.data(), result.clusters.data());
  for(auto& glyph : result.glyphs) glyph.isShaped = true;
  Require(client.GetGlyphMetrics(result.glyphs.data(), n, BITMAP_GLYPH), "glyph metrics failed");
  for(auto& glyph : result.glyphs)
  {
    GlyphBufferData buffer;
    client.CreateBitmap(id, glyph.index, false, false, buffer, 0);
    result.pixels.push_back(Hash(buffer));
  }
  return result;
}
bool Equal(const Snapshot& a, const Snapshot& b)
{
  if(a.clusters != b.clusters || a.pixels != b.pixels || a.glyphs.size() != b.glyphs.size()) return false;
  for(size_t j = 0; j < a.glyphs.size(); ++j)
  {
    auto& x = a.glyphs[j];
    auto& y = b.glyphs[j];
    if(x.index != y.index || x.advance != y.advance || x.xBearing != y.xBearing || x.yBearing != y.yBearing || x.width != y.width || x.height != y.height) return false;
  }
  return true;
}
void Reference(hb_font_t* font, const TextRun& text, const std::string& lang, const Snapshot& result)
{
  auto b = hb_buffer_create();
  hb_buffer_set_direction(b, HB_DIRECTION_LTR);
  hb_buffer_set_script(b, HB_SCRIPT_CYRILLIC);
  hb_buffer_set_language(b, hb_language_from_string(lang.c_str(), -1));
  hb_buffer_add_utf32(b, text.data(), text.size(), 0, text.size());
  hb_shape(font, b, nullptr, 0);
  unsigned n;
  auto     info = hb_buffer_get_glyph_infos(b, &n);
  Require(n == result.glyphs.size(), "reference glyph count");
  for(unsigned j = 0; j < n; ++j) Require(info[j].codepoint == result.glyphs[j].index && info[j].cluster == result.clusters[j], "reference glyph/cluster mismatch");
  hb_buffer_destroy(b);
}
int main(int argc, char** argv)
try
{
  Require(argc == 3, "usage: cache-audit FONT_DIRECTORY CORPUS_FILE");
  const std::string    fonts = argv[1];
  std::ifstream        file(argv[2]);
  std::string          line;
  std::vector<TextRun> cases;
  while(std::getline(file, line))
  {
    std::istringstream in(line);
    TextRun            text;
    unsigned           cp;
    while(in >> std::hex >> cp) text.push_back(cp);
    if(!text.empty()) cases.push_back(text);
  }
  Require(cases.size() > 1000, "corpus missing");
  const std::vector<std::string> languages  = {"und", "ru", "uk", "be", "bg", "sr", "mk", "sr-Cyrl-RS", "sr-Latn", "kk", "ky", "mn", "tt", "ba", "cv", "tg", "uz-Cyrl", "az-Cyrl", "bs-Cyrl", "os", "sah", "ab", "ce", "cu", "en", "x-test"};
  std::string                    normalized = "unchanged";
  for(const auto* tag : {"sr", "SR-cYRL-rs", "und", "x-custom", "en-u-ca-gregory", "sl-rozaj-biske-1994", "i-klingon", "zh-cmn-Hans-CN", "es-419", "en-a-abc-x-a"}) Require(NormalizeTextLanguage(tag, normalized), std::string("valid tag rejected: ") + tag);
  for(const auto& tag : std::vector<std::string>{"sr_RS", "sr_RS.UTF-8", " sr", "sr ", "ru,sr", "sr--RS", "s", "en-u", "x", "en-a-abc-A-def", "sl-rozaj-rozaj", std::string("sr\0RS", 5), std::string(256, 'a')})
  {
    normalized = "unchanged";
    Require(!NormalizeTextLanguage(tag, normalized) && normalized == "unchanged", "invalid input accepted");
  }
  Require(NormalizeTextLanguage("", normalized) && normalized.empty(), "reset rejected");
  size_t comparisons = 0, referenceChecks = 0, glyphChecks = 0;
  for(const auto* name : {"NotoSans.ttf", "NotoSans-Italic.ttf"})
    for(int points : {12, 24, 48})
    {
      auto blob = hb_blob_create_from_file((fonts + "/" + name).c_str());
      auto face = hb_face_create(blob, 0);
      auto font = hb_font_create(face);
      hb_ot_font_set_funcs(font);
      std::vector<std::vector<Snapshot>> baseline;
      for(const auto& lang : languages)
      {
        auto client = FontClient::New(96, 96);
        auto id     = client.GetFontId(fonts + "/" + name, points * 64);
        Require(id != 0, "font not loaded");
        auto shaper = Shaping::New();
        baseline.emplace_back();
        for(const auto& text : cases)
        {
          auto result = Shape(client, shaper, id, text, lang);
          Reference(font, text, lang, result);
          ++referenceChecks;
          baseline.back().push_back(std::move(result));
        }
      }
      auto                client = FontClient::New(96, 96);
      auto                id     = client.GetFontId(fonts + "/" + name, points * 64);
      auto                shaper = Shaping::New();
      std::vector<size_t> order(cases.size());
      std::iota(order.begin(), order.end(), 0);
      std::mt19937 random(20261110);
      for(unsigned pass = 0; pass < 3; ++pass)
      {
        std::shuffle(order.begin(), order.end(), random);
        for(auto index : order)
          for(size_t l = 0; l < languages.size(); ++l)
          {
            size_t langIndex = (l + index + pass) % languages.size();
            auto   result    = Shape(client, shaper, id, cases[index], languages[langIndex]);
            Require(Equal(result, baseline[langIndex][index]), std::string("warm cache mismatch: ") + name + " " + languages[langIndex] + " case=" + std::to_string(index));
            ++comparisons;
            glyphChecks += result.glyphs.size();
          }
      }
      // A rejected new-API call must not leave the previous result available.
      auto count = shaper.Shape(client, cases[0].data(), cases[0].size(), id, CYRILLIC, "ru_RU");
      Require(count == 0, "invalid shape accepted");
      GlyphInfo sentinel;
      sentinel.index     = 123456;
      CharacterIndex map = 765432;
      shaper.GetGlyphs(&sentinel, &map);
      Require(sentinel.index == 123456 && map == 765432, "stale result after rejection");
      std::cout << "PASS " << name << " " << points << "pt " << languages.size() << " languages " << cases.size() << " cases x3 warm permutations\n"
                << std::flush;
      hb_font_destroy(font);
      hb_face_destroy(face);
      hb_blob_destroy(blob);
    }
  std::cout << "RESULT reference_checks=" << referenceChecks << " warm_comparisons=" << comparisons << " glyph_bitmap_checks=" << glyphChecks << " failures=0\n";
}
catch(const std::exception& e)
{
  std::cerr << "FAIL " << e.what() << "\n";
  return 1;
}
