docker run --platform linux/amd64 --rm -v $(pwd):/data devkitpro/devkita64:20240324 \
  bash -c "/data/scripts/build_switch.sh"