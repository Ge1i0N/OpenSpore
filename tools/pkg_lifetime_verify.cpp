// Regression for transient package readers exhausting the CRT FILE table during drawable loads.
#include "engine/pkg.h"
#include <cstdio>
#include <iostream>
#include <type_traits>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: pkg_lifetime_verify <package>\n"; return 2; }
    static_assert(!std::is_copy_constructible_v<os::pkg::Package>);
    // Deliberately exceed the current CRT stream limit without changing that limit.
    const int iterations = _getmaxstdio() + 32;
    for (int n = 0; n < iterations; ++n) {
        os::pkg::Package package;
        if (!package.open(argv[1]) || package.records().empty()) {
            std::cerr << "scoped package open failed at " << n << '\n';
            return 1;
        }
        std::vector<os::u8> data;
        if (!package.read_resource_decoded(0, data) || data.empty()) {
            std::cerr << "scoped package read failed\n";
            return 1;
        }
    }
    std::cout << iterations << " scoped package open/read/destruct cycles passed (CRT limit "
              << _getmaxstdio() << ")\n";
    return 0;
}
