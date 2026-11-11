Text language native regression probes
=====================================

These tests use real HarfBuzz, FreeType, DALi adaptor, UI and glyph caches.
The regular internal UTC executable uses a shaping mock and verifies only the
input/API contracts; it does not prove language-specific glyph substitution.

Build/install the modified adaptor before the UI. Point PKG_CONFIG_PATH and
LD_LIBRARY_PATH at that SDK. Debug UI symbols are required for native probes.
On Ubuntu, install the normal DALi build dependencies plus Xephyr and Python
Pillow (a Python virtual environment is sufficient).

From the dali-ui repository:

  cmake -S samples/text -B build/text-language -DCMAKE_BUILD_TYPE=Debug \
    -DTEXT_LANGUAGE_AUDIT=ON -DTEXT_LANGUAGE_CAPTURE=ON
  cmake --build build/text-language --target text-language.example \
    text-language-cache-audit text-language-ui-audit \
    text-language-transition-audit text-language-language-semantics -j4
  python3 automated-tests/text-language/run-audit.py \
    --build build/text-language --output /tmp/text-language-results --screens

Run from a graphical X11 session (DISPLAY/XAUTHORITY); the runner starts its
own Xephyr display. It isolates Fontconfig to the selected reference fonts.
--font-dir selects another Noto Sans build; the probes expect NotoSans.ttf and
NotoSans-Italic.ttf and their Serbian locl substitutions. To experiment with
other families, use samples/text/run-text-language.sh instead.

Coverage:
- 2 faces x 3 point sizes x 26 tags x 1096 Unicode-derived corpus cases.
- Independent HarfBuzz glyph/cluster checks; fresh/warm metrics and bitmap
  comparisons with randomized interleaving of languages.
- 200 Controller transitions and 200 AsyncTextLoader transitions.
- 32 rendered states: language-only changes on reused Label/InputField/
  InputEditor versus fresh controls, including placeholders and obsolete
  async requests. Rejects blank screenshots and verifies frame presentation.
- --screens: 22 pages x regular/italic x sync/async x cold/warm, plus sync/async
  comparisons (132 equal-pixel pairs). Capture support is disabled by default.
- Empty override follows system language; explicit und remains independent
  of it and still performs ordinary shaping, including ligatures.

The pinned fonts and Unicode source hashes are recorded under
samples/text/res/text-language. They cover 456 of 521 enumerated code points;
the 65 missing code points are not evidence of successful font coverage.
Default variation coordinates and this Ubuntu configuration are tested;
these results do not certify every font, variation setting or device.

Compatibility check (capture baseline BEFORE installing the modified adaptor):
Keep the old devel-api/text-abstraction/shaping.h and the actual old shared
library as baseline/shaping.h and baseline/libdali3-adaptor.so.3.0.0. Then run:

  python3 automated-tests/text-language/check-compatibility.py \
    --baseline /path/to/baseline --sdk "$DESKTOP_PREFIX" \
    --output /tmp/text-language-compatibility

This checks a client compiled against the old Shape signature, output for
seven system locales, and preservation of strong exported DALi symbols.
It also builds a client with the new header and checks that the five-argument
Shape overload and the six-argument overload with empty language agree.
Code taking &Shaping::Shape with the new header must specify the desired
member-function pointer type because Shape now has two overloads.
It is a host-side binary/source compatibility check, not device certification.
