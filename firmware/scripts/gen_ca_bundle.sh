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
GEN_URL="https://raw.githubusercontent.com/espressif/esp-idf/release/v5.1/components/mbedtls/esp_crt_bundle/gen_crt_bundle.py"

pip install --quiet cryptography
mkdir -p data/cert
curl -fsSL "$CACERT_URL" -o cacert.pem
curl -fsSL "$GEN_URL"    -o gen_crt_bundle.py
python gen_crt_bundle.py -i cacert.pem -o data/cert/x509_crt_bundle.bin
ls -l data/cert/x509_crt_bundle.bin
