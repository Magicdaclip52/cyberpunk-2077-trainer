#include "signatures/PatternTable.h"

#include "core/Logger.h"

#include <string>

namespace er {

std::span<const ScanPattern> knownPatterns() {
    static const ScanPattern k[] = {
        // 2.5 unique player vcall 0x11
        {"aobgetplayerpointer",
         "41 B8 11 00 00 00 48 8B 01 48 89 54 24 20 48 8D 54 24 20 C6 44 24 28 00", ""},
        {"aobhealth",
         "8A 87 AE 01 00 00 88 ?? ?? ?? 00 00 8A ?? ?? ?? 00 00 88 ?? ?? ?? 00 00 ?? 8B ?? ?? 8B ?? 24 ?? 48 83 C4",
         ""},
        {"aobchangefloatvalue",
         "F3 0F 58 CE 45 33 C9 48 8B CB E8 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 44 8B C6 E8",
         ""},
        {"aobstamina",
         "F3 0F 10 89 04 01 00 00 0F 28 C3 F3 0F 59 81 18 01 00 00 0F 28 D5", ""},
        {"aobitemdecrease",
         "E8 ?? ?? ?? ?? 84 C0 74 ?? ?? 8D ?? ?? ?? 00 00 ?? ?? ?? E8 ?? ?? ?? ?? 44 8B CD 45 8B C6 48 ?? ?? 48 8B CE E8",
         ""},
        {"aobitemcode", "8B 4F 78 BA 01 00 00 00 F3 48 0F 2C C0 3B C2 0F 43 D0 33 C0 03 CB", ""},
        {"aobreload", "66 2B C6 0F B7 ?? 39 ?? ?? 74 ?? 89 ?? ?? C6", ""},
        {"aobaccuracy",
         "F3 0F 11 89 ?? ?? 00 00 ?? 8B ?? ?? ?? 00 00 0F 57 ?? F3 0F", ""},
        {"aobgetstatspointer", "48 8B 0E 4C 8B C0 ?? 8B ?? ?? 8B ?? ?? FF ?? ?? ?? 8B ?? 24", ""},
        {"aobstatsincrease", "03 13 89 13 48 85 F6 75 ?? ?? 8B ?? 24 ?? 48 83 C4", ""},
        {"aobhackingtime", "F3 0F 5C 00 0F 2F ?? F3 0F 11 ?? 77 ?? 83 ?? 00", ""},
        {"aobstealthmode", "41 38 76 08 0F 84 ?? ?? 00 00 ?? 8D ?? ?? ?? 8B ?? E8", ""},
        {"aobgetstatsid", "81 3B B1 06 00 00 74 ?? 8B 13 E8 ?? ?? ?? ?? 0F 28",
         "81 3B AE 06 00 00 74 ?? 8B 13 E8 ?? ?? ?? ?? 0F 28"},
        {"aobotherstats", "F3 0F 10 44 C8 0C 83 ?? FF ?? ?? ?? ?? ?? 8B ?? 24 ?? 48 83 C4",
         "F3 0F 10 44 C8 0C"},
        {"aobsuperjump", "F3 45 0F 59 C2 F3 44 0F 11 83", ""},
        {"aobdoublejumps",
         "48 8B 48 08 49 3B C8 74 ?? 48 83 ?? ?? EB ?? ?? 89 ?? ?? 8B ?? 10 89 ?? ?? ?? 8D ?? ?? ?? 39",
         "48 8B 48 08 49 3B C8 74 ?? 48 83 ?? ?? EB ?? ?? 89 ?? ?? 8B ?? 10 89 ?? ?? ?? 8D ?? ?? EB"},
        {"aobdaytime", "F3 48 0F 2C F8 48 8B 01 8B D7 FF ?? ?? ?? 00 00 ?? 8B ?? E8", ""},
        {"aobfly", "F2 0F 11 8E ?? ?? 00 00 ?? 85 ?? 0F 84 ?? ?? 00 00 F2 0F 10", ""},
        // 2.5 unique backpack iterator
        {"aobgetinventorypointer",
         "48 8B 0B 48 8B D6 48 8B 01 FF 90 C8 01 00 00 3C 01", ""},
        // 2.5: camera subss pitch/yaw by recoil at +0x2B0/+0x2B4
        {"aobrecoil", "F3 0F 5C 83 B0 02 00 00 F3 0F 10 6F 04", ""},
        {"aobrecoilload", "F3 0F 10 89 B4 02 00 00 F3 0F 10 81 B0 02 00 00", ""},
        {"aobrecoilmul1", "F3 0F 59 89 B0 02 00 00 F3 0F 59 81 28 01 00 00", ""},
        {"aobrecoilmul2", "F3 0F 59 82 B0 02 00 00 F3 0F 59 92 28 01 00 00", ""},
        {"aobrecoilstore", "F3 0F 11 B3 B0 02 00 00 40 8A C7 89 B3 B4 02 00 00", ""},
    };
    return k;
}

void logPatternHits(SignatureScanner& scanner) {
    Logger::instance().info("скан сигнатур на Cyberpunk2077.exe (2.5)");
    int found = 0;
    for (const auto& p : knownPatterns()) {
        Signature primary{p.id, p.primary};
        auto addr = scanner.find(primary, true);
        if (!addr && p.fallback[0] != '\0') {
            Signature fb{std::string(p.id) + "_fb", p.fallback};
            addr = scanner.find(fb, true);
        }
        if (addr) {
            ++found;
            Logger::instance().info(std::string("  ок  ") + p.id + " @ 0x" + toHex(*addr));
        } else {
            Logger::instance().warn(std::string("  нет ") + p.id);
        }
    }
    Logger::instance().info("нашли " + std::to_string(found) + "/" +
                            std::to_string(knownPatterns().size()) + " (fly — PhysX, отдельно)");
}

}  // namespace er
