# Experimental RT-BE90U integration on 3006

This branch makes the RT-BE90U firmware build from checked-out repository
sources. It is based on `master-3006` at
`920b77f5f92db14717a27abd5c8e1b06ae6c8ec1`. It imports the previously tested
dev14 port (`0dd6c693e2f12bebde29d38b1cd6b0934cce5cad`) and labels this first
native build `58138-rtbe90u-dev15-vpn`.

This is a proposed platform layout for maintainer review. It does not add the
model to the supported-model list or release workflow. Recovery and hardware
validation have not been performed; the image is not ready to flash.

## Source ownership and layout

| Location | Build role and provenance |
| --- | --- |
| `release/src/` | 68,199 existing files reused directly, with no changes to the upstream common tree |
| `release/src-qca-ipq53xx/linux`, `qca`, and other platform directories | 68,029 vendor platform inputs from ASUS GPL TUF-BE9400 3.0.0.6.102.58138 |
| `release/src-qca-ipq53xx/source-overlay/release/src/` | 13,923 differing or additional common-tree inputs, including QCA-specific rc/shared/httpd integration, package versions and UI |
| `release/src-qca-ipq53xx/vendor/` | SDK build glue and auxiliary sources needed to reconstruct the SDK layout |
| `release/src-qca-ipq53xx/sources.json` | Explicit common/override/platform file lists, SDK aliases, required empty directories and import provenance |
| `tools/rt-be90u/` | Source assembly, builder provisioning, image/linkage checks and offline runtime fixtures |

The original SDK archive is `GPL_TUF_BE9400_300610258138.zip`, SHA-256
`930142d207b1f9dfc11e2ad17c767abc59a5df5777223dfc99f12a69f1d07b9e`.
The import script verifies the archive and extracted inputs before copying.
The raw vendor import and the applied port are separate commits. The recorded
`dev14-*` manifests describe the imported baseline, before the dev15 version
change; subsequent integration changes are recorded in Git.

The overrides avoid replacing Broadcom sources with the older QCA SDK. They
also create maintenance work: fixes to an overridden common file do not flow
into this platform automatically. Shared rc, networking and UI changes need
consolidation with appropriate QCA conditionals after agreement on this layout.
Large package differences include curl, strongSwan, wget, libxml2 and iproute2.
These versions need their own compatibility/security review. This branch has
not established a supported update path for all vendor packages.

The SDK contains proprietary Wi-Fi/NSS components and other prebuilt vendor
libraries/tools. Sources are not available for all of these. Preserve their
existing notices and review redistribution and update responsibilities before
merging. This work does not establish license clearance. U-Boot source is not
imported into this firmware target; no bootloader build or update is exposed.

## Build

Use an x86-64 Linux host, Docker, Python 3.8 or newer and sufficient disk space
for both repository inputs and a private build copy. A full checkout contains
all required firmware inputs. A sparse checkout must include every common file
listed in `sources.json`; `make check` fails if one is missing. No SDK archive,
patch application, second Merlin checkout or Git fetch is needed to build.

The SDK archive's toolchain entries are Git LFS pointers. The toolchain currently
used is `openwrt-gcc750_musl1124.aarch64` from the existing
`SWRT-dev/qca-toolchains` checkout at
`5e5b8ae593edb9c3f0711c56ef7fe73e113506d0`. Its compiler targets
`aarch64-openwrt-linux-musl`, GCC 7.5.0/musl 1.1.24. Equivalence to ASUS's
unavailable LFS objects has not been established. The toolchain is an external,
pinned build dependency, not a firmware source dependency.

Provision host packages once (this step requires package access):

```sh
docker build -f tools/rt-be90u/Dockerfile.deps \
  -t rt-be90u-build-deps:20.04 tools/rt-be90u
```

Then create the builder from the pinned, already available toolchain checkout:

```sh
tools/rt-be90u/prepare-toolchain.sh /path/to/qca-toolchains
```

The toolchain script checks the revision and cleanliness, exports Git objects
only, requires a local dependency image, and disables network during assembly.
An optional third argument selects an existing compatible Ubuntu 20.04 builder
as the dependency image. The local validation used the cached
`rt-be90u-build:58138` image, ID
`sha256:74cad8c3c730d9bd59d2618f9b3d70eb315ae7b25f089e5d8a812e0530575461`,
to create `rt-be90u-build:3006` offline. Reprovisioning all OS packages from
scratch was not validated offline: the Ubuntu apt layer was unavailable.

From the repository root:

```sh
make -C release/src-qca-ipq53xx check
docker run --rm --network none --ulimit core=0 \
  --user "$(id -u):$(id -g)" \
  --mount "type=bind,src=$PWD,dst=/work" \
  rt-be90u-build:3006 make rt-be90u
```

The native entry point is `make -C release/src-qca-ipq53xx rt-be90u`. The builder
already sets that working directory. The existing Broadcom entry points and
CI matrix are unchanged.

`prepare-native.py` assembles a private SDK tree in
`release/src-qca-ipq53xx/.build/source`. Normal copies keep generated writes
away from checked-in inputs. Inputs in one staging batch receive the same
mtime, preventing artificial stamp/header dependencies caused by copy order.
Repository edits are copied with a fresh timestamp; unchanged build-generated
files are retained on incremental runs. Removed inputs require `make clean`.
A lock serializes preparation, building and cleanup. Vendor Git probes cannot
walk out of the disposable tree into this checkout.

The private SDK invokes the existing `tuf-be6500` board profile required by this
hardware. Packaging must retain product ID `TUF-BE9400`; the device's ODM ID is
`RT-BE90U`. The wrapper checks the exact versioned BE9400 image, CRCs,
architecture, load/entry addresses and observed volume limit before collecting
it in `release/src-qca-ipq53xx/image/`. These checks do not establish flash
compatibility. Do not use the vendor's generic image symlink.

For a clean rebuild:

```sh
make -C release/src-qca-ipq53xx clean
```

This removes the private build tree and retains collected versioned images.
Preserve a completed candidate and choose a new development version before
making further firmware changes. Identical firmware bytes across rebuilds
are not promised: vendor packaging includes timestamps and generated metadata.

## Validation and remaining integration work

Before the dev15 version change, all 150,610 assembled inputs matched the
independent fresh dev14 preparation in contents, executable mode and symlink
target, including all 599 port inputs. Helper tests cover image/linkage checks,
HTML parsing, source isolation, missing inputs, aliases, removed inputs,
path traversal and source timestamp ordering:

```sh
python3 -m unittest discover -s tools/rt-be90u/tests -p 'test_*.py'
```

Offline ARM fixtures execute packaged programs under QEMU and use the host
kernel in isolated Docker networking. They do not reproduce the router kernel,
rc boot orchestration, Wi-Fi, hardware acceleration or persistent NVRAM. The
IPv6 guard blocks entire protected interfaces even while the IPv4 tunnel is
up; it does not route IPv6 through VPNs or protect IPv6 identities selected
only by IPv4 device rules.

Next integration work includes maintainer agreement on source ownership,
consolidation of common-code overrides, other-model build checks, reproducible
builder provisioning, CI/artifact/release wiring, and completion of runtime
service/migration/add-on support. Recovery rehearsal and controlled hardware
validation remain separate prerequisites for a support PR.
