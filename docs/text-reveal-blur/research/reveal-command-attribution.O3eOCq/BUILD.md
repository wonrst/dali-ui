# Reproduce the previous successful GBS workflow

All commands operate on the new detached diagnostic worktrees.
The main Core/Adaptor/UI checkouts and the previous trace source worktrees are unchanged.
The previous successful GBS build root, profile, repositories and architecture are reused.
No target install/run/sdb operations are performed.

Bases and existing source contents are copied from `../reveal-trace-tizen101.prtWoU/`:

- Core `ecf444c414543b1e629f53afe1340beda2ee5f9c`.
- Adaptor `f451029c14a7bda7175254ffd3aa4c73bb8252ba`.
- UI `4d74bbf9d4169f808c0e11cc5e806d2e506f1d95` plus the same blur/sample/batching patches.
- Source-only retains the existing diagnostic delta. No new rendering variant.
- The previous spec flags and compatibility changes are unchanged.

New release identifiers separate these RPMs from the previous coarse trace packages.
Run sequentially against the shared build root:

```sh
cd /home/bowonryuubuntu/tizen/reveal-command-attribution.O3eOCq/core
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot /home/bowonryuubuntu/tizen/reveal-trace-tizen101.prtWoU/gbs-root \
  --release 2.ryucmd

cd /home/bowonryuubuntu/tizen/reveal-command-attribution.O3eOCq/adaptor
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot /home/bowonryuubuntu/tizen/reveal-trace-tizen101.prtWoU/gbs-root \
  --release 2.ryucmd

cd /home/bowonryuubuntu/tizen/reveal-command-attribution.O3eOCq/production
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot /home/bowonryuubuntu/tizen/reveal-trace-tizen101.prtWoU/gbs-root \
  --release 2.ryucmd.production

cd /home/bowonryuubuntu/tizen/reveal-command-attribution.O3eOCq/source-only
gbs build -A armv7l -P tizen_10.1 --include-all --threads 8 \
  --buildroot /home/bowonryuubuntu/tizen/reveal-trace-tizen101.prtWoU/gbs-root \
  --release 2.ryucmd.sourceonly
```

Copy each successful variant's RPMs to its separate artifact directory before
the next UI build, because GBS may replace local repository package entries.
The existing `com.samsung.dali.text-2.0.0-1.ryutrace.armv7l.rpm` is reused
byte-for-byte: sample source, trace ABI and application behavior are unchanged.
Its runtime dependencies use the same library SONAMEs, not the previous exact release.

Wrapper logs are in this directory; detailed rpmbuild logs are saved in `logs/`.
Runtime installation files are under `artifacts/`; remaining development/debug RPMs
can be retained separately without mixing them into the install globs.

Host-only checks:

```sh
c++ -std=c++17 -Wall -Wextra -Werror collector-smoke.cpp -o collector-smoke
python3 test_analysis.py
```

These validate collector/parser arithmetic and control flow, not target overhead,
GPU performance, application quality, or production regression.
