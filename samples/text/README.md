# Text sample

This sample demonstrates DALi UI text features.

## Features

- Basic Label text rendering
- Layout direction
- Per-control shaping language (integration API)
- Markup text
- Marquee
- Text style, bevel, and cutout mask
- TextGradient visual playground
- Text fit and fit candidate
- Font variation
- Emoji rendering
- Render scale
- InputField and InputEditor
- Typing style
- Localization with override callback
- Localization with PO/MO resources
- Application-side formatting of localized PO/MO strings
- Custom component localization
- Localized TextGradient markup ranges

## Examples

| Executable | Description |
| --- | --- |
| `text.example` | Basic text sample |
| `text-layout-direction.example` | Text layout direction sample |
| `text-language.example` | Shaping language, Cyrillic glyphs, sync/async and input controls |
| `text-markup.example` | Markup text sample |
| `text-marquee.example` | Marquee text sample |
| `text-style.example` | Text style sample |
| `text-style-bevel.example` | Text bevel style sample |
| `text-fit.example` | Text fit sample |
| `text-fit-candidate.example` | Text fit candidate sample |
| `text-scale.example` | System font/UI scale, ImageSpan resources, InputField and sync/async TextFit |
| `text-font-variation.example` | Font variation sample |
| `text-cutout-mask.example` | Text cutout mask sample |
| `text-gradient.example` | Label TextGradient visual playground |
| `text-gradient-simple.example` | Minimal Label TextGradient animation sample |
| `text-emoji.example` | Emoji rendering sample |
| `text-render-scale.example` | Text render scale sample |
| `text-input-field.example` | InputField sample |
| `text-clipboard.example` | Clipboard public API sample |
| `text-input-editor.example` | InputEditor sample |
| `text-typing-style.example` | Typing style sample |
| `text-localization.example` | Localization sample using override callback |
| `text-localization-po.example` | Localization sample using PO/MO resources |
| `text-formatted-localization.example` | Positional printf formatting and gettext plural lookup using localized PO/MO strings |
| `text-localization-custom-component.example` | Custom component localization sample |
| `text-gradient-localization.example` | Localized markup ranges with TextGradient |
| `text-image-span.example` | Sync/async inline image layout playground |

## Text shaping language

`text-language.example` uses `Dali::Ui::Integration::Text::SetTextLanguage`
for `Label`, `InputField`, and `InputEditor`. Include
`<dali-ui-foundation/integration-api/text/text-language.h>`.

```cpp
auto label = Dali::Ui::Label::New("бгдпт");
Dali::Ui::Integration::Text::SetTextLanguage(label, "sr-Cyrl-RS");
```

The argument is one well-formed ASCII BCP 47 language tag (up to 255 bytes).
Tags are case-insensitive; the getter returns the lowercase authored tag.
Invalid input returns `false` and leaves the previous setting intact.
`""` removes the local override (currently follows the system language);
`"und"` explicitly selects undetermined language. POSIX locale names such as
`sr_RS.UTF-8`, language lists and whitespace are rejected. This API affects
shaping of the whole control, including placeholders. It does not select
fallback fonts, change direction, translate content or propagate to children.

The sample compares `ru`, `uk`, `bg`, `sr`, `mk`, and `und` across 521 Cyrillic
and related code points from Unicode 17.0.0. Bundled Noto Sans 2.015 fonts cover
456 of those; unsupported characters are expected. Font hashes, sources,
coverage and the OFL license are in `res/text-language`. The upright font has
an alternative `б` for Serbian/Macedonian; italic also localizes `г д п т ѓ`.
Identical text need not look different for every language.

After rebuilding/installing the adaptor and UI, build this sample:

```bash
cmake -S samples/text -B build/text-language -DCMAKE_BUILD_TYPE=Debug
cmake --build build/text-language --target text-language.example -j4
samples/text/run-text-language.sh
```

The Ubuntu launcher uses a private Fontconfig configuration so the bundled
fonts are selected even if another Noto Sans version is installed. It requires
Python 3. The executable also registers the font directory on startup.
To try other fonts, specify their directory and actual family name:

```bash
TEXT_LANGUAGE_FONT_DIR=/absolute/path/to/test-fonts \
TEXT_LANGUAGE_FONT_FAMILY="Test Font Family" \
  samples/text/run-text-language.sh
```

Keys: `N`/`P` or arrows change pages, `I` toggles italic, `B` toggles bold label
rows, `A` toggles async labels, `L` swaps `ru`/`sr` on existing labels and input
controls, and Escape exits. The heading shows the current input languages.
`TEXT_LANGUAGE_ITALIC=1` and `TEXT_LANGUAGE_ASYNC=1` select startup modes.

For real-font cache, language transition and screenshot regression tests, see
[the native audit instructions](../../automated-tests/text-language/README.txt).

## Localization resources

The PO source files are stored by locale under `res/po`.

~~~text
res/po/default/en_US.po
res/po/default/ko_KR.po
res/po/default/ar_AE.po

res/po/alternate/en_US.po
res/po/alternate/ko_KR.po
res/po/alternate/ar_AE.po

res/po/formatted/en_US.po
res/po/formatted/ko_KR.po
res/po/formatted/pl_PL.po

res/po/gradient/en_US.po
res/po/gradient/ko_KR.po
res/po/gradient/ar_AE.po
~~~

During build, CMake uses `msgfmt` to generate MO files into the gettext runtime layout.

~~~text
res/locale/default/en_US/LC_MESSAGES/text-localization-po.mo
res/locale/default/ko_KR/LC_MESSAGES/text-localization-po.mo
res/locale/default/ar_AE/LC_MESSAGES/text-localization-po.mo

res/locale/alternate/en_US/LC_MESSAGES/text-localization-po-alt.mo
res/locale/alternate/ko_KR/LC_MESSAGES/text-localization-po-alt.mo
res/locale/alternate/ar_AE/LC_MESSAGES/text-localization-po-alt.mo

res/locale/formatted/en_US/LC_MESSAGES/text-formatted-localization.mo
res/locale/formatted/ko_KR/LC_MESSAGES/text-formatted-localization.mo
res/locale/formatted/pl_PL/LC_MESSAGES/text-formatted-localization.mo

res/locale/gradient/en_US/LC_MESSAGES/text-gradient-localization-po.mo
res/locale/gradient/ko_KR/LC_MESSAGES/text-gradient-localization-po.mo
res/locale/gradient/ar_AE/LC_MESSAGES/text-gradient-localization-po.mo
~~~

The generated `.mo` files are build artifacts and are not intended to be tracked in git.

## Build

### Ubuntu

Requires DALi environment to be set up first.

~~~bash
# From dali-ui root
cd samples/text
cmake -DCMAKE_INSTALL_PREFIX=$DESKTOP_PREFIX
make -j4
~~~

Run:

~~~bash
./bin/text.example
./bin/text-gradient.example
./bin/text-gradient-simple.example
~~~

Run localization samples:

~~~bash
./bin/text-localization.example
./bin/text-localization-po.example
./bin/text-formatted-localization.example
./bin/text-localization-custom-component.example
./bin/text-gradient-localization.example
~~~

### Windows / MSVC

Use an x64 Visual Studio Developer PowerShell after installing the DALi
dependencies. The separate tools vcpkg supplies `msgfmt.exe`; the DALi vcpkg
supplies the runtime `libintl.dll`.

~~~powershell
$Msgfmt = "C:\Tools\DALI_VCPKG_TOOLS\vcpkg\installed\x64-windows\tools\gettext\bin\msgfmt.exe"

cmake -S .\samples -B C:\work\DALi\out\dali-ui-samples `
  -DDALI_UI_SAMPLE_LIST=text `
  "-DMSGFMT_EXECUTABLE=$Msgfmt"

cmake --build C:\work\DALi\out\dali-ui-samples --target `
  text-localization-po.example `
  text-formatted-localization.example `
  text-localization-custom-component.example `
  text-gradient-localization.example

$env:PATH = "C:\work\DALi\dali-env\bin;C:\Tools\DALI_VCPKG\vcpkg\installed\x64-windows\bin;$env:PATH"
.\samples\text\bin\text-localization-po.example.exe
.\samples\text\bin\text-formatted-localization.example.exe
~~~

For host-side manual testing, the Windows samples update gettext's `LANGUAGE`
environment variable. The formatted-localization sample uses `1`, `2`, and `3`
for `en_US`, `ko_KR`, and `pl_PL`; samples that include `ar_AE` use `3`. This
key-driven locale setup is not the locale-management pattern for Tizen devices.

### GBS build (Tizen)

~~~bash
# From dali-ui root
gbs build -A armv7l --include-all --packaging-dir samples/text/packaging
~~~

Output:

~~~text
com.samsung.dali.text-2.0.0-1.armv7l.rpm
~~~

## Controls

Common controls:

- **Escape** or **Back**: Quit the application.

Localization samples also provide additional key controls such as locale switching, domain switching, bypass mode, refresh, and manual text update. See each sample source file for details.
