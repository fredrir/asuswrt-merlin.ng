# RT-BE90U port

The latest checkpoint is `dev11-vpn` (2026-09-26). WireGuard imports now validate
the whole profile before changing settings, preserving existing profiles on
invalid input and avoiding parser crashes/truncation. It retains authenticated
WireGuard QR exports, working OpenVPN downloads and the earlier OpenVPN routing,
cipher and 2.6.16 package fixes. The packaged image passes actual encrypted OpenVPN forwarding,
disconnect blocking, restart and TLS identity-rejection tests in isolated containers.
IPv4 policy/DNS, browser and Entware fixtures also pass. It is **not ready to flash**:
IPv6 bypasses the imported IPv4 kill switches, and router service integration,
physical recovery and hardware operation remain unverified.
See [recovery readiness](rt-be90u-recovery.md) before planning a hardware test.

| Name | Value |
| --- | --- |
| Status | Experimental dev11 VPN image built and tested offline; IPv6 policy protection and full Merlin integration unfinished; nothing flashed |
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
| IPv6 setting inspected | `ipv6_service=disabled` on 2026-09-26; no setting changed |
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
| Script API | Merlin names, argument order, blocking timeouts and background execution conventions; dev11 ARM shared-library execution through real Entware rc.unslung tested below |
| Service events | `init-start`, `services-start`, `services-stop`, `service-event`, `service-event-end` |
| Network events | `wan-event`, `wan-start`, `firewall-start`, `nat-start`, `dhcpc-event`, `zcip-event` |
| Storage events | `pre-mount`, `post-mount`, `unmount` |
| WireGuard events | `wgclient-start`, `wgclient-stop`, `wgserver-start`, `wgserver-stop` |
| OpenVPN events | `openvpn-event` with six arguments and the inherited OpenVPN environment; includes VPN Fusion route callbacks |
| Config extensions | Account files, hosts, dnsmasq, Stubby, Inadyn, UPnP, Avahi, FTP, IGMP proxy, WireGuard and IPsec |
| Per-network DNS | `dnsmasq-sdn.postconf`, `stubby-sdn.postconf`, indexed `.add` files |
| Entware prerequisites | `/opt`, `cru`, USB lifecycle hooks and custom scripts; package installation and hook/rc.unslung fixture pass; physical USB and boot/hotplug lifecycle pending |
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
| Version | `58138-rtbe90u-dev13-vpn` |
| Image | `TUF-BE9400_3.0.0.6_102_58138-rtbe90u-dev13-vpn.trx`; 59,047,165 bytes |
| SHA-256 | `a1834473eef78c39380275059fc6741beaab56f3524c29edf2d074e0aa7d22bb` |
| Superseded build | `dev2-vpn` omitted `RTCONFIG_VPN_FUSION_MERLIN` because the SDK selected a different target file; its library tests did not establish the routing build configuration |
| Configuration guard | Both target files enable Merlin VPN integration; compilation and ARM tests reject configurations without that switch |
| Saved artifact | Local `tools/rt-be90u/artifacts/`; excluded from Git |
| Engine | Merlin `libovpn` and OpenVPN 2.6.16; coordinated `rc`, HTTP and shared-library changes; libcap-ng included, DCO disabled |
| Networking | Merlin VPN profile mapping, named routing tables, DNS integration, WireGuard routing and VPNDirector service handling |
| UI | OpenVPN client/server, WireGuard client/server, VPNDirector and VPN status pages; file-backed configuration handlers |
| Acceleration | Disabled through the IPQ53xx ECM selection path for this variant; throughput impact unmeasured |
| Credential compatibility | Retains the ASUS 256-byte password field; certificate names and private-file permissions tested |
| Custom configuration | Reads legacy ASUS NVRAM settings until a file-backed configuration is saved; clearing that file does not revive old settings |
| ARM execution test | Target image library under QEMU; credentials, custom settings, key storage/reset, VPNDirector storage/filtering and route-command generation pass; NVRAM and command execution are stubbed |
| Source integrity | All 599 pre-build inputs match independent fresh preparation; 594 remain identical after compilation, with five expected Autotools-generated Makefiles separately accounted for |
| Build | Incremental build in a separate copy of completed dev12; version, shared importer and WireGuard server exporter changed; `make` exit 0; independent fresh preparation in `experimental-vpn-dev13-final-repro-58138` |
| Compiled configuration | Merlin VPN enabled in `.config` and `shared/rtconfig.h`; legacy VPN entry point linked; ECM selector in the new packaged `rc` was disassembled again and still returns disabled |
| Regression checks | ARM test rejects `dev2-vpn`; browser tests reproduce `dev3-vpn` UI/save failures and `dev4-vpn` extra-certificate loss |
| Browser fixes | Import VPN Director icons and retain them during packaging; add Merlin name validation; process generic VPN settings after their profile selectors |
| Certificate fixes | Save extra certificates and submitted generic-key values; read and accept up to 7,999 bytes; reject oversized indexed and generic submissions |
| Browser result | Six pages/tabs, VPN Director CRUD, OpenVPN settings, certificates, long chains, profile import/isolation and WireGuard settings pass; last browser run used the extracted dev12 image |
| Artifact checks | Both CRCs valid; fits observed UBI volume; `rc`, HTTP and OpenVPN linkage passes; 792 runtime ELFs are AArch64; 11 coprocessor ELFs unchanged; SquashFS offset 4323076 |
| Build and audit location | `~/projects/rt-be90u-port/experimental-vpn-dev13-58138` and `image-audit-vpn-dev13` on `archie`; results in `logs/vpn-dev13-*` |
| Current-router migration | Read-only inspection found no Fusion profiles, device policies, default VPN or SDN VPN assignments; no VPN assignments need conversion on this router |
| Encrypted OpenVPN | Generated static/TLS/tls-crypt/tls-crypt-v2 configurations, LAN forwarding, transport ciphertext, disconnect blocking, restart and wrong-server rejection pass |
| Encrypted WireGuard | Image ARM keys/import/routing and exact source config writers with native host-kernel tunnels; forwarding, outage, kill switch, wrong PSK, restart and server-exported connections pass; router kernel/full rc pending |
| Remaining work | Full router service lifecycle; IPv6 policy protection; general stock VPN Fusion/SDN migration; add-on APIs; remaining UI paths; recovery and hardware validation |

Dev12 reruns the image/source/linkage audits, ARM configuration/import checks,
packet-policy suite, browser suite, official userspace image validator and new WG
tunnel suite. Earlier dev11 OpenVPN/Entware results below remain their recorded
baseline; those suites were not repeated for this WG-only change. The OpenVPN
and libovpn binaries are unchanged. The official validator still does not prove
model enforcement or bootloader acceptance.

Dev13 reruns image/source/linkage audits, ARM configuration/import checks, the
official userspace image validator and expanded WG export/tunnel tests. Browser
and packet-policy coverage remains the dev12 baseline. HTTP, OpenVPN and libovpn
binaries are unchanged; rc and libshared changed for the WG fixes.

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

## VPN browser tests

The expanded suite passed on the final extracted dev11 image using local Chromium on
Arch Linux. Evidence: `logs/vpn-dev11-browser.log` and `logs/vpn-dev11-httpd.log`.
It now also verifies both server Export handlers, exact profile bytes, filenames,
server-unit selection and unauthenticated blocking using synthetic profile files.
The table below retains the earlier dev5 test provenance.

| Name | Value |
| --- | --- |
| Server | Extracted image HTTP binary and libraries under QEMU; disposable filesystem copy |
| Fixture | `tests/run-httpd.sh`, `httpd-nvram.c`; synthetic NVRAM, completed setup and credentials; service and shell execution intercepted |
| Browser | Pinned Playwright/Chromium; requires the fixture marker before login or writes; localhost fixture; external HTTP/HTTPS blocked by a local proxy |
| Dependencies | Node.js, pinned Playwright/Chromium and OpenSSL CLI; certificates are generated in a temporary directory and removed after the test |
| Coverage | Six VPN pages/tabs; VPN Director CRUD; OpenVPN client/server settings and certificates; long chains, oversized input, profile imports/isolation; WireGuard settings |
| Evidence | `logs/vpn-dev5-browser.log` and `logs/vpn-dev5-httpd.log` on `archie`; local `artifacts/dev5-vpn-validation/` includes regression logs |
| Limits | No router NVRAM persistence, VPN process execution, tunnels, routes or packet filtering |

On the Linux builder, from the repository root:

```sh
docker network create --internal rt-be90u-http-test
docker run -d --name rt-be90u-http-test --network rt-be90u-http-test \
  --read-only --ulimit core=0 --tmpfs /tmp:exec,size=512m \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-test:58138 sh /tests/run-httpd.sh
docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' rt-be90u-http-test
```

In a separate terminal on the browser machine, substitute the printed container IP:

```sh
ssh -N -L 127.0.0.1:18090:CONTAINER_IP:8080 archie
```

From the repository root on the browser machine:

```sh
cd tools/rt-be90u/tests/browser
npm ci
npx playwright install chromium
npm test
```

Stop the SSH tunnel with Ctrl-C. Remove the fixture on the builder:

```sh
docker rm -f rt-be90u-http-test
docker network rm rt-be90u-http-test
```

## Entware binary compatibility

The original dev5 execution fixture remains available. The separate complete
installation fixture and its dev11 results are documented below.

| Name | Value |
| --- | --- |
| Feed | [Entware AArch64](https://github.com/Entware/Entware/wiki#installation), `aarch64-k3.10` |
| Fixture | `tests/entware-fixture.json`; fixed package filenames and SHA-256 hashes |
| Downloads | `prepare-entware-test.py NEW_DIRECTORY`; verifies hashes before making the completed directory available; leaves existing directories unchanged |
| Runtime | Extracted `dev5-vpn` filesystem in a disposable, network-disabled container; firmware `/opt` link preserved |
| Passed | Static `opkg` reads the architecture configuration; dynamically linked GNU `find` 4.10.0 loads Entware's glibc and finds a fixture file |
| Failure checks | Existing download directory rejected; corrupt bootstrap rejected before extraction or execution |
| Evidence | `archie:~/projects/rt-be90u-port/logs/vpn-dev5-entware-test.log` and `logs/entware-negative-tests.log` |
| Limits | QEMU uses the build host's kernel; package installation scripts, USB mounts, boot/shutdown services and router execution remain untested |

The package payloads are extracted only in the test container; the installer and package scripts are not executed.

```sh
python3 tools/rt-be90u/prepare-entware-test.py /absolute/path/to/entware-test-packages
docker run --rm --network none --read-only --ulimit core=0 --tmpfs /tmp:exec,size=512m \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount type=bind,src=/absolute/path/to/entware-test-packages,dst=/packages,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-test:58138 sh /tests/run-entware-test.sh
```

## Packet enforcement tests

`tests/run-vpn-network-test.sh` executes the final image's ARM `libovpn` with
synthetic NVRAM. Its `system()` and `_eval()` calls execute real commands.
Native `dash`, `iproute2` and `iptables-nft` replace those utilities in a disposable
copy because this QEMU/host combination cannot execute the firmware's legacy
network utilities correctly. No global binfmt handler is registered.

The container starts with only loopback. Tests create veth LAN/SDN links and dummy
WAN/VPN links, inject Ethernet frames and inspect emitted packets. These are real
kernel routing/NAT tests with simulated tunnel interfaces, not encrypted tunnels
or the router's kernel/firewall. The current evidence used Linux `7.2.6-arch2-1`,
iproute2 `ss200127`, and iptables `1.8.4` with nftables.

| Coverage | Result |
| --- | --- |
| OpenVPN and WireGuard source policy, WAN exemption, disabled policy | Packets use the expected egress |
| UDP/TCP port-53 exclusive DNS | VPN DNAT and WAN exemption pass |
| Policy removal and empty VPN routing table | Protected IPv4 packets, including DNS, cannot reach WAN |
| Policy/route reapplication | Forwarding resumes through the simulated VPN link |
| Destination-only, source-and-destination and wildcard kill switches | Pass; unrelated destinations remain reachable |
| Router-originated traffic with wildcard kill switch | WAN route remains available for tunnel reconnection |
| Repeated application | No duplicate policy kill-switch rules |
| Explicit and NVRAM-associated SDN protection | Real parsing, blocking and removal pass |
| Overlapping OVPN/WG policies | Routing priority, DNS order, configured fallback and final blocking pass |
| OpenVPN up handler with missing gateway | Protected IPv4 stays blocked; supplying the gateway restores VPN routing |
| Global OpenVPN mode | LAN and SDN IPv4 protection passes |
| IPv6 DNS with global kill switch | **Known gap: packet escapes through WAN** |
| Dev5 regression | Destination-only traffic escapes through WAN for both OVPN and WG |

The IPv6 probe deliberately records the gap rather than counting it as protection.
The suite's successful exit establishes the IPv4 assertions only. It does not
cover encrypted DNS, actual handshakes, daemon reconnect timing, all firewall
chains, acceleration or router-originated DNS proxy behavior.

From the repository root, substitute absolute final-image and matching SDK paths:

```sh
docker build -f tools/rt-be90u/tests/Dockerfile.network \
  -t rt-be90u-network-test:58138 tools/rt-be90u/tests
docker run --rm --network none --cap-add NET_ADMIN --read-only --ulimit core=0 \
  --sysctl net.ipv4.ip_forward=1 \
  --sysctl net.ipv4.conf.all.rp_filter=0 --sysctl net.ipv4.conf.default.rp_filter=0 \
  --sysctl net.ipv6.conf.all.forwarding=1 \
  --tmpfs /tmp:exec,size=512m --tmpfs /etc/iproute2 \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount type=bind,src=/absolute/path/to/asuswrt,dst=/work,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-network-test:58138 sh /tests/run-vpn-network-test.sh
```

`--protocol ovpn` or `--protocol wg`, appended after the script name, isolates a
protocol's cases. The mixed-policy and IPv6 probes run with the default `both`.
The fixture refuses existing network interfaces; do not use host networking.
Docker removes all test links/rules with the container. No host routes, router
configuration or unrelated containers are modified.

Final dev11 evidence on `archie`: `logs/vpn-dev11-network.log`. Earlier regressions:
`logs/network-dev6/dev5-regression.log`, `logs/vpn-dev5-wg-network-regression.log`.
The failed intermediate wildcard implementation is retained in
`logs/network-dev6/reconnect-regression.log`; it was corrected before packaging.

## Entware installation and hook execution

`entware-install-fixture.json` pins `entware-opt` and its complete dependency
closure (18 packages), plus the bootstrap opkg. The original smaller fixture is
unchanged. Downloading does not install anything:

```sh
python3 tools/rt-be90u/prepare-entware-test.py --install /new/entware-install-packages
docker run --rm --network none --read-only --ulimit core=0 --tmpfs /tmp:exec,size=512m \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount type=bind,src=/new/entware-install-packages,dst=/packages,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-network-test:58138 sh /tests/run-entware-install-test.sh
```

The fixture keeps `/opt -> tmp/opt` and points `tmp/opt` to a simulated USB
directory. Actual ARM opkg installs the packages in offline mode; installed
post-install scripts are then invoked explicitly after adding QEMU launchers.
Native BusyBox supplies the shell and several utilities. ARM gzip, GNU find,
grep and locale tools run through explicit launchers. This arrangement is not
an execution test of the downloaded online installer or the firmware shell.

The image's actual ARM `libshared` calls `run_custom_script()` without execution
stubs. Disabled hooks do not run. Enabled `services-start` and `services-stop`
invoke Entware's installed `rc.unslung`, which starts a fixture ARM process and
stops it in reverse service order. The harness verifies the daemon's readiness
PID and running state before stopping it, then verifies termination and retained
storage files. No live USB device is mounted or formatted.

All assertions and explicit post-install scripts pass on dev11. Evidence:
`logs/vpn-dev11-entware-install.log`. Real USB mounting, power/reboot persistence,
hotplug order, `rc.func` daemon handling and third-party add-ons remain untested.
The user confirmed physical access, Ethernet and spare 8 GB/16 GB USB drives.
Only Linux and macOS recovery computers are available. Neither drive has been
mounted, formatted or otherwise changed by this work.

## OpenVPN package and encrypted tunnel regression tests

The vendor OpenVPN 2.4.12 rejects `data-ciphers` emitted by the imported Merlin
configuration writer. The VPN variant now replaces the whole OpenVPN and
libcap-ng directories with trees from the same pinned Merlin revision, yielding
OpenVPN 2.6.16 and its capability dependency. The AArch64 patch uses the kernel's
`__u64` definition instead of a conflicting private typedef. DCO remains disabled.
The preparation manifest records 599 inputs, including package source and licenses.
Autotools regenerates five tracked Makefiles during compilation; immutable inputs
and an independent fresh preparation are checked separately.

Generated-configuration testing also found two static-key problems: policy mode
did not initialize the VPN gateway without pushed routes, and the TLS fallback
cipher overrode the chosen static cipher. The writer now supplies a policy default
route and limits the fallback directive to TLS mode. The up handler never copies
the WAN default into the VPN table, so a missing gateway leaves enforced clients
blocked rather than sending them through WAN.

`tests/run-vpn-tunnel-test.sh` uses actual ARM OpenVPN peers, keys/configurations
written by ARM libovpn, and two isolated network namespaces. It tests `static`,
`tls`, `tls-crypt`, and `tls-crypt-v2` modes. The v2 peer uses a custom server
directive to emulate a provider; the server UI's own control-channel option is v1.
The library's server export embeds stored certificates/keys and is parsed by the
daemon. An additional phase connects the exported static-key, TLS and tls-crypt
profiles to the actual ARM server and verifies encrypted forwarding. Connection
and cryptographic options remain unchanged; the fixture names the TUN interface
and supplies native address/route setup. This checks exported key material with
OpenVPN 2.6.16; desktop route/DNS integration and other clients remain untested.
HTTP export handlers have a separate browser test using synthetic profile files. Custom tls-crypt-v2 provider exports are outside the
server UI's supported export modes.

Packets enter at the synthetic LAN, traverse the encrypted tunnel and arrive at
the peer. The transport capture checks that the plaintext marker is absent.
Stopping the client exercises its down hook and the kill switch; restarting it
restores forwarding. TLS tests reject a wrong server identity and verify that
traffic stays blocked. They deliberately filter pushed route directives while
retaining the peer's gateway to cover the gateway-initialization regression.

```sh
docker run --rm --network none --cap-add NET_ADMIN --cap-add SYS_ADMIN \
  --device /dev/net/tun --read-only --ulimit core=0 \
  --sysctl net.ipv4.ip_forward=1 \
  --sysctl net.ipv4.conf.all.rp_filter=0 --sysctl net.ipv4.conf.default.rp_filter=0 \
  --sysctl net.ipv6.conf.all.forwarding=1 \
  --tmpfs /tmp:exec,dev,size=512m --tmpfs /etc/iproute2 \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount type=bind,src=/absolute/path/to/asuswrt,dst=/work,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-network-test:58138 sh /tests/run-vpn-tunnel-test.sh --mode tls
```

Use each mode for full coverage. `SYS_ADMIN` creates only the nested test network
namespace; `/dev/net/tun` is used in those namespaces. Do not use host networking.
The QEMU 7.2.22 update enables TUNSETIFF; firmware ip route mutations and wg's
generic-netlink writes still fail under this emulator. Native shell/iproute2
adapters assign addresses and dispatch generated hooks to the actual ARM libovpn.
NVRAM remains synthetic, and foreground operation replaces daemonization.
Cache-flush attempts report a missing chroot `/proc` file; that kernel boundary
is not replaced with a success stub.

These tests do not run the router's kernel, full rc service orchestration, DNS
proxy restart, real USB, acceleration or encrypted WireGuard datapath. Those
limits, IPv6 VPN protection and hardware validation remain explicit release gaps.

All four modes pass against the extracted final dev11 image. Evidence:
`logs/vpn-dev11-tunnel-static.log`, `logs/vpn-dev11-tunnel-tls.log`,
`logs/vpn-dev11-tunnel-tls-crypt.log`, and `logs/vpn-dev11-tunnel-tls-crypt-v2.log`.
The packaged `libovpn.so` SHA-256 is
`4f64ac512cf0803ff6ba3e7631e3cee6346c91f9d82a054771b1c141daf9d845`;
OpenVPN's is `a70908279b35b2c5447c9ed734c8877d645779e1b2b4d7cab7464d5b956078ad`.
The previous dev7 static-cipher failure and missing-gateway escape remain recorded
in `logs/vpn-dev7-tunnel-static.log` and `logs/vpn-dev7-gateway-regression.log`.

Exported-profile evidence: `logs/vpn-dev8-export-static.log`,
`logs/vpn-dev8-export-tls.log`, and `logs/vpn-dev8-export-tls-crypt.log`.
The original generated-config lifecycle tests also pass in those runs; explicit
fixture routes are introduced only in the separate exported-profile phase.

Dev9 packages `client1.ovpn` and `client2.ovpn` links to their corresponding
runtime server profiles, retaining the legacy `client.ovpn` alias. The numbered
links are conditional on Merlin VPN integration. Dev8's missing-download
regression is recorded in `logs/vpn-dev8-export-browser-regression.log`.
All dev9 runtime ELFs match dev8 except `libshared.so` and `miniupnpd`; those two
are byte-identical after normalizing version/build timestamp strings. Evidence:
`logs/vpn-dev9-runtime-comparison.json`. Final dev9 runtime tests were rerun and pass,
including all three exported-profile modes in the tunnel logs above.

The user requested continued offline work on 2026-09-26 because other processes
cannot be interrupted until the following morning. Recovery/hardware actions
remain unapproved; the passage of time does not authorize them.

## WireGuard browser coverage and QR authentication

The final dev10 browser fixture passes valid client imports with private/public/
preshared keys, an IPv6 endpoint, AllowedIPs, DNS, MTU and keepalive, while
preserving other client profiles. Server peer editing, disable and unit isolation
also pass. Authenticated configuration and QR downloads select the correct peer.
Synthetic per-peer files stand in for rc-generated export artifacts; these tests
do not establish key generation, QR encoding correctness or encrypted WG traffic.

The GPL HTTP table inherited into dev9 protected `wgs_client.conf` but left
`wgs_client.png` without authentication. The server creates those QR images from
peer configurations containing private keys. The isolated dev9 regression exposed
a synthetic PNG marker before login; the configuration marker stayed protected.
Dev10 assigns the same authentication handler to both routes. The expanded suite
rejects either marker before login and verifies both peer exports after login.
Evidence: `logs/vpn-dev9-wg-qr-auth-regression.log` and the final dev10 browser log.
This defect was reproduced only in the offline candidate fixture; installed stock
58500 was not probed. Dev11 additionally validates failed imports as described below.

## WireGuard import validation

The old GPL importer restored defaults before opening the file, reported success
for empty input, crashed on a recognized field without `=`, truncated long lines,
and silently accepted multiple peers/unsupported settings. These failures are
reproduced against dev10 in `logs/vpn-dev10-wg-import-regression.log`.

Dev11 stages and validates up to 64 KiB before changing profile settings. It
accepts one Interface and one Peer with PrivateKey, Address, PublicKey, AllowedIPs
and Endpoint. Optional DNS, MTU, PresharedKey and PersistentKeepalive are supported;
`off` means zero keepalive. Comments, whitespace, CRLF, repeated list fields and
IPv6 endpoints are handled. Unknown options, duplicate scalar fields/peers, bad
keys/IP prefixes/ports/numbers, embedded NULs and values exceeding the existing
NVRAM field capacities are rejected. Each router client profile represents one
peer, so multi-peer files require separate supported profiles. HTTP upload units
must be numeric and within 1–5; failure cannot reuse an earlier success status.

Seventeen actual ARM `libshared` cases pass, including zero profile writes on
rejection and complete import of a list longer than 255 bytes. Browser tests
upload empty/malformed/multiple-peer files and invalid units, then verify the
prior keys, address and routes survive and HTTP remains responsive. Evidence:
`logs/vpn-dev11-wg-import.log`, `logs/vpn-dev11-browser.log`. This establishes
preservation on parsing/validation failure, not power-loss-safe NVRAM transactions.

```sh
docker run --rm --network none --read-only --ulimit core=0 --tmpfs /tmp:exec,size=512m \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-test:58138 sh /tests/run-wg-import-test.sh
```


## WireGuard configuration and encrypted lifecycle

Dev12 preserves an explicit PersistentKeepalive value of `0`/`off` in the generated
client configuration. The vendor writer previously changed it to 25; the existing
25-second default still applies when no value is set. The importer also accepts
unbracketed IPv6 endpoints, using the last colon as the port separator just as
`wg setconf` does. The host and port remain validated. Dev11 failures are recorded
in `logs/vpn-dev11-wg-config-regression.log`.

`tests/run-wg-tunnel-test.sh` runs entirely in an isolated network-none container.
The actual image ARM `wg` generates keys; native public-key derivation agrees.
The actual ARM shared importer loads a synthetic provider profile. The fixture
extracts and compiles the exact `_wg_resolv_ep`, `_wg_client_gen_conf` and
`_wg_server_gen_conf` functions from the corresponding prepared source, along
with their `trim_r` helper. Source and image-library hashes are logged. Custom
scripts are disabled in synthetic NVRAM. This tests those source functions with
image libraries; it does not execute the packaged rc service-start sequence.

Native `wg`, compiled offline from the supplied SDK source into disposable storage,
bridges QEMU's unsupported netlink boundary. Crypto runs in the already-loaded
host WireGuard kernel. No global module, binfmt or physical-interface changes are
needed. A second isolated namespace holds the peer. Actual ARM `libovpn` installs
the policy and kill-switch rules through native iproute2/iptables adapters.

Dev12 passes:

- Numeric IPv4 and bracketed/unbracketed IPv6 endpoint import, source resolution,
  config writing and native applied endpoint checks.
- Explicit `0`/`off`, explicit 25 and omitted keepalive values in applied WG state.
- Encrypted LAN payload forwarding with no plaintext marker on transport or WAN.
- Peer outage and return; interface removal with library policy cleanup and IPv4
  kill-switch blocking; wrong-PSK handshake rejection; restored encrypted forwarding.
- Eighteen standalone ARM importer cases, including unbracketed IPv6 and zero
  profile writes for rejected imports.

Evidence: `logs/vpn-dev12-wg-tunnel.log`, `logs/vpn-dev12-wg-import.log`,
`logs/vpn-dev12-network.log` and `logs/vpn-dev12-browser.log`.
IPv6 endpoint parsing is not IPv6 policy protection. The previously demonstrated
IPv6 policy escape remains unresolved. Router-kernel WireGuard, full rc lifecycle,
DNS proxy integration, boot/persistence and hardware tests remain outstanding.

```sh
docker run --rm --network none --read-only --ulimit core=0 \
  --cap-add NET_ADMIN --cap-add SYS_ADMIN \
  --sysctl net.ipv4.ip_forward=1 \
  --sysctl net.ipv4.conf.all.rp_filter=0 --sysctl net.ipv4.conf.default.rp_filter=0 \
  --sysctl net.ipv6.conf.all.forwarding=1 \
  --tmpfs /etc/iproute2 --tmpfs /tmp:exec,dev,size=512m \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount type=bind,src=/absolute/path/to/matching/asuswrt,dst=/work,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-network-test:58138 sh /tests/run-wg-tunnel-test.sh
```


## WireGuard server export defaults

Dev13 also fixes two failures reproduced against dev12 in
`logs/vpn-dev12-wg-export-regression.log`. With an empty peer route list, the
source exporter wrote `AllowedIPs = /32` for a bare server IP and produced no
profile for CIDR or dual-stack server addresses. It now exports a /32 or /128
host route to each server address. Explicit peer route lists are preserved.

An exported profile with server keepalive set to zero correctly omits
PersistentKeepalive. Native WireGuard applies that as disabled, but the importer
previously replaced the missing value with the router's 25-second UI default.
Dev13 imports an omitted keepalive as zero. Explicit values are retained, and
an unset router NVRAM value still uses the existing 25-second writer default.
This supersedes the omitted-import behavior tested in dev12 above.

`tests/run-wg-export-test.sh` checks the exact source exporter, actual image ARM
importer and native applied WG settings for explicit routes, omitted keepalive,
bare server addresses, CIDR addresses and dual-stack addresses. It uses the same
container mounts/capabilities as the WG tunnel test; SYS_ADMIN is unnecessary.
The extended tunnel test also connects using the source-generated server export.
Only Address/DNS directives, which belong to wg-quick, are removed before native
setconf; the fixture supplies addressing/routing. This does not test desktop
wg-quick DNS integration or packaged rc startup.

All five export cases and the expanded encrypted tunnel suite pass on dev13.
Evidence: `logs/vpn-dev13-wg-export.log`, `logs/vpn-dev13-wg-tunnel.log`,
`logs/vpn-dev13-wg-import.log` and `logs/vpn-dev13-config.log`. Source, image,
linkage and official userspace-validator evidence is in the other
`logs/vpn-dev13-*` files. The IPv6 policy and hardware limitations above remain.
