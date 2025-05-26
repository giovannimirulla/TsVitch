GA_ID=$(grep '^GA_ID=' .env | cut -d'=' -f2-)
GA_KEY=$(grep '^GA_KEY=' .env | cut -d'=' -f2-)
if [ -z "${GA_ID}" ] || [ -z "${GA_KEY}" ]; then
    echo "GA_ID or GA_KEY not found in .env file"
    exit 1
fi

SERVER_URL=$(grep '^SERVER_URL=' .env | cut -d'=' -f2-)
if [ -z "${SERVER_URL}" ]; then
    echo "SERVER_URL not found in .env file"
    exit 1
fi

SERVER_TOKEN=$(grep '^SERVER_TOKEN=' .env | cut -d'=' -f2-)
if [ -z "${SERVER_TOKEN}" ]; then
    echo "SERVER_TOKEN not found in .env file"
    exit 1
fi

M3U8_URL=$(grep '^M3U8_URL=' .env | cut -d'=' -f2-)
if [ -z "${M3U8_URL}" ]; then
    echo "M3U8_URL not found in .env file"
    exit 1
fi

cmake -B build -DCPR_USE_SYSTEM_CURL=ON \
  -DCPR_USE_BOOST_FILESYSTEM=ON \
  -DCURL_INCLUDE_DIR=/opt/homebrew/opt/curl/include \
  -DCURL_LIBRARY=/opt/homebrew/opt/curl/lib/libcurl.dylib \
  -DBOOST_ROOT=/opt/homebrew/opt/boost \
  -DBoost_NO_SYSTEM_PATHS=ON \
  -DPLATFORM_DESKTOP=ON \
  -DANALYTICS=ON \
  -DANALYTICS_ID="${GA_ID}" \
  -DANALYTICS_KEY="${GA_KEY}" \
  -DSERVER_URL="${SERVER_URL}" \
    -DSERVER_TOKEN="${SERVER_TOKEN}" \
  -DM3U8_URL="${M3U8_URL}" \
  

make -C build TsVitch -j$(sysctl -n hw.ncpu)