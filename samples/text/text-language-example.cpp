/* Copyright (c) 2026 Samsung Electronics Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0
 */
#include <dali-ui-foundation/dali-ui-foundation.h>
#include <dali-ui-foundation/integration-api/text/text-language.h>
#include <dali/devel-api/adaptor-framework/application.h>
#include <dali/devel-api/text-abstraction/font-client.h>
#include <dali/public-api/adaptor-framework/timer.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#ifdef TEXT_LANGUAGE_CAPTURE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
using namespace Dali;
using namespace Dali::Ui;
namespace Language = Dali::Ui::Integration::Text;
namespace
{
const char*      LANGUAGES[] = {"ru", "uk", "bg", "sr", "mk", "und"};
constexpr size_t PAGE_SIZE   = 24;
std::string      Utf8(uint32_t cp)
{
  std::string s;
  if(cp < 0x80)
    s += char(cp);
  else if(cp < 0x800)
  {
    s += char(0xc0 | (cp >> 6));
    s += char(0x80 | (cp & 63));
  }
  else if(cp < 0x10000)
  {
    s += char(0xe0 | (cp >> 12));
    s += char(0x80 | ((cp >> 6) & 63));
    s += char(0x80 | (cp & 63));
  }
  else
  {
    s += char(0xf0 | (cp >> 18));
    s += char(0x80 | ((cp >> 12) & 63));
    s += char(0x80 | ((cp >> 6) & 63));
    s += char(0x80 | (cp & 63));
  }
  return s;
}
} //namespace
class LanguageSample : public ConnectionTracker
{
public:
  explicit LanguageSample(Application app)
  : mApp(app)
  {
    mApp.InitSignal().Connect(this, &LanguageSample::Init);
  }

private:
  void Init(Application app)
  {
    // Register fonts before creating controls. The companion launcher isolates
    // Fontconfig as well, so an installed font with the same family cannot win.
    if(const char* family = std::getenv("TEXT_LANGUAGE_FONT_FAMILY")) mFontFamily = family;
    const char*       customDirectory = std::getenv("TEXT_LANGUAGE_FONT_DIR");
    const std::string fontDirectory   = customDirectory ? customDirectory : std::string(RESOURCES_DIR) + "text-language/fonts";
    if(!TextAbstraction::FontClient::Get().AddCustomFontDirectory(fontDirectory))
    {
      std::fprintf(stderr, "Could not register font directory: %s\n", fontDirectory.c_str());
    }
    auto window = app.GetWindow();
    window.SetPositionSize(PositionSize(0, 0, 1280, 900));
    window.SetBackgroundColor(Color::WHITE);
    window.KeyEventSignal().Connect(this, &LanguageSample::Key);
    mRoot = AbsoluteLayout::New();
    mRoot.SetRequestedWidth(MATCH_PARENT);
    mRoot.SetRequestedHeight(MATCH_PARENT);
    window.Add(mRoot);
    mTitle = Label::New();
    mTitle.SetFontFamily(mFontFamily.c_str());
    mTitle.SetFontSize(23);
    mTitle.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(20, 10, 1240, 80)));
    mTitle.SetMultiLine(true);
    mRoot.Add(mTitle);
    std::ifstream codes(std::string(RESOURCES_DIR) + "text-language/cyrillic-codepoints.txt");
    uint32_t      cp;
    while(codes >> std::hex >> cp) mCodepoints.push_back(cp);
    if(mCodepoints.empty())
    {
      std::fprintf(stderr, "Missing corpus resources\n");
      std::exit(2);
    }
    mAsync  = std::getenv("TEXT_LANGUAGE_ASYNC");
    mItalic = std::getenv("TEXT_LANGUAGE_ITALIC");
    mBold   = std::getenv("TEXT_LANGUAGE_BOLD");
    for(size_t i = 0; i < 6; ++i)
    {
      auto title = Label::New(LANGUAGES[i]);
      title.SetFontFamily(mFontFamily.c_str());
      title.SetFontSize(24);
      title.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(20, 110 + i * 90, 70, 80)));
      mRoot.Add(title);
      mLanguageLabels.push_back(title);
      auto label = Label::New();
      label.SetUiScalePolicy(UiScalePolicy::DISABLED);
      label.SetFontFamily(mFontFamily.c_str());
      label.SetFontSize(30);
      label.SetTextColor(UiColor(0x101010));
      label.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(100, 110 + i * 90, 1160, 80)));
      label.SetMultiLine(true);
      label.SetAsyncRendering(mAsync);
      Language::SetTextLanguage(label, LANGUAGES[i]);
      label.AsyncRenderFinishedSignal().Connect(this, &LanguageSample::Finished);
      mRoot.Add(label);
      mLabels.push_back(label);
    }
    mField = InputField::New();
    mField.SetFontFamily(mFontFamily.c_str());
    mField.SetFontSize(30);
    mField.SetText("бгдпт — InputField");
    mField.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(100, 670, 1160, 70)));
    Language::SetTextLanguage(mField, "sr");
    mRoot.Add(mField);
    mEditor = InputEditor::New();
    mEditor.SetFontFamily(mFontFamily.c_str());
    mEditor.SetFontSize(30);
    mEditor.SetText("бгдпт — InputEditor\nА́ ё й");
    mEditor.SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(100, 750, 1160, 130)));
    Language::SetTextLanguage(mEditor, "ru");
    mRoot.Add(mEditor);
#ifdef TEXT_LANGUAGE_CAPTURE
    if(std::getenv("TEXT_LANGUAGE_CAPTURE_DIR"))
    {
      for(unsigned bit = 0; bit < 5; ++bit)
      {
        mFrameMarkers[bit] = View::New();
        mFrameMarkers[bit].SetLayoutParams(AbsoluteLayoutParams::New().SetBounds(LayoutRect(1220 + bit * 10, 880, 10, 10)));
        mRoot.Add(mFrameMarkers[bit]);
      }
    }
#endif
    Refresh();
#ifdef TEXT_LANGUAGE_CAPTURE
    if(const char* path = std::getenv("TEXT_LANGUAGE_CAPTURE_DIR"))
    {
      mOutput = path;
      mTimer  = Timer::New(100);
      mTimer.TickSignal().Connect(this, &LanguageSample::Tick);
      mTimer.Start();
    }
#endif
  }
  void Refresh()
  {
    mTicks     = 0;
    mCompleted = 0;
#ifdef TEXT_LANGUAGE_CAPTURE
    if(std::getenv("TEXT_LANGUAGE_CAPTURE_DIR"))
    {
      for(unsigned bit = 0; bit < 5; ++bit) mFrameMarkers[bit].SetBackgroundColor(UiColor((mPage & (1u << bit)) ? 0xffffff : 0x000000));
      mApp.GetWindow().KeepRendering(1.2f);
    }
#endif
    std::string  text  = "бгдпт · ";
    const size_t start = mPage * PAGE_SIZE, end = std::min(start + PAGE_SIZE, mCodepoints.size());
    for(size_t n = start; n < end; ++n)
    {
      text += Utf8(mCodepoints[n]);
      text += ' ';
    }
    std::ostringstream title;
    title << mFontFamily << " | page " << mPage + 1 << "/" << Pages() << " | U+" << std::hex << std::uppercase << mCodepoints[start] << "..U+" << mCodepoints[end - 1]
          << " | field=" << (mSwapped ? "ru" : "sr") << " editor=" << (mSwapped ? "sr" : "ru")
          << "\nN/P page   I italic   B bold   A async   L swap ru/sr   Esc exit | " << (mAsync ? "async" : "sync") << " | " << (mItalic ? "italic" : "upright");
    mTitle.SetText(title.str().c_str());
    for(size_t i = 0; i < mLabels.size(); ++i)
    {
      auto         label         = mLabels[i];
      const size_t languageIndex = mSwapped && (i == 0 || i == 3) ? 3 - i : i;
      Language::SetTextLanguage(label, LANGUAGES[languageIndex]);
      mLanguageLabels[i].SetText(LANGUAGES[languageIndex]);
      label.SetAsyncRendering(mAsync);
      label.SetFontSlant(mItalic ? Text::FontSlant::ITALIC : Text::FontSlant::NORMAL);
      label.SetFontWeight(mBold ? Text::FontWeight::BOLD : Text::FontWeight::NORMAL);
      label.SetText(text.c_str());
    }
    mField.SetFontSlant(mItalic ? Text::FontSlant::ITALIC : Text::FontSlant::NORMAL);
    mEditor.SetFontSlant(mItalic ? Text::FontSlant::ITALIC : Text::FontSlant::NORMAL);
    Language::SetTextLanguage(mField, mSwapped ? "ru" : "sr");
    Language::SetTextLanguage(mEditor, mSwapped ? "sr" : "ru");
  }
  size_t Pages() const
  {
    return (mCodepoints.size() + PAGE_SIZE - 1) / PAGE_SIZE;
  }
  void Finished(View, float, float)
  {
    ++mCompleted;
    mTicks = 0;
  }
  void Key(Dali::Window, KeyEvent event)
  {
    if(event.GetState() != KeyEvent::UP) return;
    if(IsKey(event, DALI_KEY_ESCAPE) || IsKey(event, DALI_KEY_BACK))
    {
      mApp.Quit();
      return;
    }
    const auto key = event.GetKeyName();
    if(key == "n" || key == "Right")
      mPage = (mPage + 1) % Pages();
    else if(key == "p" || key == "Left")
      mPage = (mPage + Pages() - 1) % Pages();
    else if(key == "i")
      mItalic = !mItalic;
    else if(key == "b")
      mBold = !mBold;
    else if(key == "a")
      mAsync = !mAsync;
    else if(key == "l")
      mSwapped = !mSwapped;
    else
      return;
    Refresh();
  }
#ifdef TEXT_LANGUAGE_CAPTURE
  bool Tick()
  {
    if(++mWatchdog > 3000)
    {
      std::fprintf(stderr, "Capture timeout\n");
      std::exit(3);
    }
    if(++mTicks < 7 || (mAsync && mCompleted < 6)) return true;
    auto display = XOpenDisplay(nullptr);
    auto image   = XGetImage(display, static_cast<::Window>(mApp.GetWindow().GetNativeId()), 0, 0, 1280, 900, AllPlanes, ZPixmap);
    if(!image) std::exit(4);
    for(unsigned bit = 0; bit < 5; ++bit)
    {
      if((XGetPixel(image, 1225 + bit * 10, 885) & 0xffffff) != ((mPage & (1u << bit)) ? 0xffffffu : 0u))
      {
        XDestroyImage(image);
        XCloseDisplay(display);
        mApp.GetWindow().KeepRendering(0.5f);
        return true;
      }
    }
    std::ofstream file(mOutput + "/" + std::to_string(mPass) + "-" + std::to_string(mPage) + ".ppm", std::ios::binary);
    file << "P6\n1280 900\n255\n";
    for(int y = 0; y < 900; ++y)
      for(int x = 0; x < 1280; ++x)
      {
        auto          pixel = XGetPixel(image, x, y);
        unsigned char rgb[] = {static_cast<unsigned char>(pixel >> 16), static_cast<unsigned char>(pixel >> 8), static_cast<unsigned char>(pixel)};
        file.write(reinterpret_cast<char*>(rgb), 3);
      }
    XDestroyImage(image);
    XCloseDisplay(display);
    std::printf("CAPTURE pass=%u page=%zu async=%d\n", mPass, mPage, mAsync);
    std::fflush(stdout);
    if(mPass == 0)
    {
      if(++mPage == Pages())
      {
        mPass = 1;
        mPage = 0;
      }
    }
    else if(++mPage == Pages())
    {
      mApp.Quit();
      return false;
    }
    Refresh();
    return true;
  }
#endif
#ifdef TEXT_LANGUAGE_CAPTURE
  View mFrameMarkers[5];
#endif
  Application           mApp;
  AbsoluteLayout        mRoot;
  Label                 mTitle;
  std::vector<Label>    mLabels;
  std::vector<Label>    mLanguageLabels;
  InputField            mField;
  InputEditor           mEditor;
  std::vector<uint32_t> mCodepoints;
  size_t                mPage  = 0;
  bool                  mAsync = false, mItalic = false, mBold = false;
  bool                  mSwapped    = false;
  std::string           mFontFamily = "Noto Sans";
  unsigned              mCompleted = 0, mTicks = 0, mWatchdog = 0, mPass = 0;
  Timer                 mTimer;
  std::string           mOutput;
};
int main(int argc, char** argv)
{
  auto app = Application::New(&argc, &argv);
  UiConfig::New().Apply();
  LanguageSample sample(app);
  app.MainLoop();
  return 0;
}
