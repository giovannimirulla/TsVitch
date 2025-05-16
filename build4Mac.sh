cmake -B build -DCPR_USE_SYSTEM_CURL=ON \
  -DCPR_USE_BOOST_FILESYSTEM=ON \
  -DCURL_INCLUDE_DIR=/opt/homebrew/opt/curl/include \
  -DCURL_LIBRARY=/opt/homebrew/opt/curl/lib/libcurl.dylib \
  -DBOOST_ROOT=/opt/homebrew/opt/boost \
  -DBoost_NO_SYSTEM_PATHS=ON \
  -DPLATFORM_DESKTOP=ON

make -C build tsvitch -j$(sysctl -n hw.ncpu)