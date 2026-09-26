# RT-BE90U flash and recovery readiness

As of 2026-09-26, nothing has been flashed or changed on the router. This is a
reviewable preparation record, not a verified recovery procedure or permission
to reboot, reset or flash. The development image is still not ready for that step.

## Artifacts

| Artifact | Identity |
| --- | --- |
| Experimental candidate | `TUF-BE9400_3.0.0.6_102_58138-rtbe90u-dev9-vpn.trx` |
| Candidate size / SHA-256 | 59,046,137 bytes / `3a5bf68ade4c3f271a3932ff86ecfe95a83ddfe1f45cf2e0d7b2f5b9e2522183` |
| Stock recovery archive | `FW_RT-BE90U_300610258500.zip` |
| Stock archive SHA-256 | `05a5c8227f3ecc0724787bb333728d55449c6984a823f5143108ce649388df75` |
| Extracted stock image SHA-256 | `2eb672afc3903cd664d569587c13113dcfc4c1b026acbc9cc8d7fb09441c502a` |
| Hardware identity | `productid=TUF-BE9400`, `odmpid=RT-BE90U` |
| Observed linux volume | `0x0500b000` bytes; candidate fits |

Artifacts are under `/home/fredrir/projects/rt-be90u-port` on `archie`:
the candidate in `experimental-vpn-dev9-58138/asuswrt/release/src-qca-ipq53xx/image`,
stock ZIP in `downloads`, and stock image/rootfs in `image-audit-official-58500`.
Use the versioned BE9400 filename; the vendor's generic symlink selects BE6500.

## What has been established

The candidate passes strict local model, uImage structure, CRC, payload length,
load/entry address and observed volume-size checks. Its extracted runtime has
792 AArch64 ELFs and the same 11 coprocessor firmware objects as the extension
baseline. Runtime library dependencies resolve. The firmware source configuration
has `RTCONFIG_DUAL_TRX`, `RTCONFIG_DUAL_TRX2` and `RTCONFIG_TEMPROOTFS` enabled;
`RTCONFIG_FITFDT`, `RTCONFIG_PIPEFW` and `RTCONFIG_SECUREBOOT` are disabled.

The official 58500 image was extracted locally from the checksum-verified ASUS
ZIP. Its actual `libshared.so` (`7415ffec462705b6bc20bf9b875021cb21cdc9d6ed86669ce6eee576a0199522`)
was run under QEMU with synthetic model/version NVRAM. `check_imageheader()`
accepts the dev9 candidate and reports the exact size; `check_imagefile()` returns
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

Evidence: `logs/vpn-dev9-official-validator.log`. No upload endpoint or flash
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

The user confirmed physical access, Ethernet, Linux/macOS computers, and spare
8 GB and 16 GB USB flash drives on 2026-09-26. Windows is unavailable. A tested
recovery tool, a maintenance window and alternate connectivity still need
confirmation before requesting a flash. Neither USB drive has been touched.
Save the current settings through ASUS's backup UI and keep that
credential-bearing backup private; no backup was collected by this work. Keep
the stock image and its verified hash locally available without Internet access.
Record the current computer network settings so they can be restored afterwards.

ASUS's generic guide uses `192.168.1.10/24` on the computer and holds Reset
while applying router power until the power LED flashes slowly. Its example is
a different model, and its utility requires Windows. Do not assume its addresses
or LEDs apply unchanged here. [ASUS rescue-mode instructions](https://www.asus.com/support/faq/1000814/)

### Linux/macOS path from the model's vendor source

The pinned GPL archive includes `release/src-qca-ipq53xx/uboot_v2016.01`.
`README.ASUS` explicitly identifies the TUF-BE9400 target as RT-BE90U.
`include/configs/tuf-be9400.h` defines router address `192.168.50.1/24`, default
computer/server address `192.168.50.75`, and filename `TUF-BE9400.trx`.
`common/Makefile` includes ASUS's `cmd_tftpServer.c` under `CONFIG_ASUS_PRODUCT`;
the disabled generic `CONFIG_CMD_TFTPSRV` setting does not disable this service.

`do_tftpd()` enters rescue on Reset; WPS instead selects a factory reset, so
these buttons must not be confused. The ASUS server listens on UDP 69 and accepts
ordinary TFTP WRQ and 512-byte data blocks, including a wrapping 16-bit block
counter. Its WRQ handler retains the configured peer IP until an RRQ establishes
another peer. A direct computer address of `192.168.50.75/24` therefore matches
the source defaults. An ordinary RRQ returns an empty DATA block and updates
the temporary peer; it does not write flash. No probe or upload has been sent.

The proposed rehearsal is an isolated Ethernet link with saved computer network
settings, followed by an approved power/Reset sequence and a probe of the rescue
service. Verify the address and LED behavior before uploading anything. Then a
binary-mode TFTP client can upload the verified stock image with remote filename
`TUF-BE9400.trx`. Avoid TFTP block-size negotiation; this implementation uses 512
bytes. A final data ACK confirms reception only: the source subsequently checks
and writes the image, sends ASUS-specific completion packets, and reboots.
Allow flashing to finish before changing power or networking. The rescue source
writes both firmware volumes, too.

`archie` already has curl 8.22.0 with TFTP support. The loopback-only
`tests/tftp-client-test.py` test passed an empty-read probe and a complete binary
transfer of the verified stock image using `--tftp-no-options`. It checked every
512-byte block, sequence rollover beyond block 65535, length and final SHA-256.
Evidence: `logs/recovery-tftp-client.log`. This validates the local client against
a protocol fixture, not ASUS bootloader code or hardware.

After approval, with the direct link configured and the router already in rescue,
the following is a **probe only**; its expected result is a zero-byte file:

```sh
curl --silent --show-error --noproxy '*' --proto '=tftp' --tftp-no-options \
  --max-time 5 --output /tmp/rt-be90u-rescue-probe.bin \
  tftp://192.168.50.1/rt-be90u-probe
```

Stop after the probe and record the result. Do not infer a successful flash from
a probe response. Uploading the stock image is a separate approved action.
The source's Reset path only enters the server; WPS performs a factory reset.
If no upload has been started, release Reset and power-cycle normally to leave
rescue. Any actual upload needs time to complete before touching power.

This is source-backed preparation, **not a verified procedure for the installed
bootloader**. Do not build/install a replacement bootloader. The stock recovery
transfer has not been rehearsed. A read-only query returned no `bl_version`
NVRAM value, so the installed bootloader version has not been established.

Entering rescue mode already interrupts the active router and requires approval.
A recovery rehearsal, any reset, firmware upload, configuration restoration and
reboot must be explicitly scoped before execution. No raw `mtd-write` command is
part of the proposed user procedure.

## Proposed probe-only rehearsal

The user requested continued offline work because other processes cannot be
interrupted until the following morning. This rehearsal remains deferred and
unapproved. Time passing does not supply approval. When requested later, it is
separate from installing dev9:

1. Reserve a short outage and keep another Internet connection available for
   coordination. Save the current ASUS settings backup privately. Keep the
   verified stock image locally available.
2. Use a direct Ethernet link from the recovery computer, isolate other router
   links, record that computer's current network settings, and temporarily use
   `192.168.50.75/24` on its Ethernet interface without a gateway.
3. With approval and physical access, power off the router, hold **Reset** while
   powering it on, and release Reset once rescue behavior appears. Do not use
   WPS. Record LED behavior; do not treat an assumed LED pattern as confirmation.
4. Run only the ordinary TFTP read probe above. A zero-byte successful reply is
   the expected result. If it fails, stop and inspect the recorded observations.
   Do not upload firmware, reset settings, or try changing bootloader variables.
5. With no upload attempted, power-cycle normally to return to stock firmware,
   restore computer settings and cables, and verify normal connectivity and the
   stock firmware/model identity.

Entering and leaving rescue causes downtime. This rehearsal verifies access to
the service only; it cannot prove that firmware validation or flashing works.
Firmware installation, USB formatting and configuration restoration are separate
future actions. No step in this rehearsal has been performed on hardware.

## Outstanding readiness work

IPv6 traffic currently bypasses the imported IPv4 VPN kill switches. Dev8 updates
OpenVPN to 2.6.16 and passes encrypted static/TLS/tls-crypt/tls-crypt-v2 forwarding,
disconnect blocking, restart and TLS identity-rejection tests. These run under
QEMU with native networking adapters and synthetic NVRAM, not the router kernel.
Encrypted WireGuard, router DNS-proxy behavior, complete firewall interaction
and rc service ordering remain unverified. General stock VPN Fusion/SDN migration,
certificate regeneration, desktop exported-profile integration,
remaining peer/import UI paths, add-on APIs, physical USB, and 3006 source/CI
integration are unfinished.
The inspected router has no VPN assignments requiring migration today.

After those software issues are addressed and recovery access is confirmed,
prepare an explicit hardware test covering Ethernet/VLANs, all radios/MLO,
IPv6, USB/JFFS hooks, VPN enforcement, LEDs/buttons, reboot persistence and
throughput. Keep ECM acceleration disabled until routing/filtering correctness
has been established on hardware. A supported release remains a later milestone.
