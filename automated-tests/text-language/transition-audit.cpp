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
#include <dali-ui-foundation/integration-api/label-impl.h>
#include <dali-ui-foundation/integration-api/text/text-language.h>
#include <dali/devel-api/adaptor-framework/application.h>
#include <dali/public-api/adaptor-framework/timer.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
// X11 defines Status; include it after the DALi headers.
#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace Dali;
using namespace Dali::Ui;
namespace API      = Dali::Ui::Integration::Text;
const char* tags[] = {"ru", "sr", "mk", "bg", "und", "sr", "", "sr-Cyrl-RS"};
struct Probe : ConnectionTracker
{
  Application    app;
  AbsoluteLayout root;
  View           marker;
  const unsigned colors[8] = {0x000000, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0xff00ff, 0x00ffff, 0xffffff};
  Label          target, reference;
  InputField     field, fieldRef;
  InputEditor    editor, editorRef;
  Timer          timer;
  unsigned       stage = 0, ticks = 0, completed = 0, total = 0;
  bool           async = false, placeholder = false;
  std::string    out;
  int            status = 1;
  Probe(Application a)
  : app(a)
  {
    app.InitSignal().Connect(this, &Probe::Init);
  }
  template<class V>
  void Setup(V view, float x, float y, float height)
  {
    view.SetFontFamily("Noto Sans");
    view.SetFontSize(36);
    view.SetFontSlant(Text::FontSlant::ITALIC);
    view.SetText("бгдпт бгдпт");
    view.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(x, y, 600, height)));
    root.Add(view);
  }
  void Init(Application)
  {
    app.GetWindow().SetPositionSize(PositionSize(0, 0, 1280, 500));
    app.GetWindow().SetBackgroundColor(Color::WHITE);
    root = AbsoluteLayout::New();
    root.SetRequestedWidth(MATCH_PARENT);
    root.SetRequestedHeight(MATCH_PARENT);
    app.GetWindow().Add(root);
    async       = std::getenv("TEXT_LANGUAGE_ASYNC");
    placeholder = std::getenv("TEXT_LANGUAGE_PLACEHOLDER");
    out         = std::getenv("TEXT_LANGUAGE_CAPTURE_DIR");
    target      = Label::New();
    Setup(target, 20, 30, 100);
    target.SetAsyncRendering(async);
    target.AsyncRenderFinishedSignal().Connect(this, &Probe::Finished);
    field = InputField::New();
    Setup(field, 20, 150, 80);
    editor = InputEditor::New();
    Setup(editor, 20, 270, 150);
    marker = View::New();
    marker.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(20, 450, 30, 30)));
    root.Add(marker);
    Next();
    timer = Timer::New(100);
    timer.TickSignal().Connect(this, &Probe::Tick);
    timer.Start();
  }
  void Finished(View, float, float)
  {
    ++completed;
    ticks = 0;
  }
  void Next()
  {
    completed = 0;
    ticks     = 0;
    if(reference) reference.Unparent();
    if(fieldRef) fieldRef.Unparent();
    if(editorRef) editorRef.Unparent();
    reference = Label::New();
    Setup(reference, 660, 30, 100);
    reference.SetAsyncRendering(async);
    reference.AsyncRenderFinishedSignal().Connect(this, &Probe::Finished);
    fieldRef = InputField::New();
    Setup(fieldRef, 660, 150, 80);
    editorRef = InputEditor::New();
    Setup(editorRef, 660, 270, 150);
    // Submit obsolete requests before changing the authored language again.
    if(stage)
    {
      for(const char* language : {"mk", "uk"})
      {
        API::SetTextLanguage(target, language);
        if(async) static_cast<Dali::Ui::Integration::LabelImpl&>(target.GetImplementation()).RequestAsyncRenderWithFixedSize(600, 100);
      }
    }
    if(placeholder)
    {
      for(auto c : {field, fieldRef})
      {
        c.SetPlaceholder("бгдпт бгдпт");
        c.SetText("");
      }
      for(auto c : {editor, editorRef})
      {
        c.SetPlaceholder("бгдпт бгдпт");
        c.SetText("");
      }
    }
    for(auto label : {target, reference}) API::SetTextLanguage(label, tags[stage]);
    for(auto control : {field, fieldRef}) API::SetTextLanguage(control, tags[stage]);
    for(auto control : {editor, editorRef}) API::SetTextLanguage(control, tags[stage]);
    if(!API::SetTextLanguage(target, "sr_RS") && API::GetTextLanguage(target) == API::GetTextLanguage(reference))
    {
    }
    else
      std::exit(7);
    marker.SetBackgroundColor(UiColor(colors[stage]));
    app.GetWindow().KeepRendering(1.2f);
  }
  bool Tick()
  {
    if(++total > 1000) std::exit(8);
    if(++ticks < 7 || (async && completed < 2)) return true;
    auto display = XOpenDisplay(nullptr);
    auto im      = XGetImage(display, static_cast<::Window>(app.GetWindow().GetNativeId()), 0, 0, 1280, 500, AllPlanes, ZPixmap);
    if(!im) std::exit(9);
    // A frame marker proves that X11 has presented this stage, even under load.
    if((XGetPixel(im, 35, 465) & 0xffffff) != colors[stage])
    {
      XDestroyImage(im);
      XCloseDisplay(display);
      app.GetWindow().KeepRendering(0.5f);
      return true;
    }
    size_t                     differences = 0, ink = 0;
    std::vector<unsigned char> pixels;
    for(int y = 30; y < 420; ++y)
      for(int x = 20; x < 620; ++x)
      {
        auto a = XGetPixel(im, x, y), b = XGetPixel(im, x + 640, y);
        differences += (a != b);
        ink += ((a & 0xffffff) != 0xffffff);
        pixels.push_back(a >> 16);
        pixels.push_back(a >> 8);
        pixels.push_back(a);
      }
    XDestroyImage(im);
    XCloseDisplay(display);
    std::ofstream file(out + "/" + std::to_string(stage) + ".ppm", std::ios::binary);
    file << "P6\n600 390\n255\n";
    file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    std::printf("TRANSITION %u tag=%s async=%d differences=%zu ink=%zu\n", stage, tags[stage], async, differences, ink);
    std::fflush(stdout);
    if(differences || ink < 500)
    {
      std::fprintf(stderr, "FAIL stale language or empty render\n");
      app.Quit();
      return false;
    }
    if(++stage == sizeof(tags) / sizeof(*tags))
    {
      status = 0;
      app.Quit();
      return false;
    }
    Next();
    return true;
  }
};
int main(int argc, char** argv)
{
  auto app = Application::New(&argc, &argv);
  UiConfig::New().Apply();
  Probe p(app);
  app.MainLoop();
  return p.status;
}
