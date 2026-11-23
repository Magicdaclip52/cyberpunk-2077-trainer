#include "hooks/GameHooks.h"

#include "core/Logger.h"
#include "hooks/CaveEmit.h"
#include "signatures/PatternTable.h"

#include <Windows.h>
#include <Psapi.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace er {
namespace {

std::string hexBytes(ProcessMemory& memory, uptr addr, usize n) {
    std::vector<u8> b(n);
    if (n == 0 || !memory.read(addr, b.data(), n)) {
        return "?";
    }
    std::string s;
    s.reserve(n * 3);
    char tmp[4];
    for (usize i = 0; i < n; ++i) {
        std::snprintf(tmp, sizeof(tmp), "%02X ", b[i]);
        s += tmp;
    }
    if (!s.empty()) {
        s.pop_back();
    }
    return s;
}

bool commitCave(ProcessMemory& memory, CodeCave& cave, uptr site, usize patchLen, CaveEmit& e,
                const char* name) {
    std::string why;
    const std::string orig = hexBytes(memory, site, patchLen);
    Logger::instance().dbg(std::string(name) + " install site=0x" + toHex(site) + " cave=0x" + toHex(cave.base()) +
                           " len=" + std::to_string(patchLen) + " orig=[" + orig + "] stub=" +
                           std::to_string(e.b.size()) + "B");
    if (!e.finish(&why)) {
        Logger::instance().error(std::string(name) + ": " + why);
        GameHooks::instance().noteError(std::string(name) + ": " + why);
        return false;
    }
    if (e.b.size() >= GameHooks::kFlags) {
        Logger::instance().error(std::string(name) + ": stub " + std::to_string(e.b.size()) +
                                 "B перекрывает флаги @" + std::to_string(GameHooks::kFlags));
        GameHooks::instance().noteError(std::string(name) + ": stub overlap flags");
        return false;
    }
    if (!cave.write(memory, 0, e.b) || !cave.installRelJmpHook(memory, site, patchLen)) {
        Logger::instance().error(std::string(name) + ": jmp не записан");
        GameHooks::instance().noteError(std::string(name) + ": jmp не записан");
        return false;
    }
    Logger::instance().info(std::string(name) + " ON @ 0x" + toHex(site) + " orig=[" + orig + "]");
    return true;
}

uptr findModuleText(ProcessMemory& memory, const wchar_t* modName, usize& textSizeOut) {
    textSizeOut = 0;
    DWORD needed = 0;
    EnumProcessModulesEx(memory.handle(), nullptr, 0, &needed, LIST_MODULES_64BIT);
    if (needed < sizeof(HMODULE)) {
        EnumProcessModules(memory.handle(), nullptr, 0, &needed);
    }
    if (needed < sizeof(HMODULE)) {
        return 0;
    }
    std::vector<HMODULE> mods(needed / sizeof(HMODULE) + 32);
    const DWORD bytes = static_cast<DWORD>(mods.size() * sizeof(HMODULE));
    if (!EnumProcessModulesEx(memory.handle(), mods.data(), bytes, &needed, LIST_MODULES_64BIT) &&
        !EnumProcessModules(memory.handle(), mods.data(), bytes, &needed)) {
        return 0;
    }
    const usize n = needed / sizeof(HMODULE);
    wchar_t name[MAX_PATH]{};
    for (usize i = 0; i < n && i < mods.size(); ++i) {
        name[0] = 0;
        GetModuleBaseNameW(memory.handle(), mods[i], name, MAX_PATH);
        if (_wcsicmp(name, modName) != 0) {
            continue;
        }
        MODULEINFO info{};
        if (!GetModuleInformation(memory.handle(), mods[i], &info, sizeof(info))) {
            return 0;
        }
        const uptr base = reinterpret_cast<uptr>(info.lpBaseOfDll);
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!memory.read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
            return 0;
        }
        if (!memory.read(base + static_cast<uptr>(dos.e_lfanew), nt) || nt.Signature != IMAGE_NT_SIGNATURE) {
            return 0;
        }
        const uptr sectionStart = base + static_cast<uptr>(dos.e_lfanew) +
                                  offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                                  nt.FileHeader.SizeOfOptionalHeader;
        for (u16 s = 0; s < nt.FileHeader.NumberOfSections; ++s) {
            IMAGE_SECTION_HEADER sec{};
            if (!memory.read(sectionStart + static_cast<uptr>(s) * sizeof(IMAGE_SECTION_HEADER), sec)) {
                continue;
            }
            char sn[9]{};
            std::memcpy(sn, sec.Name, 8);
            if (std::string_view(sn) == ".text") {
                textSizeOut = sec.Misc.VirtualSize ? sec.Misc.VirtualSize : sec.SizeOfRawData;
                return base + sec.VirtualAddress;
            }
        }
    }
    return 0;
}

uptr findModuleBase(ProcessMemory& memory, const wchar_t* modName, usize& sizeOut) {
    sizeOut = 0;
    DWORD needed = 0;
    EnumProcessModulesEx(memory.handle(), nullptr, 0, &needed, LIST_MODULES_64BIT);
    if (needed < sizeof(HMODULE)) {
        EnumProcessModules(memory.handle(), nullptr, 0, &needed);
    }
    if (needed < sizeof(HMODULE)) {
        return 0;
    }
    std::vector<HMODULE> mods(needed / sizeof(HMODULE) + 32);
    const DWORD bytes = static_cast<DWORD>(mods.size() * sizeof(HMODULE));
    if (!EnumProcessModulesEx(memory.handle(), mods.data(), bytes, &needed, LIST_MODULES_64BIT) &&
        !EnumProcessModules(memory.handle(), mods.data(), bytes, &needed)) {
        return 0;
    }
    const usize n = needed / sizeof(HMODULE);
    wchar_t name[MAX_PATH]{};
    for (usize i = 0; i < n && i < mods.size(); ++i) {
        name[0] = 0;
        GetModuleBaseNameW(memory.handle(), mods[i], name, MAX_PATH);
        if (_wcsicmp(name, modName) != 0) {
            continue;
        }
        MODULEINFO info{};
        if (!GetModuleInformation(memory.handle(), mods[i], &info, sizeof(info))) {
            return 0;
        }
        sizeOut = info.SizeOfImage;
        return reinterpret_cast<uptr>(info.lpBaseOfDll);
    }
    return 0;
}

uptr remoteExport(ProcessMemory& memory, uptr base, const char* name) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!memory.read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    if (!memory.read(base + static_cast<uptr>(dos.e_lfanew), nt) || nt.Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    const auto& exp = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!exp.VirtualAddress || !exp.Size) {
        return 0;
    }
    IMAGE_EXPORT_DIRECTORY dir{};
    if (!memory.read(base + exp.VirtualAddress, dir)) {
        return 0;
    }
    const usize nNames = dir.NumberOfNames;
    if (nNames == 0 || nNames > 8192) {
        return 0;
    }
    std::vector<u32> names(nNames);
    std::vector<u16> ords(nNames);
    if (!memory.read(base + dir.AddressOfNames, names.data(), nNames * 4) ||
        !memory.read(base + dir.AddressOfNameOrdinals, ords.data(), nNames * 2)) {
        return 0;
    }
    for (usize i = 0; i < nNames; ++i) {
        char buf[64]{};
        if (!memory.read(base + names[i], buf, sizeof(buf) - 1)) {
            continue;
        }
        buf[sizeof(buf) - 1] = 0;
        if (std::strcmp(buf, name) != 0) {
            continue;
        }
        u32 rva = 0;
        if (!memory.read(base + dir.AddressOfFunctions + static_cast<uptr>(ords[i]) * 4, rva) || !rva) {
            return 0;
        }
        return base + rva;
    }
    return 0;
}

uptr remoteProc(ProcessMemory& memory, const wchar_t* dll, const char* fn) {
    usize sz = 0;
    const uptr base = findModuleBase(memory, dll, sz);
    if (!base) {
        return 0;
    }
    return remoteExport(memory, base, fn);
}

void collectIatSlots(ProcessMemory& memory, const uptr fnAddr, std::vector<uptr>& slots) {
    if (!fnAddr) {
        return;
    }
    DWORD needed = 0;
    EnumProcessModulesEx(memory.handle(), nullptr, 0, &needed, LIST_MODULES_64BIT);
    if (needed < sizeof(HMODULE)) {
        EnumProcessModules(memory.handle(), nullptr, 0, &needed);
    }
    if (needed < sizeof(HMODULE)) {
        return;
    }
    std::vector<HMODULE> mods(needed / sizeof(HMODULE) + 32);
    const DWORD bytes = static_cast<DWORD>(mods.size() * sizeof(HMODULE));
    if (!EnumProcessModulesEx(memory.handle(), mods.data(), bytes, &needed, LIST_MODULES_64BIT) &&
        !EnumProcessModules(memory.handle(), mods.data(), bytes, &needed)) {
        return;
    }
    const usize n = needed / sizeof(HMODULE);
    wchar_t modName[MAX_PATH]{};
    for (usize i = 0; i < n && i < mods.size(); ++i) {
        modName[0] = 0;
        GetModuleBaseNameW(memory.handle(), mods[i], modName, MAX_PATH);
        if (_wcsicmp(modName, kProcessName) != 0) {
            continue;
        }
        MODULEINFO info{};
        if (!GetModuleInformation(memory.handle(), mods[i], &info, sizeof(info))) {
            continue;
        }
        const uptr base = reinterpret_cast<uptr>(info.lpBaseOfDll);
        IMAGE_DOS_HEADER dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!memory.read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
            continue;
        }
        if (!memory.read(base + static_cast<uptr>(dos.e_lfanew), nt) || nt.Signature != IMAGE_NT_SIGNATURE) {
            continue;
        }
        auto walkThunks = [&](uptr iatRva, usize maxSlots) {
            if (!iatRva) {
                return;
            }
            const uptr iat = base + iatRva;
            for (usize k = 0; k < maxSlots; ++k) {
                u64 val = 0;
                if (!memory.read(iat + k * 8, val)) {
                    break;
                }
                if (val == 0) {
                    break;
                }
                if (val == fnAddr) {
                    slots.push_back(iat + k * 8);
                }
            }
        };
        const auto& imp = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (imp.VirtualAddress && imp.Size) {
            const uptr desc = base + imp.VirtualAddress;
            const usize count = imp.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
            for (usize d = 0; d < count && d < 512; ++d) {
                IMAGE_IMPORT_DESCRIPTOR id{};
                if (!memory.read(desc + d * sizeof(IMAGE_IMPORT_DESCRIPTOR), id)) {
                    break;
                }
                if (id.Name == 0 && id.FirstThunk == 0) {
                    break;
                }
                walkThunks(id.FirstThunk, 4096);
            }
        }
        const auto& delay = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
        if (delay.VirtualAddress && delay.Size) {
            struct DelayDesc {
                u32 attributes;
                u32 dllNameRva;
                u32 moduleHandleRva;
                u32 iatRva;
                u32 intRva;
                u32 boundIatRva;
                u32 unloadIatRva;
                u32 timeStamp;
            };
            const uptr desc = base + delay.VirtualAddress;
            const usize count = delay.Size / sizeof(DelayDesc);
            for (usize d = 0; d < count && d < 256; ++d) {
                DelayDesc dd{};
                if (!memory.read(desc + d * sizeof(DelayDesc), dd)) {
                    break;
                }
                if (dd.dllNameRva == 0 && dd.iatRva == 0) {
                    break;
                }
                walkThunks(dd.iatRva, 4096);
            }
        }
    }
}

}  // namespace

GameHooks& GameHooks::instance() {
    static GameHooks g;
    return g;
}

std::string GameHooks::debugSnapshot() const {
    std::ostringstream o;
    o << "=== hooks ===\n";
    auto row = [&](const char* name, uptr site, bool hooked, uptr cave = 0) {
        o << (hooked ? "ON " : (site ? "sig" : " --")) << "  " << name << "  site=0x" << toHex(site);
        if (cave) {
            o << "  cave=0x" << toHex(cave);
        }
        o << "\n";
    };
    row("player", playerSite_ ? playerSite_ + 0xD60 : 0, playerHooked_, playerCave_.base());
    row("health", healthSite_, healthHooked_, healthCave_.base());
    row("float", floatSite_, floatHooked_, floatCave_.base());
    row("stamina", staminaSite_, staminaHooked_, staminaCave_.base());
    row("itemcode", itemCodeSite_, itemCodeHooked_, itemCodeCave_.base());
    row("itemdec", itemDecSite_, itemDecHooked_, itemDecCave_.base());
    row("inventory", inventorySite_, inventoryHooked_, inventoryCave_.base());
    row("recoil", recoilSite_, recoilHooked_, recoilCave_.base());
    row("reload", reloadSite_, reloadPatch_.applied());
    row("accuracy", accuracySite_, accuracyHooked_, accuracyCave_.base());
    row("stealth", stealthSite_, stealthPatch_.applied());
    row("hack", hackSite_, hackPatch_.applied());
    row("statsid", statsIdSite_, statsIdHooked_, statsIdCave_.base());
    row("statsptr", statsPtrSite_, statsPtrHooked_, statsPtrCave_.base());
    row("statsinc", statsIncSite_, statsIncHooked_, statsIncCave_.base());
    row("other", otherStatsSite_, otherHooked_, otherCave_.base());
    row("jump", jumpSite_, jumpHooked_, jumpCave_.base());
    row("djump", djumpSite_, djumpHooked_, djumpCave_.base());
    row("day", daySite_, dayHooked_, dayCave_.base());
    row("fly", flySite_, flyHooked_, flyCave_.base());
    row("speed", speedReady_ ? 1 : 0, speedHooked_, speedCave_.base());
    o << "healthOn=" << (healthOn_ ? 1 : 0) << " peak=" << healthPeak_ << " pool=0x" << toHex(lastHealthPool_)
      << " flyOn=" << (flyOn_ ? 1 : 0) << " lastError=[" << lastError_ << "]\n";
    o << "speed IAT QPC=" << qpcIat_.size() << " GTC64=" << gtcIat_.size() << "\n";
    return o.str();
}

namespace {

bool jmpAt(ProcessMemory& memory, uptr site) {
    if (!site) {
        return false;
    }
    u8 b = 0;
    return memory.read(site, b) && b == 0xE9;
}

std::string flagOn(ProcessMemory& memory, uptr cave, usize off, const char* hookedOk, const char* noHook) {
    if (!cave) {
        return noHook;
    }
    u32 v = 0;
    memory.read(cave + off, v);
    return v ? hookedOk : std::string("хук стоит, флаг=0");
}

}  // namespace

std::string GameHooks::liveProbe(ProcessMemory& memory) const {
    std::ostringstream o;
    o << "=== live hooks ===\n";
    auto line = [&](const char* name, uptr site, bool hooked, uptr cave) {
        const char* st = !site ? "нет сигнатуры" : (!hooked ? "сигнатура, хук выкл" : (jmpAt(memory, site) ? "хук ОК" : "хук СЛЕТЕЛ"));
        o << st << "  " << name << "  site=0x" << toHex(site);
        if (cave) {
            o << "  cave=0x" << toHex(cave);
        }
        o << "\n";
    };
    line("player", playerSite_ ? playerSite_ + 0xD60 : 0, playerHooked_, playerCave_.base());
    line("health", healthSite_, healthHooked_, healthCave_.base());
    line("float", floatSite_, floatHooked_, floatCave_.base());
    line("stamina", staminaSite_, staminaHooked_, staminaCave_.base());
    line("itemcode", itemCodeSite_, itemCodeHooked_, itemCodeCave_.base());
    line("itemdec", itemDecSite_, itemDecHooked_, itemDecCave_.base());
    line("inventory", inventorySite_, inventoryHooked_, inventoryCave_.base());
    o << (recoilHooked_ ? "патч ОК" : "патч выкл") << "  recoil subss=" << toHex(recoilSite_)
      << " load=" << toHex(recoilLoadSite_) << " store=" << toHex(recoilStoreSite_) << "\n";
    line("accuracy", accuracySite_, accuracyHooked_, accuracyCave_.base());
    line("statsid", statsIdSite_, statsIdHooked_, statsIdCave_.base());
    line("statsptr", statsPtrSite_, statsPtrHooked_, statsPtrCave_.base());
    line("statsinc", statsIncSite_, statsIncHooked_, statsIncCave_.base());
    line("other", otherStatsSite_, otherHooked_, otherCave_.base());
    line("jump", jumpSite_, jumpHooked_, jumpCave_.base());
    line("djump", djumpSite_, djumpHooked_, djumpCave_.base());
    line("day", dayHooked_ ? daySite_ + 5 : daySite_, dayHooked_, dayCave_.base());
    line("fly", flySite_, flyHooked_, flyCave_.base());
    o << (reloadPatch_.applied() ? "патч ОК" : "патч выкл") << "  reload\n";
    o << (stealthPatch_.applied() ? "патч ОК" : "патч выкл") << "  stealth\n";
    o << (hackPatch_.applied() ? "патч ОК" : "патч выкл") << "  hack\n";
    o << (speedHooked_ ? "IAT ОК" : (speedReady_ ? "IAT готов, выкл" : "IAT нет")) << "  speed\n";
    if (playerCave_.base()) {
        uptr pool = 0;
        memory.read(playerCave_.base() + kPtrs, pool);
        o << "player pool=0x" << toHex(pool) << " healthOn=" << (healthOn_ ? 1 : 0) << "\n";
    }
    return o.str();
}

std::string GameHooks::probeForFeature(ProcessMemory& memory, std::string_view id) const {
    auto needJmp = [&](uptr site, bool hooked, const char* missingSig) -> std::string {
        if (!site) {
            return missingSig;
        }
        if (!hooked) {
            return "сигнатура есть, хук не поставлен";
        }
        return jmpAt(memory, site) ? std::string() : std::string("хук слетел (нет jmp)");
    };

    if (id == "infinite_health") {
        std::string e = needJmp(playerSite_ ? playerSite_ + 0xD60 : 0, playerHooked_, "нет player-сайта");
        if (e.empty()) {
            e = needJmp(floatSite_, floatHooked_, "нет float-сайта");
        }
        if (!e.empty()) {
            return "НЕ работает: " + e;
        }
        u32 fl = 0;
        if (floatCave_.base()) {
            memory.read(floatCave_.base() + kFlags + 0x18, fl);
        }
        uptr pool = 0;
        if (playerCave_.base()) {
            memory.read(playerCave_.base() + kPtrs, pool);
        }
        if (!fl) {
            return "хук ОК, флаг здоровья=0";
        }
        if (pool < 0x10000) {
            return "хук ОК, ждём игрока (зайди в мир)";
        }
        return "работает (только игрок) puppet=0x" + toHex(pool);
    }
    if (id == "infinite_stamina" || id == "infinite_ram") {
        std::string e = needJmp(staminaSite_, staminaHooked_, "нет stamina-сайта");
        if (!e.empty()) {
            return "НЕ работает: " + e;
        }
        const usize off = id == "infinite_ram" ? 4 : 0;
        return flagOn(memory, staminaCave_.base(), kFlags + off, "работает (хук+флаг)", "нет cave");
    }
    if (id == "one_hit_kill" || id == "grenade_no_cd" || id == "pls_no_cd" || id == "heal_no_cd" ||
        id == "damage_multiplier" || id == "defense_multiplier") {
        std::string e = needJmp(floatSite_, floatHooked_, "нет float-сайта");
        if (!e.empty()) {
            return "НЕ работает: " + e;
        }
        usize off = kFlags;
        if (id == "grenade_no_cd") {
            off = kFlags + 4;
        } else if (id == "pls_no_cd") {
            off = kFlags + 8;
        } else if (id == "one_hit_kill") {
            off = kFlags + 0x0C;
        } else if (id == "damage_multiplier") {
            off = kFlags + 0x10;
        } else if (id == "defense_multiplier") {
            off = kFlags + 0x14;
        }
        u32 v = 0;
        if (floatCave_.base()) {
            memory.read(floatCave_.base() + off, v);
        }
        if (id == "damage_multiplier" || id == "defense_multiplier") {
            return v ? "работает (множитель записан)" : "хук ОК, множитель=0";
        }
        return v ? "работает (хук+флаг)" : "хук стоит, флаг=0";
    }
    if (id == "no_reload") {
        return reloadPatch_.applied() ? "работает (патч)" : "патч выкл";
    }
    if (id == "stealth_mode") {
        return stealthPatch_.applied() ? "работает (патч)" : "патч выкл";
    }
    if (id == "freeze_breach") {
        return hackPatch_.applied() ? "работает (патч)" : "патч выкл";
    }
    if (id == "super_accuracy") {
        std::string e = needJmp(accuracySite_, accuracyHooked_, "нет accuracy-сайта");
        return e.empty() ? "работает (хук)" : "НЕ работает: " + e;
    }
    if (id == "no_recoil") {
        if (!recoilSiteFound()) {
            return "НЕ работает: нет recoil-сайта";
        }
        int n = 0;
        if (recoilPatch_.applied()) {
            ++n;
        }
        if (recoilLoadPatch_.applied()) {
            ++n;
        }
        if (recoilMul1Patch_.applied()) {
            ++n;
        }
        if (recoilMul2Patch_.applied()) {
            ++n;
        }
        if (recoilCave_.active()) {
            ++n;
        }
        return n ? ("работает (патчей " + std::to_string(n) + ")") : "НЕ работает: патч не стоит";
    }
    if (id == "infinite_items" || id == "items_no_decrease") {
        std::string e = needJmp(itemCodeSite_, itemCodeHooked_, "нет itemcode-сайта");
        return e.empty() ? "работает (хук расхода)" : "НЕ работает: " + e;
    }
    if (id == "infinite_components" || id == "infinite_qh_components" || id == "edit_money") {
        std::string e = needJmp(inventorySite_, inventoryHooked_, "нет inventory-сайта");
        if (!e.empty()) {
            return "НЕ работает: " + e;
        }
        if (id == "edit_money") {
            return "хук инвентаря ОК (применение пишет в сейв)";
        }
        const usize off = id == "infinite_qh_components" ? 8 : 4;
        return flagOn(memory, inventoryCave_.base(), kFlags + off, "работает (хук+флаг)", "нет cave");
    }
    if (id == "infinite_xp" || id == "xp_multiplier" || id == "infinite_streetcred" ||
        id == "streetcred_multiplier" || id == "max_skill_xp" || id == "skill_xp_multiplier") {
        std::string e = needJmp(statsIncSite_, statsIncHooked_, "нет statsinc-сайта");
        return e.empty() ? "работает (хук статов)" : "НЕ работает: " + e;
    }
    if (id == "ignore_cyberware" || id == "movement_speed" || id == "edit_carry_weight") {
        std::string e = needJmp(otherStatsSite_, otherHooked_, "нет otherstats-сайта");
        return e.empty() ? "работает (хук otherstats)" : "НЕ работает: " + e;
    }
    if (id == "edit_attribute_points" || id == "edit_perk_points" || id == "edit_relic_points" ||
        id == "edit_player_level" || id == "edit_streetcred_level" || id == "edit_headhunter" ||
        id == "edit_netrunner" || id == "edit_shinobi" || id == "edit_solo" || id == "edit_engineer") {
        std::string e = needJmp(statsPtrSite_, statsPtrHooked_, "нет statsptr-сайта");
        return e.empty() ? "хук статов ОК (применение пишет в сейв)" : "НЕ работает: " + e;
    }
    if (id == "freeze_daytime" || id == "daytime_plus_1h") {
        std::string e = needJmp(dayHooked_ ? daySite_ + 5 : 0, dayHooked_, "нет daytime-сайта");
        return e.empty() ? "работает (хук времени)" : "НЕ работает: " + e;
    }
    if (id == "super_jump") {
        std::string e = needJmp(jumpSite_, jumpHooked_, "нет jump-сайта");
        return e.empty() ? "работает (хук)" : "НЕ работает: " + e;
    }
    if (id == "infinite_double_jump") {
        std::string e = needJmp(djumpSite_, djumpHooked_, "нет djump-сайта");
        return e.empty() ? "работает (хук)" : "НЕ работает: " + e;
    }
    if (id == "fly_mode" || id == "fly_height" || id == "fly_speed") {
        std::string e = needJmp(flySite_, flyHooked_, "нет fly-сайта");
        return e.empty() ? "работает (хук PhysX)" : "НЕ работает: " + e;
    }
    if (id == "game_speed") {
        if (!speedReady_) {
            return "НЕ работает: IAT не найден";
        }
        return speedHooked_ ? "работает (IAT QPC)" : "IAT готов, хук выкл";
    }
    return "нет отдельной проверки";
}

bool GameHooks::writeU32(ProcessMemory& memory, CodeCave& cave, usize off, u32 v) {
    return cave.base() != 0 && cave.writeU32(memory, off, v);
}

bool GameHooks::writeF32(ProcessMemory& memory, CodeCave& cave, usize off, float v) {
    u32 bits = 0;
    std::memcpy(&bits, &v, 4);
    return writeU32(memory, cave, off, bits);
}

bool GameHooks::resolve(ProcessMemory& memory, SignatureScanner& scanner) {
    lastError_.clear();
    logPatternHits(scanner);

    auto takeU = [&](uptr& dst, const char* id, const char* aob, const char* fb) {
        Signature a{id, aob};
        if (auto s = scanner.find(a, true)) {
            dst = *s;
            return;
        }
        if (fb && fb[0]) {
            Signature b{std::string(id) + "_fb", fb};
            if (auto s = scanner.find(b, true)) {
                dst = *s;
            }
        }
    };

    for (const auto& p : knownPatterns()) {
        const std::string_view id = p.id;
        if (id == "aobfly") {
            continue;
        }
        if (id == "aobstatsincrease") {
            auto hits = scanner.findAll(Signature{p.id, p.primary});
            if (!hits.empty()) {
                statsIncSite_ = hits.front();
            }
            continue;
        }
        uptr* dst = nullptr;
        if (id == "aobgetplayerpointer") dst = &playerSite_;
        else if (id == "aobhealth") dst = &healthSite_;
        else if (id == "aobchangefloatvalue") dst = &floatSite_;
        else if (id == "aobstamina") dst = &staminaSite_;
        else if (id == "aobitemdecrease") dst = &itemDecSite_;
        else if (id == "aobitemcode") dst = &itemCodeSite_;
        else if (id == "aobreload") dst = &reloadSite_;
        else if (id == "aobaccuracy") dst = &accuracySite_;
        else if (id == "aobgetstatspointer") dst = &statsPtrSite_;
        else if (id == "aobhackingtime") dst = &hackSite_;
        else if (id == "aobstealthmode") dst = &stealthSite_;
        else if (id == "aobgetstatsid") dst = &statsIdSite_;
        else if (id == "aobotherstats") dst = &otherStatsSite_;
        else if (id == "aobsuperjump") dst = &jumpSite_;
        else if (id == "aobdoublejumps") dst = &djumpSite_;
        else if (id == "aobdaytime") dst = &daySite_;
        else if (id == "aobgetinventorypointer") dst = &inventorySite_;
        else if (id == "aobrecoil") dst = &recoilSite_;
        else if (id == "aobrecoilload") dst = &recoilLoadSite_;
        else if (id == "aobrecoilmul1") dst = &recoilMul1Site_;
        else if (id == "aobrecoilmul2") dst = &recoilMul2Site_;
        else if (id == "aobrecoilstore") dst = &recoilStoreSite_;
        if (dst) {
            takeU(*dst, p.id, p.primary, p.fallback);
        }
    }

    if (staminaSite_) {
        memory.read(staminaSite_ + 4, staminaDisp_);
    }
    if (accuracySite_) {
        memory.read(accuracySite_ + 4, accuracyDisp_);
    }

    tryResolveFly(memory);
    tryResolveSpeed(memory);
    return true;
}

bool GameHooks::tryResolveFly(ProcessMemory& memory) {
    if (flySite_) {
        return true;
    }
    usize physxTextSize = 0;
    const uptr physxText = findModuleText(memory, kPhysxModule, physxTextSize);
    if (!physxText || !physxTextSize) {
        return false;
    }
    SignatureScanner scan(memory);
    scan.setTextRange(physxText, physxTextSize);
    if (auto s = scan.find(
            Signature{"aobfly", "F2 0F 11 8E ?? ?? 00 00 ?? 85 ?? 0F 84 ?? ?? 00 00 F2 0F 10"}, true)) {
        flySite_ = *s;
        memory.read(flySite_ + 4, flyDisp_);
        Logger::instance().info("aobfly (PhysX) @ 0x" + toHex(*s));
        return true;
    }
    return false;
}

void GameHooks::shutdown(ProcessMemory& memory) {
    restoreSpeedIat(memory);
    reloadPatch_.revert(memory);
    recoilPatch_.revert(memory);
    recoilYawPatch_.revert(memory);
    recoilLoadPatch_.revert(memory);
    recoilMul1Patch_.revert(memory);
    recoilMul2Patch_.revert(memory);
    stealthPatch_.revert(memory);
    hackPatch_.revert(memory);
    CodeCave* caves[] = {&playerCave_,  &healthCave_,  &floatCave_,     &staminaCave_, &itemCodeCave_,
                         &itemDecCave_, &statsIdCave_, &statsPtrCave_,  &statsIncCave_, &otherCave_,
                         &jumpCave_,    &djumpCave_,   &dayCave_,       &flyCave_,      &accuracyCave_,
                         &inventoryCave_, &recoilCave_, &speedCave_};
    for (CodeCave* c : caves) {
        c->removeHook(memory);
    }
    Sleep(150);
    for (CodeCave* c : caves) {
        c->release(memory);
    }
    playerHooked_ = healthHooked_ = floatHooked_ = staminaHooked_ = false;
    itemCodeHooked_ = itemDecHooked_ = statsIdHooked_ = statsPtrHooked_ = false;
    statsIncHooked_ = otherHooked_ = jumpHooked_ = djumpHooked_ = false;
    dayHooked_ = flyHooked_ = accuracyHooked_ = flyOn_ = false;
    inventoryHooked_ = recoilHooked_ = speedHooked_ = healthOn_ = false;
    healthPeak_ = 0.0f;
    lastHealthPool_ = 0;
    healthLogTick_ = 0;
}

void GameHooks::tick(ProcessMemory& memory) {
    if (!flySite_ && (++flyRetryTick_ % 60) == 1) {
        tryResolveFly(memory);
    }
    if (!speedReady_ && (flyRetryTick_ % 120) == 2) {
        tryResolveSpeed(memory);
    }
    if (healthOn_ && playerCave_.base()) {
        uptr puppet = 0;
        uptr inner = 0;
        memory.read(playerCave_.base() + kPtrs, puppet);
        memory.read(playerCave_.base() + kPtrs + 8, inner);
        if (puppet != lastHealthPool_) {
            Logger::instance().dbg("hp puppet 0x" + toHex(lastHealthPool_) + " -> 0x" + toHex(puppet) +
                                   " comp=0x" + toHex(inner));
            lastHealthPool_ = puppet;
        }
        if ((++healthLogTick_ % 120) == 0) {
            uptr cfRbp = 0;
            uptr cfRbx = 0;
            uptr cfR9 = 0;
            uptr cfHash = 0;
            if (floatCave_.base()) {
                memory.read(floatCave_.base() + kPtrs, cfRbp);
                memory.read(floatCave_.base() + kPtrs + 8, cfRbx);
                memory.read(floatCave_.base() + kPtrs + 16, cfR9);
                memory.read(floatCave_.base() + kPtrs + 24, cfHash);
            }
            Logger::instance().dbg("hp watch puppet=0x" + toHex(puppet) + " comp=0x" + toHex(inner) +
                                   " cf.rbp=0x" + toHex(cfRbp) + " rbx=0x" + toHex(cfRbx) + " r9=0x" +
                                   toHex(cfR9) + " hash=0x" + toHex(cfHash));
        }
    }
    if (!flyOn_ || !flyHooked_ || flyCave_.base() == 0) {
        return;
    }
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    writeU32(memory, flyCave_, kFlags + 12, shift && (GetAsyncKeyState(VK_UP) & 0x8000) ? 1 : 0);
    writeU32(memory, flyCave_, kFlags + 16, shift && (GetAsyncKeyState(VK_DOWN) & 0x8000) ? 1 : 0);
    writeU32(memory, flyCave_, kFlags + 20, shift && (GetAsyncKeyState(VK_LEFT) & 0x8000) ? 1 : 0);
    writeU32(memory, flyCave_, kFlags + 24, shift && (GetAsyncKeyState(VK_RIGHT) & 0x8000) ? 1 : 0);
}

bool GameHooks::ensurePlayerHook(ProcessMemory& memory) {
    return playerHooked_ || (playerSite_ && installPlayerHook(memory));
}

bool GameHooks::ensureHealthHook(ProcessMemory& memory) {
    if (!ensurePlayerHook(memory)) {
        lastError_ += "нет player pointer. ";
    }
    return healthHooked_ || (healthSite_ && installHealthHook(memory));
}

bool GameHooks::ensureFloatHook(ProcessMemory& memory) {
    if (!ensurePlayerHook(memory)) {
        lastError_ += "нет player pointer. ";
    }
    return floatHooked_ || (floatSite_ && installFloatHook(memory));
}

bool GameHooks::ensureStaminaHook(ProcessMemory& memory) {
    return staminaHooked_ || (staminaSite_ && installStaminaHook(memory));
}

bool GameHooks::ensureItemHooks(ProcessMemory& memory) {
    return itemCodeHooked_ || (itemCodeSite_ && installItemCodeHook(memory));
}

bool GameHooks::ensureInventoryHook(ProcessMemory& memory) {
    return inventoryHooked_ || (inventorySite_ && installInventoryHook(memory));
}

bool GameHooks::ensureRecoilHook(ProcessMemory& memory) {
    (void)memory;
    return recoilSiteFound();
}

bool GameHooks::ensureSpeedHook(ProcessMemory& memory) {
    if (speedHooked_) {
        return true;
    }
    tryResolveSpeed(memory);
    return speedReady_ && installSpeedHook(memory);
}

bool GameHooks::ensureStatsIdHook(ProcessMemory& memory) {
    return statsIdHooked_ || (statsIdSite_ && installStatsIdHook(memory));
}

bool GameHooks::ensureStatsPtrHook(ProcessMemory& memory) {
    return statsPtrHooked_ || (statsPtrSite_ && installStatsPtrHook(memory));
}

bool GameHooks::ensureStatsIncHook(ProcessMemory& memory) {
    ensureStatsPtrHook(memory);
    return statsIncHooked_ || (statsIncSite_ && installStatsIncHook(memory));
}

bool GameHooks::ensureOtherStatsHook(ProcessMemory& memory) {
    ensureStatsIdHook(memory);
    return otherHooked_ || (otherStatsSite_ && installOtherStatsHook(memory));
}

bool GameHooks::ensureStatsHooks(ProcessMemory& memory) {
    const bool a = ensureStatsPtrHook(memory);
    const bool b = ensureStatsIncHook(memory);
    const bool c = ensureOtherStatsHook(memory);
    return a || b || c;
}

bool GameHooks::ensureJumpHook(ProcessMemory& memory) {
    return jumpHooked_ || (jumpSite_ && installJumpHook(memory));
}

bool GameHooks::ensureDjumpHook(ProcessMemory& memory) {
    return djumpHooked_ || (djumpSite_ && installDjumpHook(memory));
}

bool GameHooks::ensureDayHook(ProcessMemory& memory) {
    return dayHooked_ || (daySite_ && installDayHook(memory));
}

bool GameHooks::ensureFlyHook(ProcessMemory& memory) {
    return flyHooked_ || (flySite_ && installFlyHook(memory));
}

bool GameHooks::installPlayerHook(ProcessMemory& memory) {
    if (playerHooked_ || playerSite_ == 0) {
        return playerHooked_;
    }
    const uptr helper = playerSite_ - 11;
    const uptr callSite = playerSite_ + 0xD60;
    u8 prolog[4]{};
    u8 callb = 0;
    if (!memory.read(helper, prolog) || prolog[0] != 0x48 || prolog[1] != 0x83 || prolog[2] != 0xEC) {
        lastError_ = "player helper не найден";
        return false;
    }
    if (!memory.read(callSite, callb) || callb != 0xE8) {
        lastError_ = "player caller не найден";
        return false;
    }
    if (!playerCave_.allocateNear(memory, callSite)) {
        return false;
    }
    const uptr cave = playerCave_.base();
    const uptr puppet = cave + kPtrs;
    const uptr comp = cave + kPtrs + 8;

    CaveEmit e;
    e.base = cave;
    e.bytes({0x4D, 0x85, 0xF6});  // test r14, r14
    e.je8("call");
    e.movRipR14(puppet);
    e.bytes({0x49, 0x8B, 0x86, 0xD0, 0x00, 0x00, 0x00});  // mov rax,[r14+0xD0]
    e.movRipRax(comp);
    e.L("call");
    e.callRel(helper);
    e.absJmp(callSite + 5);

    playerCave_.writeU32(memory, kFlags, 0);
    playerCave_.writeU64(memory, kPtrs, 0);
    playerCave_.writeU64(memory, kPtrs + 8, 0);
    if (!commitCave(memory, playerCave_, callSite, 5, e, "player")) {
        return false;
    }
    playerHooked_ = true;
    return true;
}

bool GameHooks::installHealthHook(ProcessMemory& memory) {
    if (healthHooked_ || healthSite_ == 0) {
        return healthHooked_;
    }
    if (!healthCave_.allocateNear(memory, healthSite_)) {
        return false;
    }
    const uptr cave = healthCave_.base();

    CaveEmit e;
    e.base = cave;
    e.L("code");
    e.bytes({0x8A, 0x87, 0xAE, 0x01, 0x00, 0x00});
    e.absJmp(healthSite_ + 6);

    healthCave_.writeU32(memory, kFlags, 0);
    healthCave_.writeU64(memory, kPtrs, 0);
    if (!commitCave(memory, healthCave_, healthSite_, 6, e, "health")) {
        return false;
    }
    healthHooked_ = true;
    return true;
}

bool GameHooks::installFloatHook(ProcessMemory& memory) {
    if (floatHooked_ || floatSite_ == 0) {
        return floatHooked_;
    }
    if (!floatCave_.allocateNear(memory, floatSite_)) {
        return false;
    }
    const uptr cave = floatCave_.base();
    const uptr healings = cave + kFlags;
    const uptr grenades = cave + kFlags + 4;
    const uptr proj = cave + kFlags + 8;
    const uptr ohk = cave + kFlags + 0x0C;
    const uptr dmg = cave + kFlags + 0x10;
    const uptr defm = cave + kFlags + 0x14;
    const uptr maxv = cave + 0x500;
    const uptr minv = cave + 0x504;
    const uptr puppet = playerCave_.base() ? playerCave_.base() + kPtrs : cave + kPtrs + 0x40;
    const uptr healthFl = cave + kFlags + 0x18;
    const uptr lastRbp = cave + kPtrs;
    const uptr lastRbx = cave + kPtrs + 8;
    const uptr lastR9 = cave + kPtrs + 16;
    const uptr lastHash = cave + kPtrs + 24;

    CaveEmit e;
    e.base = cave;
    // rbp = StatsObjectID*: entityHash==1 и idType==Entity — это V.
    // Иначе сверка hash с полями куклы. 9999 только игроку, никогда всем.
    e.comissXmm6Rip(maxv);
    e.jae32("code");
    e.bytes({0x83, 0xFE, 0x11});
    e.je32("hp");
    e.bytes({0x45, 0x33, 0xC9});  // xor r9d, r9d
    e.bytes({0x83, 0xFE, 0x0F});
    e.jne8("p1");
    e.movR9dRip(grenades);
    e.jmp8("prevent");
    e.L("p1");
    e.bytes({0x83, 0xFE, 0x1D});
    e.jne8("p2");
    e.movR9dRip(proj);
    e.jmp8("prevent");
    e.L("p2");
    e.bytes({0x83, 0xFE, 0x10});
    e.jne8("p3");
    e.movR9dRip(healings);
    e.jmp8("prevent");
    e.L("p3");
    e.jmp32("code");
    e.L("hp");
    e.movRipRbp(lastRbp);
    e.movRipRbx(lastRbx);
    e.movRipR9(lastR9);
    e.bytes({0x48, 0x89, 0xE8});        // mov rax, rbp  ; rdx = StatsObjectID*
    e.bytes({0x48, 0xC1, 0xE8, 0x2F});  // shr rax, 47
    e.jne32("npc");
    e.bytes({0x48, 0x81, 0xFD, 0x00, 0x00, 0x01, 0x00});  // cmp rbp, 0x10000
    e.jb32("npc");
    e.bytes({0x48, 0x8B, 0x45, 0x00});  // mov rax, [rbp]  entityHash
    e.movRipRax(lastHash);
    e.bytes({0x48, 0x83, 0xF8, 0x01});  // cmp rax, 1  ; V
    e.jne32("tryent");
    e.bytes({0x80, 0x7D, 0x08, 0x00});  // cmp byte [rbp+8], EntityID
    e.je32("pl");
    e.L("tryent");
    e.movRaxRip(puppet);
    e.bytes({0x48, 0x85, 0xC0});
    e.je32("npc");
    e.bytes({0x48, 0x8B, 0x48, 0x40});  // mov rcx, [puppet+0x40]
    e.bytes({0x48, 0x3B, 0x4D, 0x00});  // cmp rcx, [rbp]
    e.je32("pl");
    e.bytes({0x48, 0x8B, 0x48, 0x48});
    e.bytes({0x48, 0x3B, 0x4D, 0x00});
    e.je32("pl");
    e.bytes({0x48, 0x8B, 0x48, 0x50});
    e.bytes({0x48, 0x3B, 0x4D, 0x00});
    e.je32("pl");
    e.bytes({0x48, 0x8B, 0x48, 0x58});
    e.bytes({0x48, 0x3B, 0x4D, 0x00});
    e.jne32("npc");
    e.L("pl");
    e.cmpRip1(healthFl);
    e.jne8("pldef");
    e.movssXmm6Rip(maxv);
    e.jmp32("code");
    e.L("pldef");
    e.cmpRip0(defm);
    e.je32("code");
    e.divssXmm6Rip(defm);
    e.jmp32("code");
    e.L("npc");
    e.cmpRip0(dmg);
    e.je8("ohkchk");
    e.mulssXmm6Rip(dmg);
    e.L("ohkchk");
    e.cmpRip1(ohk);
    e.jne32("code");
    e.movssXmm6Rip(minv);
    e.jmp32("code");
    e.L("prevent");
    e.bytes({0x41, 0x83, 0xF9, 0x01});
    e.jne32("code");
    e.bytes({0x0F, 0x57, 0xF6});
    e.L("code");
    e.bytes({0xF3, 0x0F, 0x58, 0xCE, 0x45, 0x33, 0xC9});
    e.absJmp(floatSite_ + 7);

    const float hi = 9999.0f;
    const float lo = -9999.0f;
    u32 hiB = 0, loB = 0;
    std::memcpy(&hiB, &hi, 4);
    std::memcpy(&loB, &lo, 4);
    for (usize i = 0; i < 0x1C; i += 4) {
        floatCave_.writeU32(memory, kFlags + i, 0);
    }
    floatCave_.writeU32(memory, 0x500, hiB);
    floatCave_.writeU32(memory, 0x504, loB);
    floatCave_.writeU64(memory, kPtrs, 0);
    floatCave_.writeU64(memory, kPtrs + 8, 0);
    floatCave_.writeU64(memory, kPtrs + 16, 0);
    floatCave_.writeU64(memory, kPtrs + 24, 0);
    floatCave_.writeU64(memory, kPtrs + 0x40, 0);
    if (!commitCave(memory, floatCave_, floatSite_, 7, e, "float")) {
        return false;
    }
    floatHooked_ = true;
    return true;
}

bool GameHooks::installStaminaHook(ProcessMemory& memory) {
    if (staminaHooked_ || staminaSite_ == 0) {
        return staminaHooked_;
    }
    if (!staminaCave_.allocateNear(memory, staminaSite_)) {
        return false;
    }
    const uptr cave = staminaCave_.base();
    const uptr st = cave + kFlags;
    const uptr ram = cave + kFlags + 4;
    const uptr mx = cave + 0x300;
    CaveEmit e;
    e.base = cave;
    e.bytes({0xF3, 0x0F, 0x10, 0x89});
    e.put32(staminaDisp_);
    e.cmpRip1(st);
    e.jne8("ram");
    e.bytes({0x83, 0x79, 0x30, 0x23});
    e.jne8("ram");
    e.jmp8("setmax");
    e.L("ram");
    e.cmpRip1(ram);
    e.jne8("code");
    e.bytes({0x83, 0x79, 0x30, 0x16});
    e.jne8("code");
    e.L("setmax");
    e.comissXmm1Rip(mx);
    e.jae8("code");
    e.movssXmm1Rip(mx);
    e.bytes({0xF3, 0x0F, 0x11, 0x89});
    e.put32(staminaDisp_);
    e.L("code");
    e.absJmp(staminaSite_ + 8);
    staminaCave_.writeU32(memory, kFlags, 0);
    staminaCave_.writeU32(memory, kFlags + 4, 0);
    const float cap = 9999.0f;
    u32 bits = 0;
    std::memcpy(&bits, &cap, 4);
    staminaCave_.writeU32(memory, 0x300, bits);
    if (!commitCave(memory, staminaCave_, staminaSite_, 8, e, "stamina")) {
        return false;
    }
    staminaHooked_ = true;
    return true;
}

bool GameHooks::installItemDecHook(ProcessMemory& memory) {
    if (itemDecHooked_ || itemDecSite_ == 0) {
        return itemDecHooked_;
    }
    if (!inventoryCave_.base()) {
        return false;
    }
    const uptr site = itemDecSite_ + 0x18;
    if (!itemDecCave_.allocateNear(memory, site)) {
        return false;
    }
    const uptr playerInv = inventoryCave_.base() + kPtrs;
    const uptr isInv = itemCodeCave_.base() ? itemCodeCave_.base() + kPtrs : inventoryCave_.base() + kPtrs + 8;
    CaveEmit e;
    e.base = itemDecCave_.base();
    e.movRaxRip(playerInv);
    e.bytes({0x48, 0x85, 0xC0});  // test rax,rax
    e.je8("code");
    e.bytes({0x48, 0x3B, 0xC6});  // cmp rax,rsi
    e.jne8("code");
    e.movImmRip(isInv, 1);
    e.L("code");
    e.bytes({0x44, 0x8B, 0xCD, 0x45, 0x8B, 0xC6});
    e.absJmp(site + 6);
    if (!commitCave(memory, itemDecCave_, site, 6, e, "itemdec")) {
        return false;
    }
    itemDecHooked_ = true;
    return true;
}

bool GameHooks::installItemCodeHook(ProcessMemory& memory) {
    if (itemCodeHooked_ || itemCodeSite_ == 0) {
        return itemCodeHooked_;
    }
    if (!itemCodeCave_.allocateNear(memory, itemCodeSite_)) {
        return false;
    }
    const uptr cave = itemCodeCave_.base();
    const uptr items = cave + kFlags;
    const uptr nodec = cave + kFlags + 4;
    CaveEmit e;
    e.base = cave;
    e.bytes({0x83, 0xFB, 0x00});              // cmp ebx, 0  (уменьшение стака)
    e.jge8("code");
    e.cmpRip1(items);
    e.je8("block");
    e.cmpRip1(nodec);
    e.jne8("code");
    e.L("block");
    e.bytes({0x33, 0xDB});                    // xor ebx,ebx — не тратить
    e.L("code");
    e.bytes({0x8B, 0x4F, 0x78, 0xBA, 0x01, 0x00, 0x00, 0x00});
    e.absJmp(itemCodeSite_ + 8);
    itemCodeCave_.writeU32(memory, kFlags, 0);
    itemCodeCave_.writeU32(memory, kFlags + 4, 0);
    itemCodeCave_.writeU32(memory, kPtrs, 0);
    if (!commitCave(memory, itemCodeCave_, itemCodeSite_, 8, e, "itemcode")) {
        return false;
    }
    itemCodeHooked_ = true;
    return true;
}

bool GameHooks::installStatsIdHook(ProcessMemory& memory) {
    if (statsIdHooked_ || statsIdSite_ == 0) {
        return statsIdHooked_;
    }
    if (!statsIdCave_.allocateNear(memory, statsIdSite_)) {
        return false;
    }
    const uptr cave = statsIdCave_.base();
    const uptr spd = cave + kPtrs;
    const uptr w = cave + kPtrs + 4;
    const uptr cap = cave + kPtrs + 8;
    CaveEmit e;
    e.base = cave;
    e.bytes({0x8B, 0x13});            // mov edx,[rbx]
    e.bytes({0x81, 0xFA, 0xCB, 0x03, 0x00, 0x00});  // MaxSpeed=971
    e.jne8("w");
    e.movImmRip(spd, 1);
    e.jmp8("code");
    e.L("w");
    e.bytes({0x81, 0xFA, 0x90, 0x01, 0x00, 0x00});  // CarryCapacity=400
    e.jne8("c");
    e.movImmRip(w, 1);
    e.L("c");
    e.bytes({0x81, 0xFA, 0x45, 0x03, 0x00, 0x00});  // Humanity=837
    e.jne8("code");
    e.movImmRip(cap, 1);
    e.L("code");
    e.bytes({0x81, 0x3B, 0xB1, 0x06, 0x00, 0x00});
    e.absJmp(statsIdSite_ + 6);
    statsIdCave_.writeU32(memory, kPtrs, 0);
    statsIdCave_.writeU32(memory, kPtrs + 4, 0);
    statsIdCave_.writeU32(memory, kPtrs + 8, 0);
    if (!commitCave(memory, statsIdCave_, statsIdSite_, 6, e, "statsid")) {
        return false;
    }
    statsIdHooked_ = true;
    return true;
}

bool GameHooks::installStatsPtrHook(ProcessMemory& memory) {
    if (statsPtrHooked_ || statsPtrSite_ == 0) {
        return statsPtrHooked_;
    }
    if (!statsPtrCave_.allocateNear(memory, statsPtrSite_)) {
        return false;
    }
    const uptr cave = statsPtrCave_.base();
    const uptr attrs = cave + kFlags;
    const uptr perks = cave + kFlags + 4;
    const uptr level = cave + kFlags + 8;
    const uptr street = cave + kFlags + 12;
    const uptr relics = cave + kFlags + 16;
    const uptr sk1 = cave + kFlags + 20;
    const uptr sk2 = cave + kFlags + 24;
    const uptr sk3 = cave + kFlags + 28;
    const uptr sk4 = cave + kFlags + 32;
    const uptr sk5 = cave + kFlags + 36;
    const uptr statsbase = cave + kPtrs;

    CaveEmit e;
    e.base = cave;
    e.bytes({0x48, 0x8B, 0x0E, 0x4C, 0x8B, 0xC0});  // mov rcx,[rsi]; mov r8,rax
    e.bytes({0x50, 0x53});                          // push rax; push rbx
    e.bytes({0x48, 0x2B, 0xC4, 0x48, 0x85, 0xC0});  // sub rax,rsp; test rax,rax
    e.jcc8(0x79, "absok");                          // jns
    e.bytes({0x48, 0xF7, 0xD8});                    // neg rax
    e.L("absok");
    e.bytes({0x48, 0x3D, 0x00, 0x00, 0x01, 0x00});  // cmp rax,10000
    e.jb32("out");
    e.movAbsRax(0x5E786407AC10FD38ULL);
    e.bytes({0x48, 0x39, 0x46, 0x08});              // cmp [rsi+8],rax
    e.jne32("points");
    e.bytes({0x83, 0x7E, 0x20, 0x04});              // cmp [rsi+20],4
    e.jne32("out");
    e.bytes({0x49, 0x8D, 0x98, 0x9C, 0xFD, 0xFF, 0xFF});  // lea rbx,[r8-0x264]
    e.movRipRbx(statsbase);
    e.bytes({0x41, 0x8B, 0x58, 0xFC});              // mov ebx,[r8-4]
    e.bytes({0x83, 0xFB, 0x0E});
    e.je8("lv");
    e.bytes({0x83, 0xFB, 0x11});
    e.je8("sc");
    e.bytes({0x83, 0xFB, 0x05});
    e.je8("s1");
    e.bytes({0x83, 0xFB, 0x0C});
    e.je8("s2");
    e.bytes({0x83, 0xFB, 0x0F});
    e.je8("s3");
    e.bytes({0x83, 0xFB, 0x12});
    e.je8("s4");
    e.bytes({0x83, 0xFB, 0x13});
    e.je8("s5");
    e.jmp32("out");
    e.L("lv");
    e.leaRbxRip(level);
    e.jmp32("editv");
    e.L("sc");
    e.leaRbxRip(street);
    e.jmp32("editv");
    e.L("s1");
    e.leaRbxRip(sk1);
    e.jmp32("editv");
    e.L("s2");
    e.leaRbxRip(sk2);
    e.jmp32("editv");
    e.L("s3");
    e.leaRbxRip(sk3);
    e.jmp32("editv");
    e.L("s4");
    e.leaRbxRip(sk4);
    e.jmp32("editv");
    e.L("s5");
    e.leaRbxRip(sk5);
    e.jmp32("editv");
    e.L("points");
    e.cmpRipImm8(attrs, 0xFF);
    e.jne32("ptchk");
    e.cmpRipImm8(perks, 0xFF);
    e.jne32("ptchk");
    e.cmpRipImm8(relics, 0xFF);
    e.je32("out");
    e.L("ptchk");
    e.movAbsRax(0x6074BCB8FC388F56ULL);
    e.bytes({0x48, 0x39, 0x46, 0x08});
    e.jne32("out");
    e.bytes({0x83, 0x7E, 0x20, 0x08});
    e.jne32("out");
    e.bytes({0x41, 0x8B, 0x58, 0xF8});              // mov ebx,[r8-8]
    e.bytes({0x83, 0xFB, 0x00});
    e.je8("attr");
    e.bytes({0x83, 0xFB, 0x02});
    e.je8("perk");
    e.bytes({0x83, 0xFB, 0x01});
    e.je8("rel");
    e.jmp32("out");
    e.L("attr");
    e.leaRbxRip(attrs);
    e.jmp32("editp");
    e.L("perk");
    e.leaRbxRip(perks);
    e.jmp32("editp");
    e.L("rel");
    e.leaRbxRip(relics);
    e.jmp32("editp");
    e.L("editv");
    e.bytes({0x8B, 0x03});                          // mov eax,[rbx]  flag value
    e.bytes({0x83, 0xF8, 0x00});
    e.jle32("out");
    e.bytes({0x41, 0x89, 0x00});                    // mov [r8],eax  — не [rax]
    e.bytes({0xC7, 0x03, 0xFF, 0xFF, 0xFF, 0xFF});
    e.jmp32("out");
    e.L("editp");
    e.bytes({0x8B, 0x03});
    e.bytes({0x83, 0xF8, 0xFF});
    e.je32("out");
    e.bytes({0x41, 0x89, 0x00});
    e.bytes({0xC7, 0x03, 0xFF, 0xFF, 0xFF, 0xFF});
    e.L("out");
    e.bytes({0x5B, 0x58});
    e.absJmp(statsPtrSite_ + 6);

    const u32 unset = 0xFFFFFFFF;
    for (int i = 0; i < 10; ++i) {
        statsPtrCave_.writeU32(memory, kFlags + static_cast<usize>(i) * 4, unset);
    }
    statsPtrCave_.writeU64(memory, kPtrs, 0);
    if (!commitCave(memory, statsPtrCave_, statsPtrSite_, 6, e, "statsptr")) {
        return false;
    }
    statsPtrHooked_ = true;
    return true;
}

bool GameHooks::installStatsIncHook(ProcessMemory& memory) {
    if (statsIncHooked_ || statsIncSite_ == 0) {
        return statsIncHooked_;
    }
    if (!statsIncCave_.allocateNear(memory, statsIncSite_)) {
        return false;
    }
    const uptr cave = statsIncCave_.base();
    const uptr xp = cave + kFlags;
    const uptr xpm = cave + kFlags + 4;
    const uptr sxp = cave + kFlags + 8;
    const uptr sxm = cave + kFlags + 12;
    const uptr sk = cave + kFlags + 16;
    const uptr skm = cave + kFlags + 20;
    const uptr statsbase = statsPtrCave_.base() ? statsPtrCave_.base() + kPtrs : cave + 0x400;

    CaveEmit e;
    e.base = cave;
    e.bytes({0x50, 0x51});                          // push rax; push rcx
    e.leaRaxRip(statsbase);
    e.bytes({0x48, 0x8B, 0x00});                    // mov rax,[rax]
    e.bytes({0x48, 0x3B, 0xD8});                    // cmp rbx,rax
    e.jb32("out");
    e.bytes({0x48, 0x8D, 0x88, 0xD0, 0x04, 0x00, 0x00});  // lea rcx,[rax+0x4D0]
    e.bytes({0x48, 0x3B, 0xD9});                    // cmp rbx,rcx
    e.ja32("out");
    e.bytes({0x8B, 0x43, 0xF0});                    // mov eax,[rbx-10]
    e.bytes({0x83, 0xF8, 0x0E});
    e.je32("isxp");
    e.bytes({0x83, 0xF8, 0x11});
    e.je32("isst");
    e.bytes({0x83, 0xF8, 0x05});
    e.je32("issk");
    e.bytes({0x83, 0xF8, 0x0C});
    e.je32("issk");
    e.bytes({0x83, 0xF8, 0x0F});
    e.je32("issk");
    e.bytes({0x83, 0xF8, 0x12});
    e.je32("issk");
    e.bytes({0x83, 0xF8, 0x13});
    e.je32("issk");
    e.jmp32("out");
    e.L("issk");
    e.cmpRip1(sk);
    e.je32("setmax");
    e.cmpRip0(skm);
    e.je32("out");
    e.leaRaxRip(skm);
    e.jmp32("setmul");
    e.L("isxp");
    e.cmpRip1(xp);
    e.je32("setmax");
    e.cmpRip0(xpm);
    e.je32("out");
    e.leaRaxRip(xpm);
    e.jmp32("setmul");
    e.L("isst");
    e.cmpRip1(sxp);
    e.je32("setmax");
    e.cmpRip0(sxm);
    e.je32("out");
    e.leaRaxRip(sxm);
    e.jmp32("setmul");
    e.L("setmax");
    e.bytes({0x81, 0x3B, 0xFF, 0xE0, 0xF5, 0x05});  // cmp [rbx],99999999
    e.jge32("out");
    e.bytes({0xC7, 0x03, 0xFF, 0xE0, 0xF5, 0x05});
    e.jmp32("out");
    e.L("setmul");
    e.bytes({0x83, 0xFA, 0x00});                    // cmp edx,0
    e.jle32("out");
    e.bytes({0x83, 0x7B, 0x04, 0x00});              // cmp [rbx+4],0
    e.jle32("out");
    e.bytes({0x52});                                // push rdx
    e.bytes({0xDB, 0x04, 0x24});                    // fild [rsp]
    e.bytes({0xD8, 0x00});                          // fmul dword [rax]
    e.bytes({0xDB, 0x1C, 0x24});                    // fistp [rsp]
    e.bytes({0x5A});                                // pop rdx
    e.bytes({0x3B, 0x53, 0x04});                    // cmp edx,[rbx+4]
    e.jle32("out");
    e.bytes({0x8B, 0x53, 0x04, 0xFF, 0xC2});        // mov edx,[rbx+4]; inc edx
    e.L("out");
    e.bytes({0x59, 0x58});
    e.bytes({0x03, 0x13, 0x89, 0x13, 0x48, 0x85, 0xF6});
    e.absJmp(statsIncSite_ + 7);
    for (usize i = 0; i < 24; i += 4) {
        statsIncCave_.writeU32(memory, kFlags + i, 0);
    }
    if (!statsPtrCave_.base()) {
        statsIncCave_.writeU64(memory, 0x400, 0);
    }
    if (!commitCave(memory, statsIncCave_, statsIncSite_, 7, e, "statsinc")) {
        return false;
    }
    statsIncHooked_ = true;
    return true;
}

bool GameHooks::installOtherStatsHook(ProcessMemory& memory) {
    if (otherHooked_ || otherStatsSite_ == 0) {
        return otherHooked_;
    }
    if (!otherCave_.allocateNear(memory, otherStatsSite_)) {
        return false;
    }
    const uptr cave = otherCave_.base();
    const uptr w = cave + kFlags;
    const uptr spd = cave + kFlags + 4;
    const uptr cap = cave + kFlags + 8;
    const uptr recFl = cave + kFlags + 12;
    const uptr isSpd = statsIdCave_.base() ? statsIdCave_.base() + kPtrs : cave + 0x400;
    const uptr isW = statsIdCave_.base() ? statsIdCave_.base() + kPtrs + 4 : cave + 0x404;
    const uptr isC = statsIdCave_.base() ? statsIdCave_.base() + kPtrs + 8 : cave + 0x408;
    CaveEmit e;
    e.base = cave;
    e.bytes({0xF3, 0x0F, 0x10, 0x44, 0xC8, 0x0C});  // movss xmm0,[rax+rcx*8+0xC]
    e.bytes({0x81, 0xF9, 0x90, 0x01, 0x00, 0x00});  // cmp ecx, CarryCapacity
    e.jne8("rec");
    e.cmpRip0(w);
    e.je8("rec");
    e.cvtsi2ssXmm0Rip(w);
    e.bytes({0xF3, 0x0F, 0x11, 0x44, 0xC8, 0x0C});
    e.jmp32("done");
    e.L("rec");
    e.bytes({0x81, 0xF9, 0xBA, 0x04, 0x00, 0x00});  // cmp ecx, Recoil=1210
    e.jb8("spd");
    e.bytes({0x81, 0xF9, 0xFC, 0x04, 0x00, 0x00});  // RecoilUseDifferentStatsInADS=1276
    e.jae8("spd");
    e.cmpRip1(recFl);
    e.jne8("spd");
    e.bytes({0x0F, 0x57, 0xC0});
    e.bytes({0xF3, 0x0F, 0x11, 0x44, 0xC8, 0x0C});
    e.jmp32("done");
    e.L("spd");
    e.cmpRip1(isSpd);
    e.jne8("wt");
    e.cmpRip0(spd);
    e.jle8("clr");
    e.mulssXmm0Rip(spd);
    e.jmp8("clr");
    e.L("wt");
    e.cmpRip1(isW);
    e.jne8("cp");
    e.cmpRip0(w);
    e.je8("clr");
    e.cvtsi2ssXmm0Rip(w);
    e.bytes({0xF3, 0x0F, 0x11, 0x44, 0xC8, 0x0C});
    e.jmp8("clr");
    e.L("cp");
    e.cmpRip1(isC);
    e.jne8("clr");
    e.cmpRip1(cap);
    e.jne8("clr");
    e.bytes({0xC7, 0x44, 0xC8, 0x0C, 0x00, 0x00, 0xDC, 0x43});
    e.bytes({0xF3, 0x0F, 0x10, 0x44, 0xC8, 0x0C});
    e.L("clr");
    e.movImmRip0(isSpd);
    e.movImmRip0(isW);
    e.movImmRip0(isC);
    e.L("done");
    e.absJmp(otherStatsSite_ + 6);

    otherCave_.writeU32(memory, kFlags, 0);
    otherCave_.writeU32(memory, kFlags + 4, 0);
    otherCave_.writeU32(memory, kFlags + 8, 0);
    otherCave_.writeU32(memory, kFlags + 12, 0);
    if (!statsIdCave_.base()) {
        otherCave_.writeU32(memory, 0x400, 0);
        otherCave_.writeU32(memory, 0x404, 0);
        otherCave_.writeU32(memory, 0x408, 0);
    }
    if (!commitCave(memory, otherCave_, otherStatsSite_, 6, e, "otherstats")) {
        return false;
    }
    otherHooked_ = true;
    return true;
}

bool GameHooks::installJumpHook(ProcessMemory& memory) {
    if (jumpHooked_ || jumpSite_ == 0) {
        return jumpHooked_;
    }
    if (!jumpCave_.allocateNear(memory, jumpSite_)) {
        return false;
    }
    const uptr cave = jumpCave_.base();
    const uptr enabled = cave + kFlags;
    const uptr once = cave + 0x300;
    const uptr thresh = cave + 0x500;
    const uptr extra = cave + 0x504;

    // mulss xmm8,xmm10; буст только если прыжок (>=4) и один раз.
    // Не трогаем xmm1 — после хука идёт movss [rbx+0x11C], xmm8.
    CaveEmit e;
    e.base = cave;
    e.bytes({0xF3, 0x45, 0x0F, 0x59, 0xC2});
    e.cmpRip1(enabled);
    e.jne8("code");
    e.comissXmm8Rip(thresh);
    e.jb8("reset");
    e.cmpRip0(once);
    e.jne8("code");
    e.movImmRip(once, 1);
    e.addssXmm8Rip(extra);
    e.jmp8("code");
    e.L("reset");
    e.movImmRip0(once);
    e.L("code");
    e.absJmp(jumpSite_ + 5);

    jumpCave_.writeU32(memory, kFlags, 0);
    jumpCave_.writeU32(memory, 0x300, 0);
    const float t = 4.0f;
    const float add = 10.0f;
    u32 tb = 0, ab = 0;
    std::memcpy(&tb, &t, 4);
    std::memcpy(&ab, &add, 4);
    jumpCave_.writeU32(memory, 0x500, tb);
    jumpCave_.writeU32(memory, 0x504, ab);
    if (!commitCave(memory, jumpCave_, jumpSite_, 5, e, "jump")) {
        return false;
    }
    jumpHooked_ = true;
    return true;
}

bool GameHooks::installDjumpHook(ProcessMemory& memory) {
    if (djumpHooked_ || djumpSite_ == 0) {
        return djumpHooked_;
    }
    if (!djumpCave_.allocateNear(memory, djumpSite_)) {
        return false;
    }
    const uptr cave = djumpCave_.base();
    const uptr enabled = cave + kFlags;
    const uptr hash = cave + 0x300;

    // rax остаётся указателем объекта. Писать [rax+10] после затирания rax
    // хешем — это и был краш (супер-прыжок ещё и ставил этот хук).
    CaveEmit e;
    e.base = cave;
    e.bytes({0x48, 0x8B, 0x48, 0x08});  // mov rcx,[rax+8]
    e.cmpRip1(enabled);
    e.jne8("orig");
    e.cmpRcxRip(hash);
    e.jne8("orig");
    e.bytes({0xC7, 0x40, 0x10, 0x00, 0x00, 0x00, 0x00});  // mov dword [rax+10],0 — поле 32-bit
    e.L("orig");
    e.bytes({0x49, 0x3B, 0xC8});  // cmp rcx,r8
    e.absJmp(djumpSite_ + 7);

    djumpCave_.writeU32(memory, kFlags, 0);
    djumpCave_.writeU64(memory, 0x300, 0xED9449382C6F13F3ULL);
    if (!commitCave(memory, djumpCave_, djumpSite_, 7, e, "djump")) {
        return false;
    }
    djumpHooked_ = true;
    return true;
}

bool GameHooks::installDayHook(ProcessMemory& memory) {
    if (dayHooked_ || daySite_ == 0) {
        return dayHooked_;
    }
    const uptr site = daySite_ + 5;
    if (!dayCave_.allocateNear(memory, site)) {
        return false;
    }
    const uptr cave = dayCave_.base();
    const uptr freeze = cave + kFlags;
    const uptr add = cave + kFlags + 4;
    CaveEmit e;
    e.base = cave;
    e.bytes({0x48, 0x8B, 0x01, 0x8B, 0xD7});
    e.cmpRip1(add);
    e.jne8("fr");
    e.movImmRip0(add);
    e.bytes({0xBA, 0x10, 0x0E, 0x00, 0x00});  // 3600
    e.jmp8("code");
    e.L("fr");
    e.cmpRip1(freeze);
    e.jne8("code");
    e.bytes({0x33, 0xD2});
    e.L("code");
    e.absJmp(site + 5);
    dayCave_.writeU32(memory, kFlags, 0);
    dayCave_.writeU32(memory, kFlags + 4, 0);
    if (!commitCave(memory, dayCave_, site, 5, e, "day")) {
        return false;
    }
    dayHooked_ = true;
    return true;
}

bool GameHooks::installFlyHook(ProcessMemory& memory) {
    if (flyHooked_ || flySite_ == 0) {
        return flyHooked_;
    }
    if (!flyCave_.allocateNear(memory, flySite_)) {
        return false;
    }
    const uptr cave = flyCave_.base();
    const uptr mode = cave + kFlags;
    const uptr armed = cave + kFlags + 0x1C;
    const uptr saved = cave + 0x400;
    const uptr heightD = cave + 0x408;

    // mode=0: как оригинал. Первое включение копирует текущий Y, дальше Y = saved + height.
    CaveEmit e;
    e.base = cave;
    e.cmpRip1(mode);
    e.jne8("orig");
    e.cmpRip1(armed);
    e.jne8("addh");
    e.movsdRipXmm1(saved);
    e.movImmRip0(armed);
    e.L("addh");
    e.movsdXmm1Rip(saved);
    e.addsdXmm1Rip(heightD);
    e.L("orig");
    e.bytes({0xF2, 0x0F, 0x11, 0x8E});
    e.put32(flyDisp_);
    e.absJmp(flySite_ + 8);
    flyCave_.writeU32(memory, kFlags, 0);
    flyCave_.writeU32(memory, kFlags + 0x1C, 0);
    writeF32(memory, flyCave_, kFlags + 4, 0.0f);
    writeF32(memory, flyCave_, kFlags + 8, 1.0f);
    flyCave_.writeU64(memory, 0x400, 0);
    flyCave_.writeU64(memory, 0x408, 0);
    if (!commitCave(memory, flyCave_, flySite_, 8, e, "fly")) {
        return false;
    }
    flyHooked_ = true;
    return true;
}

bool GameHooks::installAccuracyHook(ProcessMemory& memory) {
    if (accuracyHooked_ || accuracySite_ == 0) {
        return accuracyHooked_;
    }
    if (!accuracyCave_.allocateNear(memory, accuracySite_)) {
        return false;
    }
    CaveEmit e;
    e.base = accuracyCave_.base();
    e.bytes({0x0F, 0x57, 0xC9});  // xorps xmm1,xmm1
    e.bytes({0xF3, 0x0F, 0x11, 0x89});
    e.put32(accuracyDisp_);
    e.absJmp(accuracySite_ + 8);
    if (!commitCave(memory, accuracyCave_, accuracySite_, 8, e, "accuracy")) {
        return false;
    }
    accuracyHooked_ = true;
    return true;
}

bool GameHooks::installInventoryHook(ProcessMemory& memory) {
    if (inventoryHooked_ || inventorySite_ == 0) {
        return inventoryHooked_;
    }
    if (!inventoryCave_.allocateNear(memory, inventorySite_)) {
        return false;
    }
    const uptr cave = inventoryCave_.base();
    const uptr money = cave + kFlags;
    const uptr comps = cave + kFlags + 4;
    const uptr qh = cave + kFlags + 8;
    const uptr playerInv = cave + kPtrs;
    const uptr compIds = cave + 0x500;
    const uptr qhIds = cave + 0x530;

    CaveEmit e;
    e.base = cave;
    e.bytes({0x48, 0x8B, 0x0B, 0x48, 0x8B, 0xD6});  // mov rcx,[rbx]; mov rdx,rsi
    e.cmpRip0(money);
    e.jne8("work");
    e.cmpRip1(comps);
    e.je8("work");
    e.cmpRip1(qh);
    e.jne32("skip");
    e.L("work");
    e.bytes({0x48, 0x85, 0xC9});                      // test rcx,rcx
    e.je32("skip");
    e.bytes({0x48, 0x89, 0xC8});                      // mov rax,rcx
    e.bytes({0x48, 0xC1, 0xE8, 0x2F});                // shr rax,47
    e.jne32("skip");
    e.bytes({0x48, 0x81, 0xF9, 0x00, 0x00, 0x01, 0x00});
    e.jb32("skip");
    e.movRipRsi(playerInv);
    e.bytes({0x8B, 0x41, 0x40});                      // mov eax,[rcx+0x40]
    e.bytes({0x3D, 0xEC, 0x88, 0xE1, 0xF5});          // cmp eax, F5E188EC (eddie)
    e.jne8("comps");
    e.cmpRip0(money);
    e.je8("comps");
    e.movEdxRip(money);
    e.bytes({0x89, 0x51, 0x78});                      // mov [rcx+0x78],edx
    e.jmp32("skip");
    e.L("comps");
    e.cmpRip1(comps);
    e.jne8("qh");
    e.leaRdxRip(compIds);
    e.L("cloop");
    e.bytes({0x39, 0x02});                            // cmp [rdx],eax
    e.je8("cset");
    e.bytes({0x48, 0x83, 0xC2, 0x04});
    e.bytes({0x83, 0x3A, 0x00});
    e.jne8("cloop");
    e.jmp8("qh");
    e.L("cset");
    e.bytes({0xC7, 0x41, 0x78, 0x0F, 0x27, 0x00, 0x00});  // [rcx+78]=9999
    e.jmp32("skip");
    e.L("qh");
    e.cmpRip1(qh);
    e.jne32("skip");
    e.leaRdxRip(qhIds);
    e.L("qloop");
    e.bytes({0x39, 0x02});
    e.je8("qset");
    e.bytes({0x48, 0x83, 0xC2, 0x04});
    e.bytes({0x83, 0x3A, 0x00});
    e.jne8("qloop");
    e.jmp32("skip");
    e.L("qset");
    e.bytes({0xC7, 0x41, 0x78, 0x0F, 0x27, 0x00, 0x00});
    e.L("skip");
    e.bytes({0x48, 0x8B, 0xD6});  // mov rdx,rsi
    e.absJmp(inventorySite_ + 6);
    if (e.b.size() >= kFlags) {
        lastError_ = "inventory stub overflow";
        return false;
    }

    inventoryCave_.writeU32(memory, kFlags, 0);
    inventoryCave_.writeU32(memory, kFlags + 4, 0);
    inventoryCave_.writeU32(memory, kFlags + 8, 0);
    inventoryCave_.writeU32(memory, kFlags + 12, 0);
    inventoryCave_.writeU64(memory, kPtrs, 0);
    inventoryCave_.writeU64(memory, kPtrs + 8, 0);

    const u32 componentIds[] = {0x5AC5B1CB, 0x09E636CA, 0x0181345A, 0x988865E0,
                                0xDF810683, 0x46885739, 0xA5EC1EA5, 0x3CE54F1F, 0};
    const u32 qhComponentIds[] = {0x62D7084F, 0x6B7F4DC8, 0xB57F7F11, 0x38555834, 0};
    for (int i = 0; i < 9; ++i) {
        inventoryCave_.writeU32(memory, 0x500 + static_cast<usize>(i) * 4, componentIds[i]);
    }
    for (int i = 0; i < 5; ++i) {
        inventoryCave_.writeU32(memory, 0x530 + static_cast<usize>(i) * 4, qhComponentIds[i]);
    }

    if (!commitCave(memory, inventoryCave_, inventorySite_, 6, e, "inventory")) {
        return false;
    }
    inventoryHooked_ = true;
    return true;
}

bool GameHooks::installRecoilHook(ProcessMemory& memory) {
    if (recoilCave_.active()) {
        return true;
    }
    if (recoilStoreSite_ == 0) {
        return false;
    }
    const uptr store = recoilStoreSite_;
    if (!recoilCave_.allocateNear(memory, store)) {
        return false;
    }
    CaveEmit e;
    e.base = recoilCave_.base();
    e.bytes({0x0F, 0x57, 0xF6});  // xorps xmm6,xmm6
    e.bytes({0x31, 0xF6});        // xor esi,esi  (yaw @ +0x2B4)
    e.bytes({0xF3, 0x0F, 0x11, 0xB3, 0xB0, 0x02, 0x00, 0x00});
    e.absJmp(store + 8);
    if (!commitCave(memory, recoilCave_, store, 8, e, "recoil")) {
        return false;
    }
    return true;
}

bool GameHooks::tryResolveSpeed(ProcessMemory& memory) {
    if (speedReady_) {
        return true;
    }
    qpcIat_.clear();
    gtcIat_.clear();
    collectIatSlots(memory, remoteProc(memory, L"kernel32.dll", "QueryPerformanceCounter"), qpcIat_);
    collectIatSlots(memory, remoteProc(memory, L"kernelbase.dll", "QueryPerformanceCounter"), qpcIat_);
    collectIatSlots(memory, remoteProc(memory, L"kernel32.dll", "GetTickCount64"), gtcIat_);
    collectIatSlots(memory, remoteProc(memory, L"kernelbase.dll", "GetTickCount64"), gtcIat_);
    auto uniq = [](std::vector<uptr>& v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    };
    uniq(qpcIat_);
    uniq(gtcIat_);
    speedReady_ = !qpcIat_.empty() || !gtcIat_.empty();
    if (speedReady_) {
        Logger::instance().info("speed IAT QPC=" + std::to_string(qpcIat_.size()) +
                                " GTC64=" + std::to_string(gtcIat_.size()));
    }
    return speedReady_;
}

void GameHooks::restoreSpeedIat(ProcessMemory& memory) {
    for (usize i = 0; i < qpcIat_.size() && i < qpcIatOrig_.size(); ++i) {
        memory.write(qpcIat_[i], qpcIatOrig_[i]);
    }
    for (usize i = 0; i < gtcIat_.size() && i < gtcIatOrig_.size(); ++i) {
        memory.write(gtcIat_[i], gtcIatOrig_[i]);
    }
}

bool GameHooks::installSpeedHook(ProcessMemory& memory) {
    if (speedHooked_) {
        return true;
    }
    if (!speedReady_ && !tryResolveSpeed(memory)) {
        lastError_ = "IAT QueryPerformanceCounter/GetTickCount64 не найден";
        return false;
    }
    if (!speedCave_.ensureAllocated(memory, 0x1000)) {
        return false;
    }
    const uptr cave = speedCave_.base();
    const uptr enabled = cave + kFlags;
    const uptr speed = cave + kFlags + 8;
    const uptr origQpc = cave + kPtrs;
    const uptr origGtc = cave + kPtrs + 8;
    const uptr realQpc = cave + kPtrs + 0x10;
    const uptr fakeQpc = cave + kPtrs + 0x18;
    const uptr realGtc = cave + kPtrs + 0x20;
    const uptr fakeGtc = cave + kPtrs + 0x28;
    constexpr usize kGtcOff = 0x100;

    u64 qpcFn = 0;
    u64 gtcFn = 0;
    if (!qpcIat_.empty()) {
        memory.read(qpcIat_.front(), qpcFn);
    }
    if (!gtcIat_.empty()) {
        memory.read(gtcIat_.front(), gtcFn);
    }
    if (qpcFn == 0) {
        qpcFn = remoteProc(memory, L"kernel32.dll", "QueryPerformanceCounter");
    }
    if (gtcFn == 0) {
        gtcFn = remoteProc(memory, L"kernel32.dll", "GetTickCount64");
    }

    CaveEmit q;
    q.base = cave;
    q.bytes({0x51});
    q.bytes({0x48, 0x83, 0xEC, 0x20});
    q.movRaxRip(origQpc);
    q.bytes({0xFF, 0xD0});
    q.bytes({0x48, 0x83, 0xC4, 0x20});
    q.bytes({0x59});
    q.bytes({0x50});
    q.cmpRip1(enabled);
    q.jne8("qdone");
    q.bytes({0x48, 0x8B, 0x01});
    q.cmpQwordRip0(realQpc);
    q.jne8("qscale");
    q.movRipRax(realQpc);
    q.movRipRax(fakeQpc);
    q.jmp8("qdone");
    q.L("qscale");
    q.subRaxRip(realQpc);
    q.bytes({0xF2, 0x48, 0x0F, 0x2A, 0xC0});  // cvtsi2sd xmm0,rax
    q.mulsdXmm0Rip(speed);
    q.bytes({0xF2, 0x48, 0x0F, 0x2C, 0xC0});  // cvttsd2si rax,xmm0
    q.addRaxRip(fakeQpc);
    q.bytes({0x48, 0x89, 0x01});              // mov [rcx],rax
    q.L("qdone");
    q.bytes({0x58, 0xC3});                    // pop rax; ret

    CaveEmit g;
    g.base = cave + kGtcOff;
    g.bytes({0x48, 0x83, 0xEC, 0x28});
    g.movRaxRip(origGtc);
    g.bytes({0xFF, 0xD0});
    g.bytes({0x48, 0x83, 0xC4, 0x28});
    g.cmpRip1(enabled);
    g.jne8("gdone");
    g.cmpQwordRip0(realGtc);
    g.jne8("gscale");
    g.movRipRax(realGtc);
    g.movRipRax(fakeGtc);
    g.jmp8("gdone");
    g.L("gscale");
    g.subRaxRip(realGtc);
    g.bytes({0xF2, 0x48, 0x0F, 0x2A, 0xC0});
    g.mulsdXmm0Rip(speed);
    g.bytes({0xF2, 0x48, 0x0F, 0x2C, 0xC0});
    g.addRaxRip(fakeGtc);
    g.L("gdone");
    g.bytes({0xC3});

    std::string why;
    if (!q.finish(&why) || !g.finish(&why)) {
        lastError_ = "speed: " + why;
        return false;
    }
    if (q.b.size() > kGtcOff) {
        lastError_ = "speed QPC stub overflow";
        return false;
    }
    if (!speedCave_.write(memory, 0, q.b) || !speedCave_.write(memory, kGtcOff, g.b)) {
        return false;
    }

    speedCave_.writeU32(memory, kFlags, 0);
    const double one = 1.0;
    u64 oneBits = 0;
    std::memcpy(&oneBits, &one, 8);
    speedCave_.writeU64(memory, kFlags + 8, oneBits);
    speedCave_.writeU64(memory, kPtrs, qpcFn);
    speedCave_.writeU64(memory, kPtrs + 8, gtcFn);
    speedCave_.writeU64(memory, kPtrs + 0x10, 0);
    speedCave_.writeU64(memory, kPtrs + 0x18, 0);
    speedCave_.writeU64(memory, kPtrs + 0x20, 0);
    speedCave_.writeU64(memory, kPtrs + 0x28, 0);

    using SetCfgFn = BOOL(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, CFG_CALL_TARGET_INFO*);
    if (const auto setCfg = reinterpret_cast<SetCfgFn>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessValidCallTargets"))) {
        CFG_CALL_TARGET_INFO tgt[2]{};
        tgt[0].Offset = 0;
        tgt[0].Flags = CFG_CALL_TARGET_VALID;
        tgt[1].Offset = kGtcOff;
        tgt[1].Flags = CFG_CALL_TARGET_VALID;
        if (!setCfg(memory.handle(), reinterpret_cast<PVOID>(cave), 0x1000, 2, tgt)) {
            Logger::instance().warn("CFG whitelist speed cave: " + lastWinError());
        }
    }

    qpcIatOrig_.clear();
    gtcIatOrig_.clear();
    const u64 qpcStub = cave;
    const u64 gtcStub = cave + kGtcOff;
    std::vector<uptr> qpcPatched;
    std::vector<u64> qpcOrig;
    for (uptr slot : qpcIat_) {
        u64 cur = 0;
        if (!memory.read(slot, cur) || cur == 0 || cur == qpcStub) {
            continue;
        }
        if (!memory.write(slot, qpcStub)) {
            lastError_ = "не записан IAT QPC";
            qpcIat_ = qpcPatched;
            qpcIatOrig_ = qpcOrig;
            restoreSpeedIat(memory);
            return false;
        }
        qpcPatched.push_back(slot);
        qpcOrig.push_back(cur);
    }
    qpcIat_ = std::move(qpcPatched);
    qpcIatOrig_ = std::move(qpcOrig);
    std::vector<uptr> gtcPatched;
    std::vector<u64> gtcOrig;
    for (uptr slot : gtcIat_) {
        u64 cur = 0;
        if (!memory.read(slot, cur) || cur == 0 || cur == gtcStub) {
            continue;
        }
        if (!memory.write(slot, gtcStub)) {
            continue;
        }
        gtcPatched.push_back(slot);
        gtcOrig.push_back(cur);
    }
    gtcIat_ = std::move(gtcPatched);
    gtcIatOrig_ = std::move(gtcOrig);

    if (qpcIat_.empty() && gtcIat_.empty()) {
        lastError_ = "IAT слоты не записались";
        return false;
    }
    speedHooked_ = true;
    Logger::instance().info("speed ON cave=0x" + toHex(cave));
    return true;
}

bool GameHooks::applyReload(ProcessMemory& memory, bool on) {
    if (!reloadSite_) {
        return false;
    }
    if (on) {
        const u8 nop[3] = {0x90, 0x90, 0x90};
        return reloadPatch_.apply(memory, reloadSite_, nop);
    }
    return reloadPatch_.revert(memory);
}

bool GameHooks::applyRecoil(ProcessMemory& memory, bool on) {
    if (!recoilSiteFound()) {
        return false;
    }
    if (!on) {
        recoilPatch_.revert(memory);
        recoilYawPatch_.revert(memory);
        recoilLoadPatch_.revert(memory);
        recoilMul1Patch_.revert(memory);
        recoilMul2Patch_.revert(memory);
        recoilCave_.removeHook(memory);
        writeU32(memory, otherCave_, kFlags + 12, 0);
        recoilHooked_ = false;
        return true;
    }
    bool ok = false;
    const u8 nops8[8] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    if (recoilSite_) {
        ok = recoilPatch_.apply(memory, recoilSite_, nops8) || ok;
        ok = recoilYawPatch_.apply(memory, recoilSite_ + 0x18, nops8) || ok;
    }
    if (recoilLoadSite_) {
        const u8 load[16] = {0x0F, 0x57, 0xC0, 0x0F, 0x57, 0xC9, 0x90, 0x90,
                             0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        ok = recoilLoadPatch_.apply(memory, recoilLoadSite_, load) || ok;
    }
    if (recoilMul1Site_) {
        const u8 m1[8] = {0x0F, 0x57, 0xC9, 0x90, 0x90, 0x90, 0x90, 0x90};
        ok = recoilMul1Patch_.apply(memory, recoilMul1Site_, m1) || ok;
    }
    if (recoilMul2Site_) {
        const u8 m2[8] = {0x0F, 0x57, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90};
        ok = recoilMul2Patch_.apply(memory, recoilMul2Site_, m2) || ok;
    }
    if (recoilStoreSite_) {
        ok = installRecoilHook(memory) || ok;
    }
    if (ensureOtherStatsHook(memory)) {
        writeU32(memory, otherCave_, kFlags + 12, 1);
        ok = true;
    }
    recoilHooked_ = ok;
    return ok;
}

bool GameHooks::applyStealth(ProcessMemory& memory, bool on) {
    if (!stealthSite_) {
        return false;
    }
    if (on) {
        const u8 p[6] = {0x41, 0x38, 0x76, 0x08, 0x90, 0xE9};
        return stealthPatch_.apply(memory, stealthSite_, p);
    }
    return stealthPatch_.revert(memory);
}

bool GameHooks::applyBreachFreeze(ProcessMemory& memory, bool on) {
    if (!hackSite_) {
        return false;
    }
    if (on) {
        const u8 nop[4] = {0x90, 0x90, 0x90, 0x90};
        return hackPatch_.apply(memory, hackSite_, nop);
    }
    return hackPatch_.revert(memory);
}

bool GameHooks::applyAccuracy(ProcessMemory& memory, bool on) {
    if (!on) {
        accuracyCave_.removeHook(memory);
        accuracyHooked_ = false;
        return true;
    }
    return installAccuracyHook(memory);
}

void GameHooks::setHealthFlag(ProcessMemory& memory, bool on) {
    healthOn_ = on;
    if (!on) {
        healthPeak_ = 0.0f;
    }
    writeU32(memory, floatCave_, kFlags + 0x18, on ? 1 : 0);
    writeU32(memory, healthCave_, kFlags, on ? 1 : 0);
}
void GameHooks::setStaminaFlag(ProcessMemory& memory, bool on) {
    writeU32(memory, staminaCave_, kFlags, on ? 1 : 0);
}
void GameHooks::setRamFlag(ProcessMemory& memory, bool on) {
    writeU32(memory, staminaCave_, kFlags + 4, on ? 1 : 0);
}
void GameHooks::setHealNoCd(ProcessMemory& memory, bool on) {
    writeU32(memory, floatCave_, kFlags, on ? 1 : 0);
}
void GameHooks::setGrenadeNoCd(ProcessMemory& memory, bool on) {
    writeU32(memory, floatCave_, kFlags + 4, on ? 1 : 0);
}
void GameHooks::setProjNoCd(ProcessMemory& memory, bool on) {
    writeU32(memory, floatCave_, kFlags + 8, on ? 1 : 0);
}
void GameHooks::setOhk(ProcessMemory& memory, bool on) {
    writeU32(memory, floatCave_, kFlags + 0x0C, on ? 1 : 0);
}
void GameHooks::setDmgMul(ProcessMemory& memory, float mul) {
    writeF32(memory, floatCave_, kFlags + 0x10, mul);
}
void GameHooks::setDefMul(ProcessMemory& memory, float mul) {
    writeF32(memory, floatCave_, kFlags + 0x14, mul);
}
void GameHooks::setItemsInf(ProcessMemory& memory, bool on) {
    writeU32(memory, itemCodeCave_, kFlags, on ? 1 : 0);
}
void GameHooks::setItemsNoDec(ProcessMemory& memory, bool on) {
    writeU32(memory, itemCodeCave_, kFlags + 4, on ? 1 : 0);
}
void GameHooks::setXpInf(ProcessMemory& memory, bool on) {
    writeU32(memory, statsIncCave_, kFlags, on ? 1 : 0);
}
void GameHooks::setXpMul(ProcessMemory& memory, float mul) {
    writeF32(memory, statsIncCave_, kFlags + 4, mul);
}
void GameHooks::setStreetXpInf(ProcessMemory& memory, bool on) {
    writeU32(memory, statsIncCave_, kFlags + 8, on ? 1 : 0);
}
void GameHooks::setStreetXpMul(ProcessMemory& memory, float mul) {
    writeF32(memory, statsIncCave_, kFlags + 12, mul);
}
void GameHooks::setSkillProgInf(ProcessMemory& memory, bool on) {
    writeU32(memory, statsIncCave_, kFlags + 16, on ? 1 : 0);
}
void GameHooks::setSkillProgMul(ProcessMemory& memory, float mul) {
    writeF32(memory, statsIncCave_, kFlags + 20, mul);
}
void GameHooks::setAttributePoints(ProcessMemory& memory, i32 v) {
    writeU32(memory, statsPtrCave_, kFlags, static_cast<u32>(v));
}
void GameHooks::setPerkPoints(ProcessMemory& memory, i32 v) {
    writeU32(memory, statsPtrCave_, kFlags + 4, static_cast<u32>(v));
}
void GameHooks::setRelicPoints(ProcessMemory& memory, i32 v) {
    writeU32(memory, statsPtrCave_, kFlags + 16, static_cast<u32>(v));
}
void GameHooks::setPlayerLevel(ProcessMemory& memory, i32 v) {
    writeU32(memory, statsPtrCave_, kFlags + 8, static_cast<u32>(v));
}
void GameHooks::setStreetCredLevel(ProcessMemory& memory, i32 v) {
    writeU32(memory, statsPtrCave_, kFlags + 12, static_cast<u32>(v));
}
void GameHooks::setSkillLevel(ProcessMemory& memory, int index, i32 v) {
    if (index < 0 || index > 4) {
        return;
    }
    writeU32(memory, statsPtrCave_, kFlags + 20 + static_cast<usize>(index) * 4, static_cast<u32>(v));
}
void GameHooks::setMoveSpeed(ProcessMemory& memory, float mul) {
    writeF32(memory, otherCave_, kFlags + 4, mul);
}
void GameHooks::setMaxWeight(ProcessMemory& memory, i32 v) {
    writeU32(memory, otherCave_, kFlags, static_cast<u32>(v));
}
void GameHooks::setIgnoreCapacity(ProcessMemory& memory, bool on) {
    writeU32(memory, otherCave_, kFlags + 8, on ? 1 : 0);
}
void GameHooks::setFreezeDay(ProcessMemory& memory, bool on) {
    writeU32(memory, dayCave_, kFlags, on ? 1 : 0);
}
void GameHooks::pulseDayAdd(ProcessMemory& memory) {
    writeU32(memory, dayCave_, kFlags + 4, 1);
}
void GameHooks::setFlyMode(ProcessMemory& memory, bool on) {
    flyOn_ = on;
    writeU32(memory, flyCave_, kFlags, on ? 1 : 0);
    writeU32(memory, flyCave_, kFlags + 0x1C, on ? 1 : 0);
    if (!on) {
        writeU32(memory, flyCave_, kFlags + 12, 0);
        writeU32(memory, flyCave_, kFlags + 16, 0);
        writeU32(memory, flyCave_, kFlags + 20, 0);
        writeU32(memory, flyCave_, kFlags + 24, 0);
    }
}
void GameHooks::setFlySpeed(ProcessMemory& memory, float v) {
    writeF32(memory, flyCave_, kFlags + 8, v);
}

void GameHooks::setJumpFlag(ProcessMemory& memory, bool on) {
    writeU32(memory, jumpCave_, kFlags, on ? 1 : 0);
    if (!on) {
        writeU32(memory, jumpCave_, 0x300, 0);
    }
}

void GameHooks::setDjumpFlag(ProcessMemory& memory, bool on) {
    writeU32(memory, djumpCave_, kFlags, on ? 1 : 0);
}

void GameHooks::setFlyHeight(ProcessMemory& memory, float v) {
    writeF32(memory, flyCave_, kFlags + 4, v);
    const double d = static_cast<double>(v);
    u64 bits = 0;
    std::memcpy(&bits, &d, 8);
    flyCave_.writeU64(memory, 0x408, bits);
}

void GameHooks::setMoney(ProcessMemory& memory, i32 v) {
    writeU32(memory, inventoryCave_, kFlags, static_cast<u32>(v));
}

void GameHooks::setComponents(ProcessMemory& memory, bool on) {
    writeU32(memory, inventoryCave_, kFlags + 4, on ? 1 : 0);
}

void GameHooks::setQhComponents(ProcessMemory& memory, bool on) {
    writeU32(memory, inventoryCave_, kFlags + 8, on ? 1 : 0);
}

void GameHooks::setGameSpeed(ProcessMemory& memory, float mul) {
    if (mul < 0.05f) {
        mul = 0.05f;
    }
    if (mul > 20.0f) {
        mul = 20.0f;
    }
    const bool on = mul > 0.05f && (mul < 0.97f || mul > 1.03f);
    if (!on && speedHooked_) {
        restoreSpeedIat(memory);
        speedHooked_ = false;
        Logger::instance().dbg("speed IAT restored");
        return;
    }
    writeU32(memory, speedCave_, kFlags, on ? 1 : 0);
    const double d = static_cast<double>(mul);
    u64 bits = 0;
    std::memcpy(&bits, &d, 8);
    speedCave_.writeU64(memory, kFlags + 8, bits);
    speedCave_.writeU64(memory, kPtrs + 0x10, 0);
    speedCave_.writeU64(memory, kPtrs + 0x18, 0);
    speedCave_.writeU64(memory, kPtrs + 0x20, 0);
    speedCave_.writeU64(memory, kPtrs + 0x28, 0);
}

}  // namespace er
