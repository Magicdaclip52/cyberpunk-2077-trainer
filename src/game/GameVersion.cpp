#include "game/GameVersion.h"

#include "core/Types.h"
#include "core/Utf8.h"

#include <Windows.h>

#include <cstdio>
#include <cwchar>
#include <vector>

namespace er {

GameVersion mapKnownVersion(std::string_view fileVersion) {
    GameVersion v;
    v.fileVersion = std::string(fileVersion);
    v.productVersion = v.fileVersion;
    const bool known25 = fileVersion == kExpectedFileVersion ||
                         fileVersion.rfind("3.0.", 0) == 0 ||
                         fileVersion.rfind("2.5", 0) == 0;
    if (known25) {
        v.displayVersion = kExpectedAppVersion;
        v.known = true;
        v.warning = false;
    } else if (fileVersion.empty()) {
        v.displayVersion = "неизвестно";
        v.known = false;
        v.warning = true;
        v.warningText = "Не удалось прочитать FileVersion Cyberpunk2077.exe.";
    } else {
        v.displayVersion = std::string(fileVersion);
        v.known = false;
        v.warning = true;
        v.warningText = "Версия FileVersion " + std::string(fileVersion) +
                        " не совпадает с ожидаемой " + kExpectedFileVersion +
                        " (App Ver. " + kExpectedAppVersion + "). Запуск разрешён, часть функций может быть недоступна.";
    }
    return v;
}

GameVersion parseFileVersion(std::wstring_view exePath) {
    GameVersion empty = mapKnownVersion("");
    if (exePath.empty()) {
        return empty;
    }
    const std::wstring path(exePath);
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0) {
        return empty;
    }
    std::vector<u8> buf(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, buf.data())) {
        return empty;
    }

    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(buf.data(), L"\\", reinterpret_cast<LPVOID*>(&info), &len) || !info) {
        return empty;
    }

    char fv[32]{};
    std::snprintf(fv, sizeof(fv), "%u.%u.%u.%u",
                  HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                  HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
    struct LangCp {
        WORD lang;
        WORD cp;
    };
    LangCp* trans = nullptr;
    UINT tlen = 0;
    std::string product;
    if (VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&trans), &tlen) &&
        trans && tlen >= sizeof(LangCp)) {
        wchar_t sub[64]{};
        std::swprintf(sub, 64, L"\\StringFileInfo\\%04x%04x\\ProductVersion", trans[0].lang, trans[0].cp);
        wchar_t* prod = nullptr;
        UINT plen = 0;
        if (VerQueryValueW(buf.data(), sub, reinterpret_cast<LPVOID*>(&prod), &plen) && prod) {
            product = wideToUtf8(prod);
        }
    }

    GameVersion v = mapKnownVersion(product.empty() ? fv : product);
    v.fileVersion = fv;
    if (!product.empty()) {
        v.productVersion = product;
        if (product.rfind("2.5", 0) == 0) {
            v.displayVersion = kExpectedAppVersion;
            v.known = true;
            v.warning = false;
            v.warningText.clear();
        }
    }
    return v;
}

}  // namespace er
