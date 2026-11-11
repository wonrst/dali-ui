#!/usr/bin/env bash
# Use only the selected fonts; do not change the host's installed fonts.
set -euo pipefail
sample_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export TEXT_LANGUAGE_FONT_DIR="${TEXT_LANGUAGE_FONT_DIR:-$sample_dir/res/text-language/fonts}"
sample_config="$(mktemp -d "${TMPDIR:-/tmp}/dali-text-language.XXXXXXXX")"
trap 'rm -rf -- "$sample_config"' EXIT
python3 - "$TEXT_LANGUAGE_FONT_DIR" "$sample_config" <<'PY'
from pathlib import Path
from xml.sax.saxutils import escape
import sys
fonts, config = map(Path, sys.argv[1:])
if not fonts.is_dir():
    raise SystemExit(f"Font directory not found: {fonts}")
(config / 'fonts.conf').write_text(
    '<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "fonts.dtd">\n'
    '<fontconfig><dir>' + escape(str(fonts.resolve())) + '</dir><cachedir>'
    + escape(str(config / 'cache')) + '</cachedir></fontconfig>\n')
PY
export FONTCONFIG_FILE="$sample_config/fonts.conf"
export DALI_WINDOW_WIDTH="${DALI_WINDOW_WIDTH:-1280}"
export DALI_WINDOW_HEIGHT="${DALI_WINDOW_HEIGHT:-900}"
"$sample_dir/bin/text-language.example" "$@"
