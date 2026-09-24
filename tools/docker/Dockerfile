# CutCutGo firmware build image — XC32 + PIC32MX DFP, no MPLAB X needed.
# Build:  docker build -f tools/docker/Dockerfile -t cutcutgo-builder tools/docker
# Override versions:  --build-arg XC32_VERSION=v4.35 --build-arg DFP_VERSION=1.5.259
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential wget ca-certificates unzip python3 file libc6-i386 \
    && rm -rf /var/lib/apt/lists/*

# XC32 is distributed as a .tar wrapping the .run installer (category DEV).
ARG XC32_VERSION=v4.35
ARG XC32_URL=https://ww1.microchip.com/downloads/aemDocuments/documents/DEV/ProductDocuments/SoftwareTools/xc32-${XC32_VERSION}-full-install-linux-x64-installer.tar
ARG DFP_VERSION=1.5.259
ARG DFP_URL=https://packs.download.microchip.com/Microchip.PIC32MX_DFP.${DFP_VERSION}.atpack

RUN wget -q "${XC32_URL}" -O /tmp/xc32.tar \
    && tar xf /tmp/xc32.tar -C /tmp \
    && chmod +x /tmp/xc32-${XC32_VERSION}-full-install-linux-x64-installer.run \
    && /tmp/xc32-${XC32_VERSION}-full-install-linux-x64-installer.run \
        --mode unattended --unattendedmodeui none --netservername localhost \
    && rm -f /tmp/xc32.tar /tmp/xc32-${XC32_VERSION}-full-install-linux-x64-installer.run

ENV PATH="/opt/microchip/xc32/${XC32_VERSION}/bin:${PATH}"

# PIC32MX Device Family Pack (linker scripts + SFR headers used by -mdfp).
RUN wget -q "${DFP_URL}" -O /tmp/dfp.zip \
    && mkdir -p /opt/packs/Microchip/PIC32MX_DFP/${DFP_VERSION} \
    && unzip -q /tmp/dfp.zip -d /opt/packs/Microchip/PIC32MX_DFP/${DFP_VERSION} \
    && rm -f /tmp/dfp.zip

ENV DFP_DIR=/opt/packs/Microchip/PIC32MX_DFP/${DFP_VERSION}

WORKDIR /work
CMD ["make", "-f", "tools/Makefile.firmware", "all"]
