# RT-BE90U port

| Name | Value |
| --- | --- |
| Status | Experimental ASUS-based image with Merlin-compatible extension hooks built; full Merlin integration unfinished; nothing flashed |
| Requested scope | JFFS scripts, Entware, custom service configuration, VPN/DNS/network controls, broad Merlin compatibility |
| Hardware inspected | 2026-09-25, stock firmware over SSH |
| `productid` | `TUF-BE9400` |
| `odmpid` | `RT-BE90U` |
| SoC | Qualcomm IPQ5322; SoC ID `593` |
| Architecture | `aarch64`, four CPU cores |
| Device-tree model | `Qualcomm Technologies, Inc. IPQ5332/ASUS-BE6500` |
| Device-tree compatible | `qcom,ipq5332-asus-be6500`, `qcom,ipq5332` |
| Installed firmware | `3.0.0.6.102_58500-g482542d_1264-g48728_Q7MB` |
| VPN settings inspected | No VPN Fusion profiles or device policies; all four SDNs use WAN (`vpnc_idx=0`) |
| Entware storage inspected | No USB filesystem mounted; `/opt` points to `/tmp/opt`; no `opkg` installed |
| Kernel | `5.4.277` |
| Kernel compiler | GCC `7.5.0`, OpenWrt `r0+12834-6a10c44cf508` |
| UBI volumes | `nvram`, `Factory`, `Factory2`, `linux`, `linux2`, `jffs2` |
| Loaded Wi-Fi modules | `qca_ol`, `wifi_3_0`, `umac`, `qdf`, `ipq_cnss2` |
| Loaded acceleration modules | `qca_nss_dp`, `qca_nss_ppe`, `qca_nss_sfe`, `ecm` |

Collect current hardware metadata from the repository root:

```sh
ssh user@router sh -s < tools/collect-router-platform.sh > router-platform.txt
```

The collector reads selected model/firmware NVRAM keys and hardware metadata. It does not change settings or read credentials, client lists, logs, or flash contents.

## ASUS source baseline

| Name | Value |
| --- | --- |
| Support page | [TUF-BE9400 downloads](https://www.asus.com/us/networking-iot-servers/wifi-routers/asus-gaming-routers/tuf-gaming-be9400/helpdesk_download?model2Name=TUF-Gaming-BE9400), OS: Others |
| GPL version | `3.0.0.6.102_58138`, published 2026-01-08 |
| Archive | [GPL_TUF_BE9400_300610258138.zip](https://dlcdnets.asus.com/pub/ASUS/wireless/TUF-BE9400/GPL_TUF_BE9400_300610258138.zip) |
| Verified SHA-256 | `930142d207b1f9dfc11e2ad17c767abc59a5df5777223dfc99f12a69f1d07b9e` |
| Platform | `release/src-qca-ipq53xx` |
| Stock build target | `make tuf-be6500`; `TUF-BE9400` inherits this profile |
| Effective target definitions | `buildtools/target.mak.3004`; selected ahead of `release/src-rt/target.mak` by the SDK Makefile |
| Alternative image name | `TUF-BE9400`, set by `platform.mak` |
| Target settings | `QCA=y IPQ53XX=y MUSL64=y ODMPID=y UBI=y DUAL_TRX=y` |
| Radio / switch profile | `QCN6274` / `QCA8386` |
| DTB | `qcom/ipq5332-tuf-be6500.dtb` |
| ODM handling | `src/router/rc/sysdeps/init-qca.c:init_others()` mounts RT-BE90U web/DLNA assets for the non-TUF UI |
| Wi-Fi driver | Actual AArch64 ELF modules under `ipq53xx/qca-wifi/prebuilt/modules` |
| Kernel / Wi-Fi module ABI | Kernel `5.4.277`; module vermagic `5.4.277 SMP preempt mod_unload aarch64` |
| Toolchain archives | Git LFS pointers, not usable compiler archives |

The GPL baseline predates the installed firmware. Shared product IDs and explicit ODM handling establish a source candidate, not flash compatibility.

## Toolchain

ASUS's archive contains these unresolved pointers:

| File | Expected SHA-256 | Expected bytes |
| --- | --- | ---: |
| `openwrt-gcc750_musl1124.aarch64.tar.bz2` | `8c2c7668796802e4ebe62b8d7305fb9c1037092b5ab6f24eba3e41a1a56838b5` | 316102099 |
| `openwrt-gcc750_musl1124.arm.tar.bz2` | `a1dc313d17e4f8e8112018e259d51e2d80d14db7d64f46b93cbb9ee504a9e269` | 319769307 |

| Name | Value |
| --- | --- |
| Candidate replacement | [SWRT-dev/qca-toolchains](https://github.com/SWRT-dev/qca-toolchains/tree/5e5b8ae593edb9c3f0711c56ef7fe73e113506d0/openwrt-gcc750_musl1124.aarch64) |
| Pinned commit | `5e5b8ae593edb9c3f0711c56ef7fe73e113506d0` |
| Compiler / libc metadata | GCC `7.5.0`, musl `1.1.24` |
| Verified compiler execution | `OpenWrt GCC 7.5.0 r0+12834-6a10c44cf508`, matching the running router's compiler identifier |
| Cross prefix | `aarch64-openwrt-linux-musl-` |
| ASUS archive equivalence | Unverified; matching versions do not establish identical toolchain contents |
| Container | `tools/rt-be90u/Dockerfile`, Linux x86_64 |

The container builds firmware with the AArch64 toolchain. Building U-Boot separately would require the ARM toolchain.

## Build host

| Name | Value |
| --- | --- |
| SSH alias | `archie` |
| Workspace | `~/projects/rt-be90u-port` |
| Source | `stock-58138/asuswrt` |
| Download | `downloads/GPL_TUF_BE9400_300610258138.zip` |
| Container context | `container/` |
| Toolchain checkout | `qca-toolchains/` |
| Container log | `logs/container-build.log` |
| Stock build log | `logs/stock-build.log` |
| Successful stock build log | `logs/stock-build-retry.log` |
| Merlin comparison checkout | `merlin-3006/`, sparse checkout, branch `DEV_rt-be90u` |
| Experimental development source | `port-58138/asuswrt` |
| Clean build source | `experimental-clean-58138/asuswrt` |
| Merlin baseline commit | `920b77f5f92db14717a27abd5c8e1b06ae6c8ec1` (`gnuton/master-3006`) |
| Repository branch | `DEV_rt-be90u`, investigation changes based on `master` |
| Filesystem | Linux case-sensitive storage; macOS checkout has colliding filenames |

From this repository on a Linux x86_64 host with Docker, Git, curl, unzip and tar:

```sh
bash tools/rt-be90u/prepare-build.sh ~/projects/rt-be90u-port
cd ~/projects/rt-be90u-port
docker run --rm --network none --user "$(id -u):$(id -g)" \
  --mount "type=bind,src=$PWD/stock-58138/asuswrt,dst=/work" \
  rt-be90u-build:58138 > logs/stock-build.log 2>&1
```

The Merlin baseline lacks `rc/sysdeps/init-qca.c` and the IPQ53xx platform. Existing Qualcomm conditionals alone do not provide buildable support.

| Check | Result |
| --- | --- |
| Collector shell syntax / ShellCheck | Passed |
| Collector on stock RT-BE90U | Passed; this BusyBox shell requires `type`, as `command -v` is unavailable |
| Build setup shell syntax / ShellCheck | Passed |
| Build setup on `archie` | Passed; container and compiler execute |
| Corrupt archive rejection | Passed; stops before extraction or toolchain setup |
| Stock image build | `make` exited 0 after adding `liblzma-dev`; subsequent audit found silently ignored dictionary-tool crashes |

## Stock image result

| Name | Value |
| --- | --- |
| Image directory | `stock-58138/asuswrt/release/src-qca-ipq53xx/image/` |
| Candidate filename | `TUF-BE9400_3.0.0.6_102_58138-g39dc88b_934-g86de7_Q7MB.trx` |
| SHA-256 | `ac285411de09881cc3953977c9a89e216b0a470ea8e533f79babe9a6e62a535b` |
| Bytes | 60110619 |
| Header / payload CRC32 | Both valid |
| Image format | uImage magic `0x27051956`; AArch64, Linux, LZMA |
| Load / entry address | Both `0x40080000` |
| Embedded model | `TUF-BE9400`, also present in ASUS's official RT-BE90U 58500 image |
| Partition size check | Fits the observed `linux` volume (`0x0500b000` bytes) |
| Vendor image tool | `asustools/mkimage -l` reads the image successfully |
| Boot / radio / Ethernet tests | Not performed |
| Optional ASUS post-build checks | `checkjffs.py` and `checktmpfs.py` absent from the GPL archive; their ignored errors do not fail `make` |
| Later packaging audit | `LnxHtmlEnumDict` crashes on long lines; repeated stock builds can include host core dumps under `/www`. Stock image is a reference artifact only. |
| Generic filename issue | ASUS's `TUF-BE9400.trx` symlink points to the BE6500 image; use the versioned BE9400 filename for inspection |

Comparison used ASUS's official `FW_RT-BE90U_300610258500.zip`, verified against SHA-256 `05a5c8227f3ecc0724787bb333728d55449c6984a823f5143108ce649388df75`. The extracted official image is 62924077 bytes, with the same model, architecture, load/entry addresses and valid CRC32 values. These checks establish image structure only, not bootability or firmware-upgrade acceptance.

## Port milestones

| Order | Work | Acceptance |
| ---: | --- | --- |
| 1 | Rebuild ASUS baseline in isolation | Done: image build and header checks; dictionary-tool defect identified during subsequent content audit |
| 2 | Prepare a clean `master-3006` integration branch | Import IPQ53xx platform and required shared ASUS changes; preserve supported Broadcom profiles |
| 3 | Integrate Merlin changes with Qualcomm/musl | Compile `rc`, shared libraries, web UI, VPN, DNS and filesystem packages |
| 4 | Validate image format and recovery | Confirm ODM/model checks, partition limits, dual-image behavior, signature requirements and recovery procedure |
| 5 | Validate on hardware | Boot, Ethernet/VLANs, all three radios, MLO, acceleration, USB, LEDs/buttons, IPv6 and VPN |
| 6 | Add model to CI and releases | Repeatable build and verified hardware support |

No flash operation is part of the inspection or stock build commands.

## Experimental extension port

Patches apply only to the verified Qualcomm GPL archive. Existing Broadcom source trees and build targets are unchanged. This is an incremental port, not a complete Merlin firmware release.

| Name | Value |
| --- | --- |
| Patch series | `tools/rt-be90u/patches/series` |
| Dictionary conversion | `html-enum.py` replaces the crashing host converter; preserves translation indices without fixed line buffers; preprocessing failures stop the build |
| Merlin reference | `gnuton/master-3006` at `920b77f5f92db14717a27abd5c8e1b06ae6c8ec1` |
| Version | `3.0.0.6.102_58138-rtbe90u-dev1` |
| Candidate image | `experimental-clean-58138/asuswrt/release/src-qca-ipq53xx/image/TUF-BE9400_3.0.0.6_102_58138-rtbe90u-dev1.trx` on `archie` |
| Candidate SHA-256 | `6a7ad7abe5046bd4533773a3ec74acc1a115043a8de1e7bfdcc6826b91605a3c` |
| Candidate bytes | 58982513 |
| Final build log | `logs/port-clean-final-build.log` on `archie`; `make tuf-be6500` exited 0 |
| Local artifact copy | `tools/rt-be90u/artifacts/` (Git-ignored) |
| Enable switch | Administration → System → Enable JFFS custom scripts and configs |
| Default | Disabled; an absent `jffs2_scripts` setting also disables execution |
| Persistent directories | `/jffs/scripts`, `/jffs/configs`, `/jffs/addons`; initialized through the Qualcomm UBIFS path |
| Script API | Merlin names, argument order, blocking timeouts and background execution conventions |
| Service events | `init-start`, `services-start`, `services-stop`, `service-event`, `service-event-end` |
| Network events | `wan-event`, `wan-start`, `firewall-start`, `nat-start`, `dhcpc-event`, `zcip-event` |
| Storage events | `pre-mount`, `post-mount`, `unmount` |
| WireGuard events | `wgclient-start`, `wgclient-stop`, `wgserver-start`, `wgserver-stop` |
| OpenVPN events | `openvpn-event` with six arguments and the inherited OpenVPN environment; includes VPN Fusion route callbacks |
| Config extensions | Account files, hosts, dnsmasq, Stubby, Inadyn, UPnP, Avahi, FTP, IGMP proxy, WireGuard and IPsec |
| Per-network DNS | `dnsmasq-sdn.postconf`, `stubby-sdn.postconf`, indexed `.add` files |
| Entware prerequisites | `/opt`, `cru`, USB lifecycle hooks and custom scripts; package installation and runtime tests pending |
| VPN engine | Default extension image retains ASUS `libvpn.so`; the separate development variant below uses Merlin `libovpn` |
| VPN ABI audit | Only source-built `rc` and `httpd` depend on stock `libvpn`; Merlin key enums and config structs differ, so consumers must be recompiled together; ASUS's 256-byte password field retained |
| ELF linkage check | Extension and VPN development images pass recursive library, AArch64 and required-symbol checks for `rc` (26 objects) and `httpd` (23 objects); no program execution |
| Add-on compatibility | No claim of general compatibility; Merlin-specific UI APIs and firmware detection remain absent |

The candidate Entware feed is `aarch64-k3.10`, based on the router's architecture/kernel and [Entware's ASUSWRT instructions](https://github.com/Entware/Entware/wiki/Install-on-ASUSWRT-step-by-step). No packages or scripts have been installed on the router.

Audit an extracted firmware filesystem on Linux with GNU `readelf`:

```sh
python3 tools/rt-be90u/check-linkage.py /path/to/rootfs
```

The audit uses musl's default library paths and symbol names. It does not validate dynamically loaded plugins, runtime configuration, routing or bootability.

After preparing the stock build environment, run from this repository on Linux:

```sh
bash tools/rt-be90u/prepare-port.sh ~/projects/rt-be90u-port
cd ~/projects/rt-be90u-port
docker run --rm --network none --user "$(id -u):$(id -g)" \
  --mount "type=bind,src=$PWD/experimental-58138/asuswrt,dst=/work" \
  rt-be90u-build:58138 > logs/port-build.log 2>&1
```

`prepare-port.sh` verifies the GPL checksum, extracts into a new directory, applies the series without fuzzy matching, installs the host dictionary converter and records hashes. Existing source directories are refused. An optional second argument selects a different new source directory.

From the repository root:

```sh
python3 tools/rt-be90u/check-image.py \
  ~/projects/rt-be90u-port/experimental-58138/asuswrt/release/src-qca-ipq53xx/image/TUF-BE9400_3.0.0.6_102_58138-rtbe90u-dev1.trx
python3 -m unittest discover -s tools/rt-be90u/tests -p 'test_*.py'

docker run --rm --network none --read-only --tmpfs /tmp:exec --tmpfs /jffs \
  --mount "type=bind,src=$HOME/projects/rt-be90u-port/experimental-58138/asuswrt,dst=/work,readonly" \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-build:58138 sh /tests/run-scripts-test.sh
```

| Verification | Result |
| --- | --- |
| Patch application to ASUS baseline | Passed with zero fuzz |
| Source reproduction | 22 patched/copied files match a fresh run of `prepare-port.sh` |
| Compilation | Full build from extracted sources, followed by recompilation of the final changes; successful image packaging |
| Custom script/config tests | Passed: unset/disabled setting, permissions, missing scripts, literal arguments, timeout mode, append/replace gating |
| OpenVPN callback tests | Passed: enable switch, permissions, tunnel type, six-argument order, TUN/TAP MTU selection |
| Image checker tests | 8 passed: expected header, wrong board/architecture, CRC corruption, truncation, trailing data, empty/oversized payload |
| Dictionary converter tests | 5 passed: long lines, UTF-8/line endings, missing keys, malformed maps, file permissions |
| Image contents | Extracted and checked for AArch64/musl `rc`, shared helpers, hook names, UI switch, development version, `cru`, `/opt` |
| Image integrity | Header and payload CRC32 valid; fits observed UBI volume |
| Packaging hygiene | No host core files, macOS metadata or x86 ELF programs/libraries in runtime directories |
| Remaining vendor build warnings | Missing optional headers/tools/diagnostics, attempts to strip shell scripts, absent `checkjffs.py`/`checktmpfs.py`; hardware operation remains unverified |
| Test limits | Native helper tests stub NVRAM/process execution; they do not test router event ordering, radio operation or VPN routing |
| Hardware verification | Pending; router remains on stock firmware |

## VPN development variant

| Name | Value |
| --- | --- |
| Status | Build and offline testing only; not ready to flash |
| Preparation | `tools/rt-be90u/prepare-vpn.sh`; separate from the default extension patch series |
| Imported source | Pinned Git objects listed in `vpn/imports.json`; working-tree edits are ignored |
| Version | `58138-rtbe90u-dev3-vpn` |
| Image | `TUF-BE9400_3.0.0.6_102_58138-rtbe90u-dev3-vpn.trx`; 59,008,093 bytes |
| SHA-256 | `de4d88cd236b6ba75ffca941d262fb9de2c4eb97c3e4a77643b602352efb6a56` |
| Superseded build | `dev2-vpn` omitted `RTCONFIG_VPN_FUSION_MERLIN` because the SDK selected a different target file; its library tests did not establish the routing build configuration |
| Configuration guard | Both target files enable Merlin VPN integration; compilation and ARM tests reject configurations without that switch |
| Saved artifact | Local `tools/rt-be90u/artifacts/`; excluded from Git |
| Engine | Merlin `libovpn`; coordinated `rc`, HTTP and shared-library changes |
| Networking | Merlin VPN profile mapping, named routing tables, DNS integration, WireGuard routing and VPNDirector service handling |
| UI | OpenVPN client/server, WireGuard client/server, VPNDirector and VPN status pages; file-backed configuration handlers |
| Acceleration | Disabled through the IPQ53xx ECM selection path for this variant; throughput impact unmeasured |
| Credential compatibility | Retains the ASUS 256-byte password field; certificate names and private-file permissions tested |
| Custom configuration | Reads legacy ASUS NVRAM settings until a file-backed configuration is saved; clearing that file does not revive old settings |
| ARM execution test | Target image library under QEMU; credentials, custom settings, key storage/reset, VPNDirector storage/filtering and route-command generation pass; NVRAM and command execution are stubbed |
| Source integrity | All 55 source inputs match the preparation manifest after the clean build, including the SDK's active target file |
| Clean build | `logs/vpn-dev3-build.log` on `archie`; exited 0 |
| Compiled configuration | Merlin VPN enabled in `.config` and `shared/rtconfig.h`; legacy VPN entry point linked; ECM selector disassembly returns disabled |
| Regression check | ARM test rejects the superseded `dev2-vpn` configuration |
| Artifact checks | Both CRCs valid; fits observed UBI volume; no missing libraries or required symbols; all runtime ELF files AArch64; Qualcomm coprocessor firmware unchanged |
| Build and audit location | `~/projects/rt-be90u-port/experimental-vpn-dev3-58138` and `image-audit-vpn-dev3` on `archie`; results in `logs/vpn-dev3-*` |
| Remaining work | Stock VPN Fusion/SDN settings migration; browser tests; route, DNS-leak and kill-switch tests; add-on APIs; hardware validation |

The supplied Merlin checkout must contain commit `920b77f5f92db14717a27abd5c8e1b06ae6c8ec1`.

```sh
bash tools/rt-be90u/prepare-vpn.sh ~/projects/rt-be90u-port \
  ~/projects/rt-be90u-port/merlin-3006 ~/projects/rt-be90u-port/vpn-development
cd ~/projects/rt-be90u-port
docker run --rm --network none --ulimit core=0 --user "$(id -u):$(id -g)" \
  --mount "type=bind,src=$PWD/vpn-development/asuswrt,dst=/work" \
  rt-be90u-build:58138 > logs/vpn-development-build.log 2>&1
```

Test an extracted VPN image with its corresponding prepared source:

```sh
docker build -t rt-be90u-test:58138 tools/rt-be90u/tests
docker run --rm --network none --read-only --ulimit core=0 --tmpfs /tmp:exec,size=512m \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount type=bind,src=/absolute/path/to/asuswrt,dst=/work,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-test:58138 sh /tests/run-vpn-config-test.sh
```
