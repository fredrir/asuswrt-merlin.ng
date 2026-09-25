# RT-BE90U flash and recovery readiness

As of 2026-09-26, nothing has been flashed or changed on the router. This is a
reviewable preparation record, not a verified recovery procedure or permission
to reboot, reset or flash. The development image is still not ready for that step.

## Artifacts

| Artifact | Identity |
| --- | --- |
| Experimental candidate | `TUF-BE9400_3.0.0.6_102_58138-rtbe90u-dev6-vpn.trx` |
| Candidate size / SHA-256 | 59,007,949 bytes / `320e3c5595abbb8a7877ba0f292595ed883bb64743f94bc66b0fb9c17778f79f` |
| Stock recovery archive | `FW_RT-BE90U_300610258500.zip` |
| Stock archive SHA-256 | `05a5c8227f3ecc0724787bb333728d55449c6984a823f5143108ce649388df75` |
| Extracted stock image SHA-256 | `2eb672afc3903cd664d569587c13113dcfc4c1b026acbc9cc8d7fb09441c502a` |
| Hardware identity | `productid=TUF-BE9400`, `odmpid=RT-BE90U` |
| Observed linux volume | `0x0500b000` bytes; candidate fits |

Artifacts are under `/home/fredrir/projects/rt-be90u-port` on `archie`:
the candidate in `experimental-vpn-dev6-58138/asuswrt/release/src-qca-ipq53xx/image`,
stock ZIP in `downloads`, and stock image/rootfs in `image-audit-official-58500`.
Use the versioned BE9400 filename; the vendor's generic symlink selects BE6500.

## What has been established

The candidate passes strict local model, uImage structure, CRC, payload length,
load/entry address and observed volume-size checks. Its extracted runtime has
791 AArch64 ELFs and the same 11 coprocessor firmware objects as the extension
baseline. Runtime library dependencies resolve. The firmware source configuration
has `RTCONFIG_DUAL_TRX`, `RTCONFIG_DUAL_TRX2` and `RTCONFIG_TEMPROOTFS` enabled;
`RTCONFIG_FITFDT`, `RTCONFIG_PIPEFW` and `RTCONFIG_SECUREBOOT` are disabled.

The official 58500 image was extracted locally from the checksum-verified ASUS
ZIP. Its actual `libshared.so` (`7415ffec462705b6bc20bf9b875021cb21cdc9d6ed86669ce6eee576a0199522`)
was run under QEMU with synthetic model/version NVRAM. `check_imageheader()`
accepts the dev6 candidate and reports the exact size; `check_imagefile()` returns
0. Bad magic is rejected, and payload corruption is rejected. A checksum-correct
wrong-model header is accepted by these isolated calls. Therefore these calls
do **not** establish model/ODM enforcement or live upload acceptance. Keep the
strict local `check-image.py` check; do not generalize these results to other models.

Reproduce against either an extracted stock or candidate filesystem:

```sh
docker run --rm --network none --read-only --ulimit core=0 --tmpfs /tmp:exec,size=512m \
  --mount type=bind,src=/absolute/path/to/rootfs,dst=/firmware,readonly \
  --mount type=bind,src=/absolute/path/to/versioned-BE9400.trx,dst=/candidate,readonly \
  --mount "type=bind,src=$PWD/tools/rt-be90u/tests,dst=/tests,readonly" \
  rt-be90u-test:58138 sh /tests/run-image-runtime-test.sh
```

Evidence: `logs/vpn-dev6-official-validator.log`. No upload endpoint or flash
utility is called, and no router NVRAM is accessed by this test.

## Dual images and firmware version

In the verified GPL baseline, `release/src/router/rc/services.c` contains
`select_upgrade_fw_order()` and the normal upgrade path. The latter writes the
inactive partition and then the selected active partition. `DUAL_TRX2` changes
the order if the secondary image was used to boot. The normal upgrade must
therefore be treated as potentially overwriting **both** `linux` and `linux2`;
the second volume is not a retained stock rollback guarantee.

The runtime's source-level secure-boot setting does not prove the installed
bootloader's signature/fuse policy. CRCs are integrity checks, not signatures.
Bootloader acceptance and power-loss behavior remain unverified. Do not replace
the bootloader, modify Factory volumes, or bypass a rejected firmware check.

The candidate uses GPL baseline 58138, older than installed 58500. Runtime
validator acceptance under synthetic 58500 NVRAM does not establish downgrade
safety, settings compatibility, or inclusion of 58500 security fixes.

## Recovery preparation and approval boundary

ASUS lists Firmware Restoration **2.1.0.3** for this model, with Windows support
and ZIP SHA-256 `474e81da30e3ccf419e2ffab27f7c2309017bbf1472ada85bf15c8faa411a30f`.
The tool has not been installed or tested here. [RT-BE90U downloads](https://www.asus.com/us/networking-iot-servers/wifi-routers/asus-wifi-routers/asus-rt-be90u/helpdesk_download?model2Name=ASUS-RT-BE90U)

The user confirmed physical access and Ethernet on 2026-09-26. A computer with
a usable recovery tool, a maintenance window and alternate connectivity still
need confirmation before requesting a flash. Save the current settings through ASUS's backup UI and keep that
credential-bearing backup private; no backup was collected by this work. Keep
the stock image and its verified hash locally available without Internet access.
Record the current computer network settings so they can be restored afterwards.

The proposed recovery workflow, to be performed only after explicit approval,
is a direct Ethernet connection, a static computer address `192.168.1.10/24`,
and holding Reset while applying router power until the power LED flashes slowly.
ASUS's utility then uploads the extracted stock image in rescue mode. Wait for
completion and reboot before restoring the computer's network settings. This
is ASUS's generic procedure, illustrated with another model; the button/LED
behavior and transfer must still be verified on this RT-BE90U. [ASUS rescue-mode instructions](https://www.asus.com/support/faq/1000814/)

Entering rescue mode already interrupts the active router and requires approval.
A recovery rehearsal, any reset, firmware upload, configuration restoration and
reboot must be explicitly scoped before execution. No raw `mtd-write` command is
part of the proposed user procedure.

## Outstanding readiness work

IPv6 traffic currently bypasses the imported IPv4 VPN kill switches. Real
daemon validation found that OpenVPN 2.4.12 rejects the imported configuration's
`data-ciphers` option; updating the daemon is required. Real
OpenVPN/WireGuard handshakes and lifecycle, router DNS-proxy behavior, complete
firewall interaction and service ordering remain unverified. General stock VPN
Fusion/SDN migration, certificate regeneration/export and remaining peer/import
UI paths, add-on APIs, physical USB, and 3006 source/CI integration are unfinished.
The inspected router has no VPN assignments requiring migration today.

After those software issues are addressed and recovery access is confirmed,
prepare an explicit hardware test covering Ethernet/VLANs, all radios/MLO,
IPv6, USB/JFFS hooks, VPN enforcement, LEDs/buttons, reboot persistence and
throughput. Keep ECM acceleration disabled until routing/filtering correctness
has been established on hardware. A supported release remains a later milestone.
