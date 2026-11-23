#include "core/CrashDebug.h"

#include "core/Logger.h"
#include "core/Utf8.h"
#include "features/FeatureManager.h"
#include "features/IFeature.h"
#include "hooks/GameHooks.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <share.h>
#include <sstream>

#pragma comment(lib, "dbghelp.lib")

namespace er {
namespace {

#ifndef ERTRAINER_DIST
std::mutex gDumpMu;
std::atomic<bool> gDumping{false};
#endif

std::filesystem::path exeDir() {
    wchar_t buf[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buf).parent_path();
}

#ifndef ERTRAINER_DIST
std::string stamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour,
                  st.wMinute, st.wSecond);
    return buf;
}

void writeMinidump(EXCEPTION_POINTERS* ep) {
    if (gDumping.exchange(true)) {
        return;
    }
    const auto path = exeDir() / ("CP2077Trainer-" + stamp() + ".dmp");
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        gDumping = false;
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpWithDataSegs, ep ? &mei : nullptr,
                      nullptr, nullptr);
    CloseHandle(f);
    Logger::instance().error(std::string("minidump: ") + wideToUtf8(path.wstring()));
    gDumping = false;
}

LONG WINAPI onUnhandled(EXCEPTION_POINTERS* ep) {
    DWORD code = 0;
    uptr addr = 0;
    if (ep && ep->ExceptionRecord) {
        code = ep->ExceptionRecord->ExceptionCode;
        addr = reinterpret_cast<uptr>(ep->ExceptionRecord->ExceptionAddress);
    }
    Logger::instance().error("CRASH trainer code=0x" + toHex(code) + " at 0x" + toHex(addr));
    writeCrashReport("trainer unhandled exception",
                     "code=0x" + toHex(code) + " addr=0x" + toHex(addr) + "\n" + hooksDebugDump());
    writeMinidump(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

}  // namespace

void installCrashDebug() {
    SetErrorMode(SEM_FAILCRITICALERRORS);
#ifndef ERTRAINER_DIST
    SetUnhandledExceptionFilter(onUnhandled);
    Logger::instance().dbg("crash debug ON, лог: " + Logger::instance().debugLogPath());
#endif
}

void debugNote(std::string_view msg) {
    Logger::instance().dbg(msg);
}

void writeCrashReport(std::string_view title, std::string_view body) {
#ifdef ERTRAINER_DIST
    (void)title;
    (void)body;
    return;
#else
    std::lock_guard lock(gDumpMu);
    Logger::instance().error(std::string("CRASH REPORT: ") + std::string(title));
    const auto path = exeDir() / ("CP2077Trainer-crash-" + stamp() + ".txt");
    FILE* f = _wfsopen(path.c_str(), L"wb", _SH_DENYNO);
    if (!f) {
        Logger::instance().error("не смог написать crash txt");
        return;
    }
    const auto lines = Logger::instance().recent(400);
    std::ostringstream o;
    o << "CP2077Trainer crash report\n";
    o << title << "\n\n";
    o << body << "\n\n";
    o << "=== last log ===\n";
    for (const auto& line : lines) {
        o << line << "\n";
    }
    const std::string s = o.str();
    std::fwrite(s.data(), 1, s.size(), f);
    std::fflush(f);
    std::fclose(f);
    Logger::instance().error(std::string("crash txt: ") + wideToUtf8(path.wstring()));
#endif
}

void openTrainerLogFolder() {
    const std::string p = Logger::instance().debugLogPath();
    std::filesystem::path dir = p.empty() ? exeDir() : std::filesystem::path(utf8ToWide(p)).parent_path();
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::string featureDebugDump(const FeatureManager& features) {
    std::ostringstream o;
    o << "=== features ===\n";
    for (const auto& f : features.features()) {
        const char* st = "?";
        switch (f->status()) {
            case FeatureStatus::Unavailable:
                st = "UNAVAIL";
                break;
            case FeatureStatus::Ready:
                st = "ready";
                break;
            case FeatureStatus::Enabled:
                st = "ON";
                break;
            case FeatureStatus::Error:
                st = "ERROR";
                break;
        }
        o << st << "  " << f->info().id << "  " << f->reason() << "\n";
    }
    return o.str();
}

std::string hooksDebugDump() {
    return GameHooks::instance().debugSnapshot();
}

std::string formatExitCode(u32 code) {
    char hex[16]{};
    std::snprintf(hex, sizeof(hex), "0x%08X", code);
    std::string s = hex;
    switch (code) {
        case 0:
            s += " (exit 0)";
            break;
        case 1:
            s += " (exit 1)";
            break;
        case STILL_ACTIVE:
            s += " (STILL_ACTIVE)";
            break;
        case 0xC0000005:
            s += " ACCESS_VIOLATION — типичный краш от хука";
            break;
        case 0xC0000006:
            s += " IN_PAGE_ERROR";
            break;
        case 0xC000001D:
            s += " ILLEGAL_INSTRUCTION";
            break;
        case 0xC0000094:
            s += " INTEGER_DIVIDE_BY_ZERO";
            break;
        case 0xC00000FD:
            s += " STACK_OVERFLOW";
            break;
        case 0xC0000374:
            s += " HEAP_CORRUPTION";
            break;
        case 0xC0000409:
            s += " STACK_BUFFER / FAST_FAIL";
            break;
        case 0x80000003:
            s += " BREAKPOINT";
            break;
        default:
            break;
    }
    return s;
}

}  // namespace er
