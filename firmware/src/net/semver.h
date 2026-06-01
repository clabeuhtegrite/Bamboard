#pragma once

// Dotted-version comparison, split out of github_ota.cpp so it can be unit-
// tested on the host without dragging in the HTTP / Update machinery. Pure: no
// Arduino or network dependency.
namespace ota {

// Compares two dotted versions ("1.2.3"). A leading 'v' and any pre-release
// suffix after '-' or '+' are ignored. Returns >0 if a is newer than b, 0 if
// equal, <0 if a is older.
int semver_cmp(const char* a, const char* b);

}  // namespace ota
