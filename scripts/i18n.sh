#!/usr/bin/env bash

# cd to TsVitch/resources/i18n
cd "$(dirname "$0")/../resources/i18n" || exit

LANG_PATH="$HOME/Downloads/TsVitch (translations)"

if [ -n "$1" ] ;then
  LANG_PATH=$1
fi

echo "Load language files from: $LANG_PATH"

cp "$LANG_PATH/it/hints.json" ./it
cp "$LANG_PATH/it/tsvitch.json" ./it

echo "Load language files done"

rm -r "$LANG_PATH"