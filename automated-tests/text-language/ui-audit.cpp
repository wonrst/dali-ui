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

#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali-ui-foundation/integration-api/text/text-language.h>
#include <dali-ui-foundation/internal/text/async-text/async-text-loader.h>
#include <dali-ui-foundation/internal/text/controller/text-controller.h>
#include <dali/devel-api/adaptor-framework/application.h>
#include <dali/devel-api/text-abstraction/font-client.h>
#include <dali/integration-api/pixel-data-integ.h>
#include <dali/public-api/adaptor-framework/timer.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
using namespace Dali;
namespace U   = Dali::Ui;
namespace T   = Dali::Ui::Text;
namespace API = Dali::Ui::Integration::Text;
void Check(bool x, const char* why)
{
  if(!x) throw std::runtime_error(why);
}
std::vector<uint32_t> Glyphs(T::ControllerPtr controller)
{
  controller->Relayout(Size(1600, 200));
  auto                  model = controller->GetRenderTextModel();
  std::vector<uint32_t> ids;
  for(unsigned i = 0; i < model->GetNumberOfGlyphs(); ++i) ids.push_back(model->GetGlyphs()[i].index);
  return ids;
}
std::vector<uint8_t> Pixels(PixelData pixels)
{
  if(!pixels) return {};
  auto buffer = Dali::Integration::GetPixelDataBuffer(pixels);
  return {buffer.buffer, buffer.buffer + buffer.bufferSize};
}
struct Probe : ConnectionTracker
{
  Application app;
  int         status = 1;
  Timer       timer;
  Probe(Application a)
  : app(a)
  {
    app.InitSignal().Connect(this, &Probe::Init);
  }
  void Init(Application)
  {
    try
    {
      TextAbstraction::SetLocale("ru_RU.UTF-8");
      auto label  = U::Label::New("бгдпт");
      auto field  = U::InputField::New();
      auto editor = U::InputEditor::New();
      Check(API::GetTextLanguage(label).Empty(), "initial label language");
      Check(API::SetTextLanguage(label, "SR-cYRL-rs"), "valid label tag");
      Check(API::GetTextLanguage(label) == "sr-cyrl-rs", "normalization");
      Check(!API::SetTextLanguage(label, "sr_RS.UTF-8"), "POSIX accepted");
      Check(API::GetTextLanguage(label) == "sr-cyrl-rs", "invalid changed value");
      Check(API::SetTextLanguage(field, "mk") && API::GetTextLanguage(field) == "mk", "field api");
      Check(API::SetTextLanguage(editor, "bg") && API::GetTextLanguage(editor) == "bg", "editor api");
      Check(!API::SetTextLanguage(field, "ru,sr") && API::GetTextLanguage(field) == "mk", "field invalid");
      Check(!API::SetTextLanguage(editor, " sr") && API::GetTextLanguage(editor) == "bg", "editor invalid");
      Check(API::SetTextLanguage(label, "") && API::GetTextLanguage(label).Empty(), "reset");
      auto c = T::Controller::New();
      c->SetDefaultFontFamily("Noto Sans");
      c->SetDefaultFontSize(24, T::Controller::POINT_SIZE);
      c->SetText("бгдпт бгдпт бгдпт");
      auto sys    = Glyphs(c);
      auto ruSize = c->GetNaturalSize();
      Check(c->SetTextLanguage("sr"), "controller sr");
      auto sr     = Glyphs(c);
      auto srSize = c->GetNaturalSize();
      Check(sys != sr, "language-only update did not reshape");
      for(const auto* locale : {"sr.UTF-8", "sr@latin", "sr_RS.UTF-8"})
      {
        TextAbstraction::SetLocale(locale);
        c->SetTextLanguage("");
        Check(c->GetResolvedTextLanguage() == "sr", "legacy system locale lost its language");
        Check(Glyphs(c) == sr, "legacy system locale changed shaping");
      }
      TextAbstraction::SetLocale("ru_RU.UTF-8");
      for(int i = 0; i < 100; ++i)
      {
        c->SetTextLanguage("ru");
        Check(Glyphs(c) == sys, "ru glyph cache contamination");
        Check(c->GetNaturalSize() == ruSize, "ru natural size stale");
        c->SetTextLanguage("sr");
        Check(Glyphs(c) == sr, "sr glyph cache contamination");
        Check(c->GetNaturalSize() == srSize, "sr natural size stale");
      }
      c->SetTextLanguage("");
      Check(Glyphs(c) == sys, "controller reset did not reshape");
      c->SetTextLanguage("sr");
      TextAbstraction::SetLocale("bg_BG.UTF-8");
      Check(c->GetResolvedTextLanguage() == "sr", "explicit language followed global");
      c->SetTextLanguage("");
      Check(c->GetResolvedTextLanguage() == "bg", "reset does not follow global");
      auto                                        loader = T::AsyncTextLoader::New();
      std::map<std::string, std::vector<uint8_t>> baseline;
      std::map<std::string, Size>                 sizes;
      for(const auto* lang : {"ru", "sr", "mk", "bg", "und"})
      {
        T::AsyncTextParameters p;
        p.text           = "бгдпт бгдпт бгдпт";
        p.fontFamily     = "Noto Sans";
        p.fontSize       = 24;
        p.textLanguage   = lang;
        p.textWidth      = 1000;
        p.textHeight     = 120;
        p.maxTextureSize = 4096;
        auto cold        = T::AsyncTextLoader::New();
        auto n           = cold.GetNaturalSize(p);
        sizes[lang]      = n.renderedSize;
        auto rendered    = cold.RenderText(p, false, Size::ZERO);
        baseline[lang]   = Pixels(rendered.textPixelData);
        Check(!baseline[lang].empty(), "async empty bitmap");
      }
      Check(baseline["ru"] != baseline["sr"], "async language ignored");
      for(int i = 0; i < 40; ++i)
        for(const auto* lang : {"sr", "bg", "ru", "mk", "und"})
        {
          T::AsyncTextParameters p;
          p.text           = "бгдпт бгдпт бгдпт";
          p.fontFamily     = "Noto Sans";
          p.fontSize       = 24;
          p.textLanguage   = lang;
          p.textWidth      = 1000;
          p.textHeight     = 120;
          p.maxTextureSize = 4096;
          auto n           = loader.GetNaturalSize(p);
          Check(n.renderedSize == sizes[lang], "async natural size stale");
          auto rendered = loader.RenderText(p, false, Size::ZERO);
          Check(Pixels(rendered.textPixelData) == baseline[lang], "async loader cache contamination");
        }
      std::cout << "PASS UI APIs, invalid input, reset, 200 controller transitions, 200 async loader transitions + pixel equality\n";
      status = 0;
    }
    catch(const std::exception& e)
    {
      std::cerr << "FAIL " << e.what() << "\n";
    }
    timer = Timer::New(20);
    timer.TickSignal().Connect(this, &Probe::Quit);
    timer.Start();
  }
  bool Quit()
  {
    app.Quit();
    return false;
  }
};
int main(int argc, char** argv)
{
  auto app = Application::New(&argc, &argv);
  U::UiConfig::New().Apply();
  Probe probe(app);
  app.MainLoop();
  return probe.status;
}
