#!/usr/bin/env python3
"""Check old-client ABI compatibility and both Shape overloads in the new header."""
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--baseline", type=Path, required=True,
                    help="directory with old shaping.h and libdali3-adaptor.so.3.0.0")
parser.add_argument("--sdk", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
baseline, sdk, output = (p.resolve() for p in (args.baseline, args.sdk, args.output))
output.mkdir(parents=True, exist_ok=True)
source = Path(__file__).resolve().with_name("legacy-corpus.cpp")
resources = source.parents[2] / "samples/text/res/text-language"
library = "libdali3-adaptor.so.3.0.0"
binary = output / "legacy-corpus"
subprocess.run([
    "c++", "-std=c++17", "-I" + str(sdk / "include"), "-I" + str(sdk / "include/dali"),
    '-DTEXT_LANGUAGE_BASELINE_SHAPING_HEADER="' + str(baseline / "shaping.h") + '"',
    str(source), str(baseline / library), "-L" + str(sdk / "lib"), "-ldali3-core", "-o", str(binary)
], check=True)
new_binary = output / "current-corpus"
subprocess.run([
    "c++", "-std=c++17", "-I" + str(sdk / "include"), "-I" + str(sdk / "include/dali"),
    str(source), "-L" + str(sdk / "lib"), "-ldali3-adaptor", "-ldali3-core", "-o", str(new_binary)
], check=True)
rows = []
new_rows = []
for font in ("NotoSans.ttf", "NotoSans-Italic.ttf"):
    outputs = []
    for library_path in (str(baseline) + ":" + str(sdk / "lib"), str(sdk / "lib")):
        env = dict(os.environ, LD_LIBRARY_PATH=library_path)
        result = subprocess.check_output([str(binary), str(resources / "fonts" / font),
                                          str(resources / "audit-corpus.txt")], env=env, text=True)
        outputs.append(next(line for line in result.splitlines() if line.startswith("LEGACY ")))
    rows.append(dict(font=font, baseline=outputs[0], patched=outputs[1], equal=outputs[0] == outputs[1]))
    result = subprocess.check_output([str(new_binary), str(resources / "fonts" / font),
                                      str(resources / "audit-corpus.txt")],
                                     env=dict(os.environ, LD_LIBRARY_PATH=str(sdk / "lib")), text=True)
    current = next(line for line in result.splitlines() if line.startswith("LEGACY "))
    new_rows.append(dict(font=font, baseline=outputs[0], current=current, equal=outputs[0] == current))


def symbols(path):
    values = set()
    for line in subprocess.check_output(["nm", "-D", "--defined-only", str(path)], text=True).splitlines():
        parts = line.split()
        if len(parts) == 3:
            _, kind, name = parts
            if kind in {"T", "D", "B", "R", "S", "G"} and "4Dali" in name:
                values.add(name)
    return values


before, after = symbols(baseline / library), symbols(sdk / "lib" / library)
report = dict(old_header_binary_runs=rows, new_header_overload_runs=new_rows,
              removed_strong_dali_symbols=sorted(before - after),
              added_strong_dali_symbols=sorted(after - before))
(output / "compatibility.json").write_text(json.dumps(report, indent=2) + "\n")
assert all(row["equal"] for row in rows), report
assert all(row["equal"] for row in new_rows), report
assert not before - after, report
print("PASS old-header binary output, new-header overloads and exported DALi symbol preservation")
