#!/usr/bin/env bash
# Metal Mutant port — one-shot setup & build for the Miyoo Mini (Allium CFW).
#
#   ./build-miyoo-allium.sh [Release|Debug] [--clean]
#
# Clones the pinned upstream ALIS engine, drops in the mmx + mmx_miyoo
# modules, applies the patch series, cross-compiles with the local Miyoo
# toolchain (../toolchain), and assembles a ready-to-install Allium package:
#
#   dist/miyoo-allium/MetalMutant.pak
#
# Game data files are NOT distributed with this repo — copy your own
# Metal Mutant .IO files into the package's data/ folder.
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build-miyoo-allium}"
CONFIG="Release"
CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=1 ;;
        *) CONFIG="$arg" ;;
    esac
done
DIST_DIR="${DIST_DIR:-dist}"
PACKAGE_NAME="MetalMutant.pak"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ALIS_DIR="${ROOT_DIR}/build/alis"
TOOLCHAIN_DIR="$(cd "${ROOT_DIR}/../toolchain" && pwd)"
TOOLCHAIN_ROOT="${TOOLCHAIN_DIR}/sdk/miyoomini-toolchain"
SDK_ARCHIVE="${TOOLCHAIN_DIR}/sdk/miyoomini-toolchain.tar.xz"
SDK_TARGET_ROOT="${TOOLCHAIN_ROOT}/arm-linux-gnueabihf"
SDK_SYSROOT="${TOOLCHAIN_ROOT}/arm-linux-gnueabihf/libc"
COMPAT_ROOT="${TOOLCHAIN_DIR}/sdk/compat"
HOST_BINUTILS_LIB="${COMPAT_ROOT}/usr/lib/x86_64-linux-gnu"

GIT_IDENT=(-c user.name="Metal Mutant Setup" -c user.email="setup@localhost")

ensure_toolchain() {
    if [[ ! -x "${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-gcc" ]]; then
        echo "ERROR: Miyoo toolchain is not installed. Run ${TOOLCHAIN_DIR}/install-local.sh first." >&2
        exit 1
    fi

    if [[ ! -x "${TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-c++" ]]; then
        echo "ERROR: Missing SDK C++ compiler: ${TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-c++" >&2
        exit 1
    fi
}

ensure_cpp_runtime() {
    local cxx_include_root="${SDK_TARGET_ROOT}/include/c++/8.3.0"
    local libstdcxx_root="${SDK_TARGET_ROOT}/lib"

    if [[ -d "${cxx_include_root}" && -f "${libstdcxx_root}/libstdc++.so" ]]; then
        return
    fi

    if [[ ! -f "${SDK_ARCHIVE}" ]]; then
        echo "ERROR: Missing Miyoo SDK archive: ${SDK_ARCHIVE}" >&2
        exit 1
    fi

    echo "Restoring GCC 8 C++ runtime from the Miyoo SDK archive..."
    tar -xf "${SDK_ARCHIVE}" -C "${TOOLCHAIN_DIR}/sdk" \
        miyoomini-toolchain/arm-linux-gnueabihf/include/c++ \
        miyoomini-toolchain/arm-linux-gnueabihf/lib \
        miyoomini-toolchain/arm-linux-gnueabihf/libc/lib \
        miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/lib
}

# Upstream engine at the pinned commit + engine modules + patch series.
# Mirrors scripts/setup.ps1 (the Windows flow).
setup_engine() {
    local upstream_url upstream_sha
    upstream_url=$(grep -v '^#' "${ROOT_DIR}/UPSTREAM" | sed -n 1p | awk '{print $1}')
    upstream_sha=$(grep -v '^#' "${ROOT_DIR}/UPSTREAM" | sed -n 2p | awk '{print $1}')

    if [[ ! -d "${ALIS_DIR}/.git" ]]; then
        echo "Cloning ${upstream_url} ..."
        git clone "${upstream_url}" "${ALIS_DIR}"
    fi

    # a deterministic tree on every run: pinned commit, engine drop-ins, patches
    git -C "${ALIS_DIR}" "${GIT_IDENT[@]}" am --abort 2>/dev/null || true
    git -C "${ALIS_DIR}" reset --hard --quiet
    git -C "${ALIS_DIR}" clean -fdq
    if ! git -C "${ALIS_DIR}" checkout --detach --quiet "${upstream_sha}" 2>/dev/null; then
        git -C "${ALIS_DIR}" fetch --quiet origin
        git -C "${ALIS_DIR}" checkout --detach --quiet "${upstream_sha}"
    fi

    cp "${ROOT_DIR}/engine/"* "${ALIS_DIR}/src/"
    cp "${ROOT_DIR}/assets/alis32.ico" "${ALIS_DIR}/src/icons/"

    local patch
    for patch in "${ROOT_DIR}/patches/"*.patch; do
        if ! git -C "${ALIS_DIR}" "${GIT_IDENT[@]}" am --3way "${patch}"; then
            git -C "${ALIS_DIR}" "${GIT_IDENT[@]}" am --abort
            echo "ERROR: Patch failed to apply: $(basename "${patch}"). Update the patch series for commit ${upstream_sha}." >&2
            exit 1
        fi
    done
    echo "Applied patch series on ${upstream_sha}"
}

reset_incompatible_build_dir() {
    local cache_path="${ROOT_DIR}/${BUILD_DIR}/CMakeCache.txt"

    if [[ ! -f "${cache_path}" ]]; then
        return
    fi

    if ! grep -Fq "ALIS_MIYOO_ALLIUM:BOOL=ON" "${cache_path}" || \
       ! grep -Fq "CMAKE_TOOLCHAIN_FILE" "${cache_path}"; then
        echo "Removing incompatible CMake cache at ${ROOT_DIR}/${BUILD_DIR}"
        rm -rf "${ROOT_DIR:?}/${BUILD_DIR}"
    fi
}

echo "========================================"
echo " Metal Mutant - Miyoo Allium Build Script"
echo "========================================"
echo "Configuration: ${CONFIG}"
echo "Toolchain: ${TOOLCHAIN_DIR}"
echo

if [[ ${CLEAN} -eq 1 ]]; then
    rm -rf "${ROOT_DIR:?}/${BUILD_DIR}" "${ALIS_DIR:?}"
fi

ensure_toolchain
ensure_cpp_runtime
reset_incompatible_build_dir

export LD_LIBRARY_PATH="${HOST_BINUTILS_LIB}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export PKG_CONFIG_SYSROOT_DIR="${SDK_SYSROOT}"
export PKG_CONFIG_LIBDIR="${SDK_SYSROOT}/usr/lib/pkgconfig:${SDK_SYSROOT}/usr/share/pkgconfig"

echo "[1/4] Setting up the ALIS engine..."
setup_engine
echo

echo "[2/4] Configuring CMake..."
cmake -B "${ROOT_DIR}/${BUILD_DIR}" -S "${ALIS_DIR}" \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_DIR}/cmake/miyoomini-toolchain.cmake" \
    -DALIS_MIYOO_ALLIUM=ON
echo

echo "[3/4] Building Metal Mutant for Miyoo Allium..."
cmake --build "${ROOT_DIR}/${BUILD_DIR}" --config "${CONFIG}" --parallel
echo

echo "[4/4] Assembling Allium package..."
BINARY="${ROOT_DIR}/${BUILD_DIR}/alis"
if [[ ! -f "${BINARY}" ]]; then
    echo "ERROR: Failed to find built executable: ${BINARY}" >&2
    exit 1
fi

PACKAGE_DEST_ROOT="${ROOT_DIR}/${DIST_DIR}/miyoo-allium"
PACKAGE_DEST="${PACKAGE_DEST_ROOT}/${PACKAGE_NAME}"
rm -rf "${PACKAGE_DEST}"
mkdir -p "${PACKAGE_DEST}/data"

cp "${BINARY}" "${PACKAGE_DEST}/metalmutant"
"${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-strip" "${PACKAGE_DEST}/metalmutant"
cp "${ROOT_DIR}/dist-miyoo/launch.sh" "${PACKAGE_DEST}/launch.sh"
cp "${ROOT_DIR}/dist-miyoo/metal.ini" "${PACKAGE_DEST}/metal.ini"
cp "${ROOT_DIR}/dist-miyoo/README-MIYOO.txt" "${PACKAGE_DEST}/README-MIYOO.txt"
cp -r "${ROOT_DIR}/dist/translations" "${PACKAGE_DEST}/translations"
cat > "${PACKAGE_DEST}/data/PUT-GAME-FILES-HERE.txt" <<'EOF'
Copy your original Metal Mutant game files (Atari ST or DOS version)
into this folder. The set must include MAIN.IO — that is the file the
engine looks for. No game data is distributed with this port.
EOF
chmod +x "${PACKAGE_DEST}/metalmutant" "${PACKAGE_DEST}/launch.sh"

echo
echo "========================================"
echo " Build succeeded!"
echo " Output: ${PACKAGE_DEST}"
echo "========================================"
