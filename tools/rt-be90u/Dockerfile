FROM ubuntu:20.04

RUN dpkg --add-architecture i386 \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        autoconf automake autopoint bc binutils-dev bison build-essential \
        bzip2 ca-certificates cmake cpio device-tree-compiler dos2unix \
        file flex gawk gengetopt gettext git gperf gtk-doc-tools intltool \
        lib32stdc++6 lib32z1 libc6-i386 libelf1:i386 libglib2.0-dev \
        libltdl-dev liblzma-dev liblzo2-dev libncurses5-dev libncurses5:i386 \
        libssl-dev libtool-bin libxml2-utils libyaml-dev lzma m4 \
        mtd-utils patch perl pkg-config python2 python3 rsync \
        sharutils shtool squashfs-tools texinfo u-boot-tools unzip \
        uuid-dev wget xsltproc xz-utils zlib1g-dev \
    && ln -s /usr/bin/python2 /usr/local/bin/python \
    && rm -rf /var/lib/apt/lists/*

# ASUS's 58138 archive contains LFS pointers for its toolchains.
# Supply an unpacked toolchain; provenance is recorded in docs/rt-be90u.md.
COPY openwrt-gcc750_musl1124.aarch64 /opt/openwrt-gcc750_musl1124.aarch64
RUN /opt/openwrt-gcc750_musl1124.aarch64/bin/aarch64-openwrt-linux-musl-gcc --version

ENV LC_ALL=C
ENV PATH=/opt/openwrt-gcc750_musl1124.aarch64/bin:$PATH
WORKDIR /work/release/src-qca-ipq53xx
CMD ["make", "tuf-be6500"]
