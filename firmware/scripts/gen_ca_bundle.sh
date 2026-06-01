#!/usr/bin/env bash
# Generate the embedded root-CA bundle used to validate Bambuddy's TLS
# certificate when the connection scheme is https://.
#
# Run from the firmware/ project directory BEFORE `pio run`. It fetches the
# current Mozilla CA set (curl.se/ca/cacert.pem) plus Espressif's
# gen_crt_bundle.py and writes the binary esp_crt_bundle to
#   data/cert/x509_crt_bundle.bin
# which platformio.ini embeds via board_build.embed_files (symbol
# _binary_data_cert_x509_crt_bundle_bin_start, see src/net/ca_bundle.h).
#
# The bundle is git-ignored and regenerated on every CI build, so the trusted
# roots stay fresh without committing a CA list that would slowly go stale.
set -euo pipefail

CACERT_URL="https://curl.se/ca/cacert.pem"
# Pinned to a tagged esp-idf release (not the moving release/v5.1 branch) so an
# upstream change to gen_crt_bundle.py can't silently break every firmware build.
GEN_URL="https://raw.githubusercontent.com/espressif/esp-idf/v5.1.5/components/mbedtls/esp_crt_bundle/gen_crt_bundle.py"

pip install --quiet cryptography
mkdir -p data/cert
curl -fsSL "$CACERT_URL" -o cacert.pem
curl -fsSL "$GEN_URL"    -o gen_crt_bundle.py
# This gen_crt_bundle.py takes only --input and writes a fixed-name file
# (./x509_crt_bundle) in the cwd — it has no --output flag — so move it into place.
python gen_crt_bundle.py --input cacert.pem
mv x509_crt_bundle data/cert/x509_crt_bundle.bin
ls -l data/cert/x509_crt_bundle.bin
