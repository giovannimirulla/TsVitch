#!/bin/bash
set -e

BUILD_DIR=cmake-build-switch

# cd to tsvitch
cd "$(dirname $0)/.."
git config --global --add safe.directory `pwd`

BASE_URL="https://github.com/xfangfang/wiliwili/releases/download/v0.1.0/"

PKGS=(
    "switch-libass-0.17.1-1-any.pkg.tar.zst"
    "switch-ffmpeg-6.1-5-any.pkg.tar.zst"
    "switch-libmpv-0.36.0-2-any.pkg.tar.zst"
    "switch-nspmini-48d4fc2-1-any.pkg.tar.xz"
    "hacBrewPack-3.05-1-any.pkg.tar.zst"
)
for PKG in "${PKGS[@]}"; do
    [ -f "${PKG}" ] || curl -LO ${BASE_URL}${PKG}
    dkp-pacman -U --noconfirm ${PKG}
done

# get the analytics id and key
GA_ID=$(grep -oP '(?<=GA_ID=).*' .env)
GA_KEY=$(grep -oP '(?<=GA_KEY=).*' .env)
if [ -z "${GA_ID}" ] || [ -z "${GA_KEY}" ]; then
    echo "GA_ID or GA_KEY not found in .env file"
    exit 1
fi

AD_SERVER_URL=$(grep -oP '(?<=AD_SERVER_URL=).*' .env)
if [ -z "${AD_SERVER_URL}" ]; then
    echo "AD_SERVER_URL not found in .env file"
    exit 1
fi

cmake -B ${BUILD_DIR} \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILTIN_NSP=ON \
  -DPLATFORM_SWITCH=ON \
  -DBRLS_UNITY_BUILD=ON \
  -DCMAKE_UNITY_BUILD_BATCH_SIZE=16 \
  -DANALYTICS=ON \
  -DANALYTICS_ID="${GA_ID}" \
  -DANALYTICS_KEY="${GA_KEY}" \
  -DAD_SERVER_URL="${AD_SERVER_URL}" \

make -C ${BUILD_DIR} tsvitch.nro -j$(nproc)