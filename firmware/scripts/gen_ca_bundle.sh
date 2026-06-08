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
# sha256 of the pinned v5.1.5 gen_crt_bundle.py. The tag pin alone isn't enough:
# a moved tag, a compromised CDN, or a GitHub-infra incident could serve a
# tampered generator that quietly weakens the emitted CA bundle. Verify the
# exact bytes and refuse to build otherwise.
GEN_SHA256="e8abcaf9bc077c3e4a18f7c350763eade39b926d90add6b27539ae12ad80eec7"

pip install --quiet cryptography
mkdir -p data/cert
curl -fsSL "$CACERT_URL" -o cacert.pem
curl -fsSL "$GEN_URL"    -o gen_crt_bundle.py
echo "${GEN_SHA256}  gen_crt_bundle.py" | sha256sum -c -
# --- Re-add GlobalSign Root CA (R1) -----------------------------------------
# Mozilla dropped the original 1998 "GlobalSign Root CA" (R1) from its trust
# store, but Google Trust Services still cross-signs its GTS roots with it. A
# Cloudflare endpoint whose edge cert chains  leaf -> GTS WEx -> GTS Root R4 ->
# GlobalSign Root CA (R1)  therefore can't be validated by esp_crt_bundle without
# R1: esp_crt_bundle trusts a root by matching the served chain's TOP issuer,
# which here is R1 — not the self-signed GTS Root R4 that IS in the Mozilla set.
# Append R1 so HTTPS to a Cloudflare-fronted (Google-issued) Bambuddy verifies.
# R1 is self-signed and valid until 2028-01-28; the SHA-256 pins the exact bytes.
GLOBALSIGN_R1_SHA256="ebd41040e4bb3ec742c9e381d31ef2a41a48b6685c96e7cef3c1df6cd4331c99"
cat > globalsign_r1.pem <<'GLOBALSIGN_R1'
-----BEGIN CERTIFICATE-----
MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG
A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv
b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw
MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i
YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT
aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ
jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp
xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp
1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG
snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ
U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8
9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E
BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B
AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz
yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE
38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP
AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad
DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME
HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==
-----END CERTIFICATE-----
GLOBALSIGN_R1
# Refuse to build if the embedded R1 is not byte-for-byte the real root.
GOT_SHA="$(openssl x509 -in globalsign_r1.pem -outform DER | sha256sum | awk '{print $1}')"
[ "$GOT_SHA" = "$GLOBALSIGN_R1_SHA256" ] || { echo "::error::embedded GlobalSign R1 SHA-256 mismatch ($GOT_SHA)"; exit 1; }
cat globalsign_r1.pem >> cacert.pem

# This gen_crt_bundle.py takes only --input and writes a fixed-name file
# (./x509_crt_bundle) in the cwd — it has no --output flag — so move it into place.
python gen_crt_bundle.py --input cacert.pem
mv x509_crt_bundle data/cert/x509_crt_bundle.bin
ls -l data/cert/x509_crt_bundle.bin
