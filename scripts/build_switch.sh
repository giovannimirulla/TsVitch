
set -e

BUILD_DIR=cmake-build-switch


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


GA_ID=$(grep -oP '(?<=GA_ID=).*' .env)
GA_KEY=$(grep -oP '(?<=GA_KEY=).*' .env)
if [ -z "${GA_ID}" ] || [ -z "${GA_KEY}" ]; then
    echo "GA_ID or GA_KEY not found in .env file"
    exit 1
fi

SERVER_URL=$(grep -oP '(?<=SERVER_URL=).*' .env)
if [ -z "${SERVER_URL}" ]; then
    echo "SERVER_URL not found in .env file"
    exit 1
fi

SERVER_TOKEN=$(grep -oP '(?<=SERVER_TOKEN=).*' .env)
if [ -z "${SERVER_TOKEN}" ]; then
    echo "SERVER_TOKEN not found in .env file"
    exit 1
fi

M3U8_URL=$(grep -oP '(?<=M3U8_URL=).*' .env)
if [ -z "${M3U8_URL}" ]; then
    echo "M3U8_URL not found in .env file"
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
  -DSERVER_URL="${SERVER_URL}" \
    -DSERVER_TOKEN="${SERVER_TOKEN}" \
  -DM3U8_URL="${M3U8_URL}" 

make -C ${BUILD_DIR} TsVitch.nro -j$(nproc)