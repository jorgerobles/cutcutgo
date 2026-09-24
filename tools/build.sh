#!/usr/bin/env bash
# Build the cutcutgo firmware using the Docker toolchain image.
#
#   ./tools/build.sh              # build both configs + UF2
#   ./tools/build.sh cutcutgo     # standalone config only
#   ./tools/build.sh cutcutgo_bl  # bootloader-layout app only
#   ./tools/build.sh uf2          # regenerate the BL-app UF2 only
#   ./tools/build.sh test         # host simulator test suite (Unity, gcc)
#   ./tools/build.sh clean|clean-all
#
# Requires: docker. Artifacts land in dist/ (host repo, bind-mounted) owned
# by the invoking user (container runs with the host uid/gid).
set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE=cutcutgo-builder:latest

# Build image if missing (downloads XC32 + DFP on first run).
if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    echo "[build.sh] Building toolchain image ${IMAGE} (first run downloads XC32 ~300MB)..."
    docker build -f tools/docker/Dockerfile -t "${IMAGE}" tools/docker
fi

run() {
    docker run --rm -v "${PWD}":/work -w /work -u "$(id -u):$(id -g)" "${IMAGE}" \
        make -f tools/Makefile.firmware "$@"
}

# Simulator tests run from the repo root (the Makefile paths are repo-root
# relative).
run_sim() {
    docker run --rm -v "${PWD}":/work -w /work -u "$(id -u):$(id -g)" "${IMAGE}" \
        make -f tools/sim/Makefile.sim "$@"
}

case "${1:-all}" in
    clean|clean-all|all|cutcutgo|cutcutgo_bl|uf2) run "$@" ;;
    test|test-sim) run_sim "$@" ;;
    *)
        echo "usage: $0 [all|cutcutgo|cutcutgo_bl|uf2|test|test-sim|clean|clean-all]" >&2
        exit 1
        ;;
esac
