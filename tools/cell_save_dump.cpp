// cell_save_dump <path.spo> — decode a real cell-stage save via the engine
// decoder (read-only; requires game data, unlike cell_save_verify).
#include "engine/cell_save.h"
#include <cstdio>
#include <iostream>

using namespace os;

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: cell_save_dump <path.spo>\n"; return 2; }
    std::vector<cell::CreatureStats> out;
    cell::StreamInfo info;
    std::string err;
    if (!cell::decode_save(argv[1], out, info, err)) {
        std::cerr << "decode failed: " << err << "\n";
        return 1;
    }
    std::cout << "declared=" << info.declared_count
              << " decoded=" << info.objects_decoded
              << " consumed=" << info.bytes_consumed
              << " crc=0x" << std::hex << info.crc
              << " token=0x" << info.token << std::dec
              << " header=" << info.header_hex << "\n";
    size_t shown = out.size() < 5 ? out.size() : 5;
    for (size_t i = 0; i < shown; ++i) {
        const auto& c = out[i];
        printf("  [%zu] stat0=%u stat1=%u stat2=0x%08x stat3=%u stat4=%u "
               "pos=(0x%08x, 0x%08x, 0x%08x, 0x%08x)",
               i, c.stat0, c.stat1, c.stat2, c.stat3, c.stat4,
               c.pos_x, c.pos_y, c.pos_z, c.pos_w);
        if (c.stat9) printf(" stat9=%.3f", (double)*c.stat9);
        if (c.stat18) printf(" stat18=%u", *c.stat18);
        printf("\n");
    }
    std::cout << "total creatures: " << out.size() << "\n";
    return 0;
}
