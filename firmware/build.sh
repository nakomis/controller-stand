#!/usr/bin/env bash
# Build the firmware (or pass any idf.py command through).
#   firmware/build.sh                       idf.py build, then sign it (sign.sh)
#   firmware/build.sh flash monitor         anything else idf.py takes (don't flash with idf.py:
#                                           use flash.sh, which writes the signed image and prov)
# A build signs its image when ota-signing/{env}.pub.pem exists (Fleet OTA): an unsigned image boots,
# but then refuses every update from Fleet, since an update must be signed like the running image.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
cd "$here"
IDF_PATH="${IDF_PATH:-$HOME/esp/v5.5/esp-idf}"
# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" > /dev/null
dir=build
if [ $# -gt 0 ]; then
    exec idf.py "$@"
fi
idf.py build
if [ -s "$here/ota-signing/${NPM_ENVIRONMENT:-sandbox}.pub.pem" ]; then
    "$here/sign.sh" "$here/$dir/grogu.bin"
else
    echo "warning: no ota-signing/${NPM_ENVIRONMENT:-sandbox}.pub.pem, so $dir/grogu.bin is unsigned and will refuse every update" >&2
fi
