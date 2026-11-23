#pragma once

#include "core/Types.h"
#include "memory/BytePatch.h"
#include "memory/CodeCave.h"
#include "memory/ProcessMemory.h"
#include "memory/SignatureScanner.h"

#include <string>
#include <string_view>
#include <vector>

namespace er {

class GameHooks {
public:
    static GameHooks& instance();

    bool resolve(ProcessMemory& memory, SignatureScanner& scanner);
    void shutdown(ProcessMemory& memory);
    void tick(ProcessMemory& memory);

    [[nodiscard]] bool playerSiteFound() const { return playerSite_ != 0; }
    [[nodiscard]] bool healthSiteFound() const { return healthSite_ != 0; }
    [[nodiscard]] bool floatSiteFound() const { return floatSite_ != 0; }
    [[nodiscard]] bool staminaSiteFound() const { return staminaSite_ != 0; }
    [[nodiscard]] bool itemCodeSiteFound() const { return itemCodeSite_ != 0; }
    [[nodiscard]] bool reloadSiteFound() const { return reloadSite_ != 0; }
    [[nodiscard]] bool accuracySiteFound() const { return accuracySite_ != 0; }
    [[nodiscard]] bool recoilSiteFound() const {
        return recoilSite_ != 0 || recoilLoadSite_ != 0 || recoilStoreSite_ != 0;
    }
    [[nodiscard]] bool stealthSiteFound() const { return stealthSite_ != 0; }
    [[nodiscard]] bool hackSiteFound() const { return hackSite_ != 0; }
    [[nodiscard]] bool statsPtrSiteFound() const { return statsPtrSite_ != 0; }
    [[nodiscard]] bool statsIncSiteFound() const { return statsIncSite_ != 0; }
    [[nodiscard]] bool statsIdSiteFound() const { return statsIdSite_ != 0; }
    [[nodiscard]] bool otherStatsSiteFound() const { return otherStatsSite_ != 0; }
    [[nodiscard]] bool jumpSiteFound() const { return jumpSite_ != 0; }
    [[nodiscard]] bool djumpSiteFound() const { return djumpSite_ != 0; }
    [[nodiscard]] bool daySiteFound() const { return daySite_ != 0; }
    [[nodiscard]] bool flySiteFound() const { return flySite_ != 0; }
    [[nodiscard]] bool inventorySiteFound() const { return inventorySite_ != 0; }
    [[nodiscard]] bool speedReady() const { return speedReady_; }

    [[nodiscard]] std::string lastError() const { return lastError_; }
    void noteError(std::string msg) { lastError_ = std::move(msg); }
    [[nodiscard]] std::string debugSnapshot() const;
    [[nodiscard]] std::string liveProbe(ProcessMemory& memory) const;
    [[nodiscard]] std::string probeForFeature(ProcessMemory& memory, std::string_view id) const;

    static constexpr usize kFlags = 0x700;
    static constexpr usize kPtrs = 0x900;

    bool ensurePlayerHook(ProcessMemory& memory);
    bool ensureHealthHook(ProcessMemory& memory);
    bool ensureFloatHook(ProcessMemory& memory);
    bool ensureStaminaHook(ProcessMemory& memory);
    bool ensureItemHooks(ProcessMemory& memory);
    bool ensureStatsHooks(ProcessMemory& memory);
    bool ensureStatsIdHook(ProcessMemory& memory);
    bool ensureStatsPtrHook(ProcessMemory& memory);
    bool ensureStatsIncHook(ProcessMemory& memory);
    bool ensureOtherStatsHook(ProcessMemory& memory);
    bool ensureJumpHook(ProcessMemory& memory);
    bool ensureDjumpHook(ProcessMemory& memory);
    bool ensureDayHook(ProcessMemory& memory);
    bool ensureFlyHook(ProcessMemory& memory);
    bool ensureInventoryHook(ProcessMemory& memory);
    bool ensureRecoilHook(ProcessMemory& memory);
    bool ensureSpeedHook(ProcessMemory& memory);
    bool tryResolveFly(ProcessMemory& memory);
    bool tryResolveSpeed(ProcessMemory& memory);

    bool applyReload(ProcessMemory& memory, bool on);
    bool applyRecoil(ProcessMemory& memory, bool on);
    bool applyStealth(ProcessMemory& memory, bool on);
    bool applyBreachFreeze(ProcessMemory& memory, bool on);
    bool applyAccuracy(ProcessMemory& memory, bool on);

    void setHealthFlag(ProcessMemory& memory, bool on);
    void setStaminaFlag(ProcessMemory& memory, bool on);
    void setRamFlag(ProcessMemory& memory, bool on);
    void setHealNoCd(ProcessMemory& memory, bool on);
    void setGrenadeNoCd(ProcessMemory& memory, bool on);
    void setProjNoCd(ProcessMemory& memory, bool on);
    void setOhk(ProcessMemory& memory, bool on);
    void setDmgMul(ProcessMemory& memory, float mul);
    void setDefMul(ProcessMemory& memory, float mul);
    void setItemsInf(ProcessMemory& memory, bool on);
    void setItemsNoDec(ProcessMemory& memory, bool on);
    void setXpInf(ProcessMemory& memory, bool on);
    void setXpMul(ProcessMemory& memory, float mul);
    void setStreetXpInf(ProcessMemory& memory, bool on);
    void setStreetXpMul(ProcessMemory& memory, float mul);
    void setSkillProgInf(ProcessMemory& memory, bool on);
    void setSkillProgMul(ProcessMemory& memory, float mul);
    void setAttributePoints(ProcessMemory& memory, i32 v);
    void setPerkPoints(ProcessMemory& memory, i32 v);
    void setRelicPoints(ProcessMemory& memory, i32 v);
    void setPlayerLevel(ProcessMemory& memory, i32 v);
    void setStreetCredLevel(ProcessMemory& memory, i32 v);
    void setSkillLevel(ProcessMemory& memory, int index, i32 v);
    void setMoveSpeed(ProcessMemory& memory, float mul);
    void setMaxWeight(ProcessMemory& memory, i32 v);
    void setIgnoreCapacity(ProcessMemory& memory, bool on);
    void setFreezeDay(ProcessMemory& memory, bool on);
    void pulseDayAdd(ProcessMemory& memory);
    void setFlyMode(ProcessMemory& memory, bool on);
    void setFlyHeight(ProcessMemory& memory, float v);
    void setFlySpeed(ProcessMemory& memory, float v);
    void setJumpFlag(ProcessMemory& memory, bool on);
    void setDjumpFlag(ProcessMemory& memory, bool on);
    void setMoney(ProcessMemory& memory, i32 v);
    void setComponents(ProcessMemory& memory, bool on);
    void setQhComponents(ProcessMemory& memory, bool on);
    void setGameSpeed(ProcessMemory& memory, float mul);

private:
    bool installPlayerHook(ProcessMemory& memory);
    bool installHealthHook(ProcessMemory& memory);
    bool installFloatHook(ProcessMemory& memory);
    bool installStaminaHook(ProcessMemory& memory);
    bool installItemCodeHook(ProcessMemory& memory);
    bool installItemDecHook(ProcessMemory& memory);
    bool installStatsIdHook(ProcessMemory& memory);
    bool installStatsPtrHook(ProcessMemory& memory);
    bool installStatsIncHook(ProcessMemory& memory);
    bool installOtherStatsHook(ProcessMemory& memory);
    bool installJumpHook(ProcessMemory& memory);
    bool installDjumpHook(ProcessMemory& memory);
    bool installDayHook(ProcessMemory& memory);
    bool installFlyHook(ProcessMemory& memory);
    bool installAccuracyHook(ProcessMemory& memory);
    bool installInventoryHook(ProcessMemory& memory);
    bool installRecoilHook(ProcessMemory& memory);
    bool installSpeedHook(ProcessMemory& memory);
    void restoreSpeedIat(ProcessMemory& memory);

    bool writeU32(ProcessMemory& memory, CodeCave& cave, usize off, u32 v);
    bool writeF32(ProcessMemory& memory, CodeCave& cave, usize off, float v);

    CodeCave playerCave_{};
    CodeCave healthCave_{};
    CodeCave floatCave_{};
    CodeCave staminaCave_{};
    CodeCave itemCodeCave_{};
    CodeCave itemDecCave_{};
    CodeCave statsIdCave_{};
    CodeCave statsPtrCave_{};
    CodeCave statsIncCave_{};
    CodeCave otherCave_{};
    CodeCave jumpCave_{};
    CodeCave djumpCave_{};
    CodeCave dayCave_{};
    CodeCave flyCave_{};
    CodeCave accuracyCave_{};
    CodeCave inventoryCave_{};
    CodeCave recoilCave_{};
    CodeCave speedCave_{};

    BytePatch reloadPatch_{};
    BytePatch recoilPatch_{};
    BytePatch recoilYawPatch_{};
    BytePatch recoilLoadPatch_{};
    BytePatch recoilMul1Patch_{};
    BytePatch recoilMul2Patch_{};
    BytePatch stealthPatch_{};
    BytePatch hackPatch_{};

    uptr playerSite_ = 0;
    uptr healthSite_ = 0;
    uptr floatSite_ = 0;
    uptr staminaSite_ = 0;
    uptr itemDecSite_ = 0;
    uptr itemCodeSite_ = 0;
    uptr reloadSite_ = 0;
    uptr accuracySite_ = 0;
    uptr recoilSite_ = 0;
    uptr recoilLoadSite_ = 0;
    uptr recoilMul1Site_ = 0;
    uptr recoilMul2Site_ = 0;
    uptr recoilStoreSite_ = 0;
    uptr stealthSite_ = 0;
    uptr hackSite_ = 0;
    uptr statsIdSite_ = 0;
    uptr statsPtrSite_ = 0;
    uptr statsIncSite_ = 0;
    uptr otherStatsSite_ = 0;
    uptr jumpSite_ = 0;
    uptr djumpSite_ = 0;
    uptr daySite_ = 0;
    uptr flySite_ = 0;
    uptr inventorySite_ = 0;
    i32 staminaDisp_ = 0;
    i32 accuracyDisp_ = 0;
    i32 flyDisp_ = 0;
    bool playerHooked_ = false;
    bool healthHooked_ = false;
    bool floatHooked_ = false;
    bool staminaHooked_ = false;
    bool itemCodeHooked_ = false;
    bool itemDecHooked_ = false;
    bool statsIdHooked_ = false;
    bool statsPtrHooked_ = false;
    bool statsIncHooked_ = false;
    bool otherHooked_ = false;
    bool jumpHooked_ = false;
    bool djumpHooked_ = false;
    bool dayHooked_ = false;
    bool flyHooked_ = false;
    bool accuracyHooked_ = false;
    bool inventoryHooked_ = false;
    bool recoilHooked_ = false;
    bool speedHooked_ = false;
    bool speedReady_ = false;
    bool flyOn_ = false;
    bool healthOn_ = false;
    int flyRetryTick_ = 0;
    int healthLogTick_ = 0;
    float healthPeak_ = 0.0f;
    uptr lastHealthPool_ = 0;
    std::vector<uptr> qpcIat_{};
    std::vector<uptr> gtcIat_{};
    std::vector<u64> qpcIatOrig_{};
    std::vector<u64> gtcIatOrig_{};
    std::string lastError_;
};

}  // namespace er
