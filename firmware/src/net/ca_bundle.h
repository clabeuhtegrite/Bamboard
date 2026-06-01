#pragma once

#include <cstdint>

// Root-CA bundle (Espressif esp_crt_bundle binary format) used to VALIDATE
// Bambuddy's TLS certificate when the connection scheme is https://.
//
// The bundle is NOT committed: it is regenerated on every CI build from the
// current Mozilla CA set (curl.se/ca/cacert.pem -> gen_crt_bundle.py) and
// embedded via `board_build.embed_files = data/cert/x509_crt_bundle.bin` in
// platformio.ini, which exposes the linker symbol below. Regenerating it per
// build keeps the trusted roots fresh without shipping a stale CA list.
//
// The host simulator can't embed a file, so it provides a stub definition of
// this symbol and validates over libcurl's system trust store instead
// (see sim/shim/ca_bundle_stub.cpp).
extern const uint8_t ca_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");
