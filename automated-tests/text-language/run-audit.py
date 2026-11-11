#!/usr/bin/env python3
"""Run the real-font probes built by samples/text, using an isolated X display."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from xml.sax.saxutils import escape

from PIL import Image, ImageChops


def main():
    repo = Path(__file__).resolve().parents[2]
    sample = repo / "samples/text"
    resources = sample / "res/text-language"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True,
                        help="samples/text CMake build directory")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--font-dir", type=Path, default=resources / "fonts",
                        help="Noto Sans directory containing NotoSans.ttf and NotoSans-Italic.ttf")
    parser.add_argument("--screens", action="store_true",
                        help="also compare all 22 sample pages, cold/warm and sync/async")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fonts = args.font_dir.resolve()
    for name in ("NotoSans.ttf", "NotoSans-Italic.ttf"):
        if not (fonts / name).is_file():
            parser.error(f"Missing reference font: {fonts / name}")
    config = output / "fonts.conf"
    config.write_text(
        '<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "fonts.dtd">\n'
        '<fontconfig><dir>' + escape(str(fonts)) + '</dir><cachedir>'
        + escape(str(output / "fontcache")) + '</cachedir></fontconfig>\n')
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("DALI_", "TEXT_LANGUAGE_", "LD_PRELOAD"))}
    env.update(FONTCONFIG_FILE=str(config), TEXT_LANGUAGE_FONT_DIR=str(fonts),
               DALI_SHADER_USE_PROGRAM_BINARY="0", LP_NUM_THREADS="2",
               DALI_WINDOW_WIDTH="1280", DALI_WINDOW_HEIGHT="900")
    probes = args.build.resolve() / "text-language-audit"
    rows = []

    def run(name, command, runenv=None, timeout=180):
        with (output / (name + ".log")).open("w") as log:
            subprocess.run([str(x) for x in command], env=runenv or env,
                           stdout=log, stderr=subprocess.STDOUT, check=True, timeout=timeout)
        print("PASS", name, flush=True)

    run("font-match", ["fc-match", "-f", "%{file}\n", "Noto Sans"])
    run("cache", [probes / "text-language-cache-audit", fonts, resources / "audit-corpus.txt"])
    result = next(line for line in (output / "cache.log").read_text().splitlines()
                  if line.startswith("RESULT"))
    rows.append(dict(test="cache", result=result))
    run("language-semantics", [probes / "text-language-language-semantics", fonts])
    display = next(n for n in range(211, 260) if not Path(f"/tmp/.X11-unix/X{n}").exists())
    parent = dict(env, DISPLAY=os.environ.get("DISPLAY", ":1"),
                  XAUTHORITY=os.environ.get("XAUTHORITY", f"/run/user/{os.getuid()}/gdm/Xauthority"))
    with (output / "xephyr.log").open("w") as log:
        server = subprocess.Popen(["Xephyr", f":{display}", "-screen", "1280x900", "-ac", "-noreset"],
                                  env=parent, stdout=log, stderr=subprocess.STDOUT)
        try:
            env["DISPLAY"] = f":{display}"
            # A live server process may not yet accept X clients under build load.
            deadline = time.monotonic() + 15
            while True:
                if server.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Xephyr unavailable; check DISPLAY and XAUTHORITY")
                ready = subprocess.run(["xdpyinfo"], env=env, stdout=subprocess.DEVNULL,
                                       stderr=subprocess.DEVNULL, timeout=3)
                if ready.returncode == 0:
                    break
                time.sleep(0.1)
            run("ui", [probes / "text-language-ui-audit"])
            for mode in ("sync", "async", "placeholder-sync", "placeholder-async"):
                dest = output / mode
                dest.mkdir(exist_ok=True)
                for old in dest.glob("*.ppm"):
                    old.unlink()
                runenv = dict(env, TEXT_LANGUAGE_CAPTURE_DIR=str(dest))
                if mode.endswith("async"):
                    runenv["TEXT_LANGUAGE_ASYNC"] = "1"
                if mode.startswith("placeholder"):
                    runenv["TEXT_LANGUAGE_PLACEHOLDER"] = "1"
                run(mode, [probes / "text-language-transition-audit"], runenv)
                assert len(list(dest.glob("*.ppm"))) == 8, mode
                # The probe saves only text regions, excluding its stage marker.
                a = Image.open(dest / "0.ppm")
                b = Image.open(dest / "1.ppm")
                assert ImageChops.difference(a, b).getbbox(), "ru/sr did not visibly differ"
                rows.append(dict(test=mode, states=8))
            if args.screens:
                for italic in (False, True):
                    for asynchronous in (False, True):
                        mode = ("italic" if italic else "upright") + ("-async" if asynchronous else "-sync")
                        dest = output / "screens" / mode
                        dest.mkdir(parents=True, exist_ok=True)
                        for old in dest.glob("*.ppm"):
                            old.unlink()
                        runenv = dict(env, TEXT_LANGUAGE_CAPTURE_DIR=str(dest))
                        if italic:
                            runenv["TEXT_LANGUAGE_ITALIC"] = "1"
                        if asynchronous:
                            runenv["TEXT_LANGUAGE_ASYNC"] = "1"
                        run("screens-" + mode, [sample / "bin/text-language.example"], runenv)
                        assert len(list(dest.glob("*.ppm"))) == 44, mode
                        for page in range(22):
                            a = Image.open(dest / f"0-{page}.ppm")
                            b = Image.open(dest / f"1-{page}.ppm")
                            assert ImageChops.difference(a, b).getbbox() is None, (mode, page)
                            pixels = a.crop((100, 110, 1260, 650)).convert("RGB").tobytes()
                            ink = sum(min(pixels[i:i + 3]) < 150 for i in range(0, len(pixels), 3))
                            assert ink > 500, (mode, page, "blank screenshot")
                        Image.open(dest / "0-0.ppm").save(dest / "preview.png")
                        rows.append(dict(test=mode, cold_warm_pairs=22, changed_pixels=0))
                for style in ("upright", "italic"):
                    for page in range(22):
                        a = Image.open(output / f"screens/{style}-sync/0-{page}.ppm").crop((100, 110, 1260, 650))
                        b = Image.open(output / f"screens/{style}-async/0-{page}.ppm").crop((100, 110, 1260, 650))
                        assert ImageChops.difference(a, b).getbbox() is None, (style, page)
                    rows.append(dict(test=style + "-sync-vs-async", pairs=22, changed_pixels=0))
        finally:
            server.terminate()
            server.wait(timeout=10)
    (output / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
    print("PASS all text language audits:", output, flush=True)


if __name__ == "__main__":
    main()
