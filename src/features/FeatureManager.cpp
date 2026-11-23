#include "features/FeatureManager.h"

#include "core/Config.h"
#include "core/CrashDebug.h"
#include "core/Logger.h"
#include "features/FeatureCatalog.h"
#include "hooks/GameHooks.h"
#include "memory/SignatureScanner.h"

#include <utility>

namespace er {
namespace {

enum class ToggleKind {
    Health,
    Stamina,
    Stealth,
    Ohk,
    DmgMul,
    DefMul,
    NoReload,
    Accuracy,
    Recoil,
    Grenade,
    Pls,
    HealCd,
    Items,
    ItemsNoDec,
    XpInf,
    XpMul,
    StreetInf,
    StreetMul,
    SkillInf,
    SkillMul,
    IgnoreCw,
    Ram,
    Breach,
    FreezeDay,
    MoveSpeed,
    SuperJump,
    DoubleJump,
    Fly,
    FlyHeight,
    FlySpeed,
    GameSpeed,
    Components,
    QhComponents,
};

class ToggleFeature final : public IFeature {
public:
    ToggleFeature(FeatureInfo info, ToggleKind kind) : info_(std::move(info)), kind_(kind) {}

    const FeatureInfo& info() const override { return info_; }
    FeatureStatus status() const override { return status_; }
    std::string_view reason() const override { return reason_; }

    bool resolve(SignatureScanner&) override {
        auto& h = GameHooks::instance();
        bool ok = false;
        switch (kind_) {
            case ToggleKind::Health:
                ok = h.playerSiteFound() && h.floatSiteFound();
                break;
            case ToggleKind::Stamina:
            case ToggleKind::Ram:
                ok = h.staminaSiteFound();
                break;
            case ToggleKind::Stealth:
                ok = h.stealthSiteFound();
                break;
            case ToggleKind::Ohk:
            case ToggleKind::DmgMul:
            case ToggleKind::DefMul:
            case ToggleKind::Grenade:
            case ToggleKind::Pls:
            case ToggleKind::HealCd:
                ok = h.floatSiteFound();
                break;
            case ToggleKind::NoReload:
                ok = h.reloadSiteFound();
                break;
            case ToggleKind::Accuracy:
                ok = h.accuracySiteFound();
                break;
            case ToggleKind::Recoil:
                ok = h.recoilSiteFound();
                break;
            case ToggleKind::Items:
            case ToggleKind::ItemsNoDec:
                ok = h.itemCodeSiteFound() || h.inventorySiteFound();
                break;
            case ToggleKind::Components:
            case ToggleKind::QhComponents:
                ok = h.inventorySiteFound();
                break;
            case ToggleKind::XpInf:
            case ToggleKind::XpMul:
            case ToggleKind::StreetInf:
            case ToggleKind::StreetMul:
            case ToggleKind::SkillInf:
            case ToggleKind::SkillMul:
                ok = h.statsIncSiteFound();
                break;
            case ToggleKind::IgnoreCw:
            case ToggleKind::MoveSpeed:
                ok = h.otherStatsSiteFound();
                break;
            case ToggleKind::Breach:
                ok = h.hackSiteFound();
                break;
            case ToggleKind::FreezeDay:
                ok = h.daySiteFound();
                break;
            case ToggleKind::SuperJump:
                ok = h.jumpSiteFound();
                break;
            case ToggleKind::DoubleJump:
                ok = h.djumpSiteFound();
                break;
            case ToggleKind::Fly:
            case ToggleKind::FlyHeight:
            case ToggleKind::FlySpeed:
                ok = h.flySiteFound();
                break;
            case ToggleKind::GameSpeed:
                ok = h.speedReady();
                break;
        }
        if (ok) {
            status_ = FeatureStatus::Ready;
            reason_ = "Готово. Хук/патч при включении.";
            return true;
        }
        status_ = FeatureStatus::Unavailable;
        reason_ = "Сигнатура не найдена на этой сборке 2.5";
        return false;
    }

    void enable() override {
        if (!memory_) {
            fail("Нет процесса");
            return;
        }
        auto& h = GameHooks::instance();
        auto& cfg = Config::instance();
        float fv = info_.defaultFloat;
        if (const auto it = cfg.featureFloat.find(info_.id); it != cfg.featureFloat.end()) {
            fv = it->second;
        }
        bool ok = false;
        switch (kind_) {
            case ToggleKind::Health:
                h.ensurePlayerHook(*memory_);
                ok = h.ensureFloatHook(*memory_);
                if (ok) {
                    h.setHealthFlag(*memory_, true);
                }
                break;
            case ToggleKind::Stamina:
                ok = h.ensureStaminaHook(*memory_);
                if (ok) {
                    h.setStaminaFlag(*memory_, true);
                }
                break;
            case ToggleKind::Ram:
                ok = h.ensureStaminaHook(*memory_);
                if (ok) {
                    h.setRamFlag(*memory_, true);
                }
                break;
            case ToggleKind::Stealth:
                ok = h.applyStealth(*memory_, true);
                break;
            case ToggleKind::Ohk:
                ok = h.ensureFloatHook(*memory_);
                if (ok) {
                    h.setOhk(*memory_, true);
                }
                break;
            case ToggleKind::DmgMul:
                ok = h.ensureFloatHook(*memory_);
                if (ok) {
                    h.setDmgMul(*memory_, fv <= 0 ? 2.0f : fv);
                }
                break;
            case ToggleKind::DefMul:
                ok = h.ensureFloatHook(*memory_);
                if (ok) {
                    h.setDefMul(*memory_, fv <= 0 ? 2.0f : fv);
                }
                break;
            case ToggleKind::Grenade:
                ok = h.ensureFloatHook(*memory_);
                if (ok) {
                    h.setGrenadeNoCd(*memory_, true);
                }
                break;
            case ToggleKind::Pls:
                ok = h.ensureFloatHook(*memory_);
                if (ok) {
                    h.setProjNoCd(*memory_, true);
                }
                break;
            case ToggleKind::HealCd:
                ok = h.ensureFloatHook(*memory_);
                if (ok) {
                    h.setHealNoCd(*memory_, true);
                }
                break;
            case ToggleKind::NoReload:
                ok = h.applyReload(*memory_, true);
                break;
            case ToggleKind::Accuracy:
                ok = h.applyAccuracy(*memory_, true);
                break;
            case ToggleKind::Recoil:
                ok = h.applyRecoil(*memory_, true);
                break;
            case ToggleKind::Items:
                ok = h.ensureItemHooks(*memory_);
                if (ok) {
                    h.setItemsInf(*memory_, true);
                }
                break;
            case ToggleKind::ItemsNoDec:
                ok = h.ensureItemHooks(*memory_);
                if (ok) {
                    h.setItemsNoDec(*memory_, true);
                }
                break;
            case ToggleKind::XpInf:
                ok = h.ensureStatsIncHook(*memory_);
                if (ok) {
                    h.setXpInf(*memory_, true);
                }
                break;
            case ToggleKind::XpMul:
                ok = h.ensureStatsIncHook(*memory_);
                if (ok) {
                    h.setXpMul(*memory_, fv <= 0 ? 2.0f : fv);
                }
                break;
            case ToggleKind::StreetInf:
                ok = h.ensureStatsIncHook(*memory_);
                if (ok) {
                    h.setStreetXpInf(*memory_, true);
                }
                break;
            case ToggleKind::StreetMul:
                ok = h.ensureStatsIncHook(*memory_);
                if (ok) {
                    h.setStreetXpMul(*memory_, fv <= 0 ? 2.0f : fv);
                }
                break;
            case ToggleKind::SkillInf:
                ok = h.ensureStatsIncHook(*memory_);
                if (ok) {
                    h.setSkillProgInf(*memory_, true);
                }
                break;
            case ToggleKind::SkillMul:
                ok = h.ensureStatsIncHook(*memory_);
                if (ok) {
                    h.setSkillProgMul(*memory_, fv <= 0 ? 2.0f : fv);
                }
                break;
            case ToggleKind::IgnoreCw:
                ok = h.ensureOtherStatsHook(*memory_);
                if (ok) {
                    h.setIgnoreCapacity(*memory_, true);
                }
                break;
            case ToggleKind::MoveSpeed:
                ok = h.ensureOtherStatsHook(*memory_);
                if (ok) {
                    h.setMoveSpeed(*memory_, fv <= 0 ? 2.0f : fv);
                }
                break;
            case ToggleKind::Breach:
                ok = h.applyBreachFreeze(*memory_, true);
                break;
            case ToggleKind::FreezeDay:
                ok = h.ensureDayHook(*memory_);
                if (ok) {
                    h.setFreezeDay(*memory_, true);
                }
                break;
            case ToggleKind::SuperJump:
                ok = h.ensureJumpHook(*memory_);
                if (ok) {
                    h.setJumpFlag(*memory_, true);
                }
                break;
            case ToggleKind::DoubleJump:
                ok = h.ensureDjumpHook(*memory_);
                if (ok) {
                    h.setDjumpFlag(*memory_, true);
                }
                break;
            case ToggleKind::Fly:
                ok = h.ensureFlyHook(*memory_);
                if (ok) {
                    h.setFlyMode(*memory_, true);
                }
                break;
            case ToggleKind::FlyHeight:
                ok = h.ensureFlyHook(*memory_);
                if (ok) {
                    h.setFlyHeight(*memory_, fv);
                }
                break;
            case ToggleKind::FlySpeed:
                ok = h.ensureFlyHook(*memory_);
                if (ok) {
                    h.setFlySpeed(*memory_, fv);
                }
                break;
            case ToggleKind::GameSpeed:
                ok = h.ensureSpeedHook(*memory_);
                if (ok) {
                    h.setGameSpeed(*memory_, fv <= 0 ? 2.0f : fv);
                }
                break;
            case ToggleKind::Components:
                ok = h.ensureInventoryHook(*memory_);
                if (ok) {
                    h.setComponents(*memory_, true);
                }
                break;
            case ToggleKind::QhComponents:
                ok = h.ensureInventoryHook(*memory_);
                if (ok) {
                    h.setQhComponents(*memory_, true);
                }
                break;
        }
        if (!ok) {
            fail(h.lastError().empty() ? "Не удалось включить" : h.lastError().c_str());
            Logger::instance().dbg(std::string("probe ") + info_.id + " FAIL " + reason_);
            return;
        }
        enabled_ = true;
        status_ = FeatureStatus::Enabled;
        reason_ = std::string("Включено | ") + h.probeForFeature(*memory_, info_.id);
        Logger::instance().dbg(std::string("probe ") + info_.id + " " + reason_);
    }

    void disable() override {
        if (memory_) {
            auto& h = GameHooks::instance();
            switch (kind_) {
                case ToggleKind::Health:
                    h.setHealthFlag(*memory_, false);
                    break;
                case ToggleKind::Stamina:
                    h.setStaminaFlag(*memory_, false);
                    break;
                case ToggleKind::Ram:
                    h.setRamFlag(*memory_, false);
                    break;
                case ToggleKind::Stealth:
                    h.applyStealth(*memory_, false);
                    break;
                case ToggleKind::Ohk:
                    h.setOhk(*memory_, false);
                    break;
                case ToggleKind::DmgMul:
                    h.setDmgMul(*memory_, 0.0f);
                    break;
                case ToggleKind::DefMul:
                    h.setDefMul(*memory_, 0.0f);
                    break;
                case ToggleKind::Grenade:
                    h.setGrenadeNoCd(*memory_, false);
                    break;
                case ToggleKind::Pls:
                    h.setProjNoCd(*memory_, false);
                    break;
                case ToggleKind::HealCd:
                    h.setHealNoCd(*memory_, false);
                    break;
                case ToggleKind::NoReload:
                    h.applyReload(*memory_, false);
                    break;
                case ToggleKind::Accuracy:
                    h.applyAccuracy(*memory_, false);
                    break;
                case ToggleKind::Recoil:
                    h.applyRecoil(*memory_, false);
                    break;
                case ToggleKind::Items:
                    h.setItemsInf(*memory_, false);
                    break;
                case ToggleKind::ItemsNoDec:
                    h.setItemsNoDec(*memory_, false);
                    break;
                case ToggleKind::XpInf:
                    h.setXpInf(*memory_, false);
                    break;
                case ToggleKind::XpMul:
                    h.setXpMul(*memory_, 0.0f);
                    break;
                case ToggleKind::StreetInf:
                    h.setStreetXpInf(*memory_, false);
                    break;
                case ToggleKind::StreetMul:
                    h.setStreetXpMul(*memory_, 0.0f);
                    break;
                case ToggleKind::SkillInf:
                    h.setSkillProgInf(*memory_, false);
                    break;
                case ToggleKind::SkillMul:
                    h.setSkillProgMul(*memory_, 0.0f);
                    break;
                case ToggleKind::IgnoreCw:
                    h.setIgnoreCapacity(*memory_, false);
                    break;
                case ToggleKind::MoveSpeed:
                    h.setMoveSpeed(*memory_, 0.0f);
                    break;
                case ToggleKind::Breach:
                    h.applyBreachFreeze(*memory_, false);
                    break;
                case ToggleKind::FreezeDay:
                    h.setFreezeDay(*memory_, false);
                    break;
                case ToggleKind::SuperJump:
                    h.setJumpFlag(*memory_, false);
                    break;
                case ToggleKind::DoubleJump:
                    h.setDjumpFlag(*memory_, false);
                    break;
                case ToggleKind::Fly:
                    h.setFlyMode(*memory_, false);
                    break;
                case ToggleKind::FlyHeight:
                    h.setFlyHeight(*memory_, 0.0f);
                    break;
                case ToggleKind::FlySpeed:
                    h.setFlySpeed(*memory_, 1.0f);
                    break;
                case ToggleKind::GameSpeed:
                    h.setGameSpeed(*memory_, 1.0f);
                    break;
                case ToggleKind::Components:
                    h.setComponents(*memory_, false);
                    break;
                case ToggleKind::QhComponents:
                    h.setQhComponents(*memory_, false);
                    break;
                default:
                    break;
            }
        }
        enabled_ = false;
        if (status_ != FeatureStatus::Unavailable) {
            status_ = FeatureStatus::Ready;
            reason_ = "Готово";
        }
    }

    void tick(float) override {
        if (!enabled_ || !memory_) {
            return;
        }
        if (kind_ != ToggleKind::DmgMul && kind_ != ToggleKind::DefMul && kind_ != ToggleKind::XpMul &&
            kind_ != ToggleKind::StreetMul && kind_ != ToggleKind::SkillMul &&
            kind_ != ToggleKind::MoveSpeed && kind_ != ToggleKind::FlyHeight &&
            kind_ != ToggleKind::FlySpeed && kind_ != ToggleKind::GameSpeed) {
            return;
        }
        auto& cfg = Config::instance();
        float v = info_.defaultFloat;
        if (const auto it = cfg.featureFloat.find(info_.id); it != cfg.featureFloat.end()) {
            v = it->second;
        }
        auto& h = GameHooks::instance();
        if (kind_ == ToggleKind::DmgMul) {
            h.setDmgMul(*memory_, v);
        } else if (kind_ == ToggleKind::DefMul) {
            h.setDefMul(*memory_, v);
        } else if (kind_ == ToggleKind::XpMul) {
            h.setXpMul(*memory_, v);
        } else if (kind_ == ToggleKind::StreetMul) {
            h.setStreetXpMul(*memory_, v);
        } else if (kind_ == ToggleKind::SkillMul) {
            h.setSkillProgMul(*memory_, v);
        } else if (kind_ == ToggleKind::MoveSpeed) {
            h.setMoveSpeed(*memory_, v);
        } else if (kind_ == ToggleKind::FlyHeight) {
            h.setFlyHeight(*memory_, v);
        } else if (kind_ == ToggleKind::FlySpeed) {
            h.setFlySpeed(*memory_, v);
        } else if (kind_ == ToggleKind::GameSpeed) {
            h.setGameSpeed(*memory_, v <= 0 ? 1.0f : v);
        }
    }

    void bindMemory(ProcessMemory* m) override { memory_ = m; }

private:
    void fail(const char* msg) {
        status_ = FeatureStatus::Error;
        reason_ = msg;
        enabled_ = false;
    }

    FeatureInfo info_;
    ToggleKind kind_;
    FeatureStatus status_ = FeatureStatus::Unavailable;
    std::string reason_ = "Нет хука";
    bool enabled_ = false;
    ProcessMemory* memory_ = nullptr;
};

enum class ApplyKind {
    DayPlus,
    Money,
    Weight,
    Attr,
    Perk,
    Relic,
    Level,
    StreetLv,
    Skill0,
    Skill1,
    Skill2,
    Skill3,
    Skill4,
};

class ApplyFeature final : public IFeature {
public:
    ApplyFeature(FeatureInfo info, ApplyKind kind) : info_(std::move(info)), kind_(kind) {}

    const FeatureInfo& info() const override { return info_; }
    FeatureStatus status() const override { return status_; }
    std::string_view reason() const override { return reason_; }

    bool resolve(SignatureScanner&) override {
        auto& h = GameHooks::instance();
        bool ok = false;
        switch (kind_) {
            case ApplyKind::DayPlus:
                ok = h.daySiteFound();
                break;
            case ApplyKind::Weight:
                ok = h.otherStatsSiteFound();
                break;
            case ApplyKind::Money:
                ok = h.inventorySiteFound();
                break;
            default:
                ok = h.statsPtrSiteFound();
                break;
        }
        status_ = ok ? FeatureStatus::Ready : FeatureStatus::Unavailable;
        reason_ = ok ? "Готово. Жми ПРИМЕНИТЬ. Пишется в сейв — сделай бэкап."
                     : "Сигнатура не найдена";
        return ok;
    }

    void enable() override { applyNow(); }
    void disable() override {
        if (status_ != FeatureStatus::Unavailable) {
            status_ = FeatureStatus::Ready;
        }
    }
    void tick(float) override {}
    void bindMemory(ProcessMemory* m) override { memory_ = m; }
    void setIntValue(i32 v) override { value_ = v; }

    void applyNow() override {
        if (!memory_) {
            return;
        }
        auto& h = GameHooks::instance();
        auto& cfg = Config::instance();
        i32 iv = info_.defaultInt;
        if (const auto it = cfg.featureInt.find(info_.id); it != cfg.featureInt.end()) {
            iv = it->second;
        }
        if (value_ != 0) {
            iv = value_;
        }
        bool ok = false;
        switch (kind_) {
            case ApplyKind::DayPlus:
                ok = h.ensureDayHook(*memory_);
                if (ok) {
                    h.pulseDayAdd(*memory_);
                }
                break;
            case ApplyKind::Weight:
                ok = h.ensureOtherStatsHook(*memory_);
                if (ok) {
                    h.setMaxWeight(*memory_, iv);
                }
                break;
            case ApplyKind::Attr:
                ok = h.ensureStatsPtrHook(*memory_);
                if (ok) {
                    h.setAttributePoints(*memory_, iv);
                }
                break;
            case ApplyKind::Perk:
                ok = h.ensureStatsPtrHook(*memory_);
                if (ok) {
                    h.setPerkPoints(*memory_, iv);
                }
                break;
            case ApplyKind::Relic:
                ok = h.ensureStatsPtrHook(*memory_);
                if (ok) {
                    h.setRelicPoints(*memory_, iv);
                }
                break;
            case ApplyKind::Level:
                ok = h.ensureStatsPtrHook(*memory_);
                if (ok) {
                    h.setPlayerLevel(*memory_, iv);
                }
                break;
            case ApplyKind::StreetLv:
                ok = h.ensureStatsPtrHook(*memory_);
                if (ok) {
                    h.setStreetCredLevel(*memory_, iv);
                }
                break;
            case ApplyKind::Skill0:
            case ApplyKind::Skill1:
            case ApplyKind::Skill2:
            case ApplyKind::Skill3:
            case ApplyKind::Skill4:
                ok = h.ensureStatsPtrHook(*memory_);
                if (ok) {
                    h.setSkillLevel(*memory_, static_cast<int>(kind_) - static_cast<int>(ApplyKind::Skill0),
                                    iv);
                }
                break;
            case ApplyKind::Money:
                ok = h.ensureInventoryHook(*memory_);
                if (ok) {
                    h.setMoney(*memory_, iv);
                }
                if (ok) {
                    status_ = FeatureStatus::Ready;
                    reason_ = std::string("Применено. Открой инвентарь. | ") +
                              h.probeForFeature(*memory_, info_.id);
                    Logger::instance().dbg(std::string("probe ") + info_.id + " " + reason_);
                    return;
                }
                break;
        }
        if (!ok) {
            status_ = FeatureStatus::Error;
            reason_ = "Не удалось применить";
            Logger::instance().dbg(std::string("probe ") + info_.id + " FAIL " + reason_);
            return;
        }
        status_ = FeatureStatus::Ready;
        reason_ = std::string("Применено | ") + h.probeForFeature(*memory_, info_.id);
        Logger::instance().dbg(std::string("probe ") + info_.id + " " + reason_);
    }

private:
    FeatureInfo info_;
    ApplyKind kind_;
    FeatureStatus status_ = FeatureStatus::Unavailable;
    std::string reason_;
    ProcessMemory* memory_ = nullptr;
    i32 value_ = 0;
};

class StubFeature final : public IFeature {
public:
    explicit StubFeature(FeatureInfo info) : info_(std::move(info)) {}

    const FeatureInfo& info() const override { return info_; }
    FeatureStatus status() const override { return status_; }
    std::string_view reason() const override { return reason_; }

    bool resolve(SignatureScanner&) override {
        if (status_ != FeatureStatus::Enabled) {
            status_ = FeatureStatus::Ready;
            reason_ = "Готово.";
        }
        return true;
    }
    void enable() override {
        status_ = FeatureStatus::Enabled;
        reason_ = "Включено. Скоро.";
    }
    void disable() override {
        status_ = FeatureStatus::Ready;
        reason_ = "Готово.";
    }
    void tick(float) override {}

private:
    FeatureInfo info_;
    FeatureStatus status_ = FeatureStatus::Ready;
    std::string reason_ = "Готово.";
};

std::unique_ptr<IFeature> makeFeature(const FeatureInfo& info) {
    if (info.placeholder) {
        return std::make_unique<StubFeature>(info);
    }
    static const std::pair<const char*, ToggleKind> toggles[] = {
        {"infinite_health", ToggleKind::Health},
        {"infinite_stamina", ToggleKind::Stamina},
        {"stealth_mode", ToggleKind::Stealth},
        {"one_hit_kill", ToggleKind::Ohk},
        {"damage_multiplier", ToggleKind::DmgMul},
        {"defense_multiplier", ToggleKind::DefMul},
        {"no_reload", ToggleKind::NoReload},
        {"super_accuracy", ToggleKind::Accuracy},
        {"no_recoil", ToggleKind::Recoil},
        {"grenade_no_cd", ToggleKind::Grenade},
        {"pls_no_cd", ToggleKind::Pls},
        {"heal_no_cd", ToggleKind::HealCd},
        {"infinite_items", ToggleKind::Items},
        {"items_no_decrease", ToggleKind::ItemsNoDec},
        {"infinite_xp", ToggleKind::XpInf},
        {"xp_multiplier", ToggleKind::XpMul},
        {"infinite_streetcred", ToggleKind::StreetInf},
        {"streetcred_multiplier", ToggleKind::StreetMul},
        {"max_skill_xp", ToggleKind::SkillInf},
        {"skill_xp_multiplier", ToggleKind::SkillMul},
        {"ignore_cyberware", ToggleKind::IgnoreCw},
        {"infinite_components", ToggleKind::Components},
        {"infinite_qh_components", ToggleKind::QhComponents},
        {"infinite_ram", ToggleKind::Ram},
        {"freeze_breach", ToggleKind::Breach},
        {"freeze_daytime", ToggleKind::FreezeDay},
        {"movement_speed", ToggleKind::MoveSpeed},
        {"super_jump", ToggleKind::SuperJump},
        {"infinite_double_jump", ToggleKind::DoubleJump},
        {"fly_mode", ToggleKind::Fly},
        {"fly_height", ToggleKind::FlyHeight},
        {"fly_speed", ToggleKind::FlySpeed},
        {"game_speed", ToggleKind::GameSpeed},
    };
    for (const auto& t : toggles) {
        if (info.id == t.first) {
            return std::make_unique<ToggleFeature>(info, t.second);
        }
    }
    static const std::pair<const char*, ApplyKind> applies[] = {
        {"daytime_plus_1h", ApplyKind::DayPlus},
        {"edit_money", ApplyKind::Money},
        {"edit_carry_weight", ApplyKind::Weight},
        {"edit_attribute_points", ApplyKind::Attr},
        {"edit_perk_points", ApplyKind::Perk},
        {"edit_relic_points", ApplyKind::Relic},
        {"edit_player_level", ApplyKind::Level},
        {"edit_streetcred_level", ApplyKind::StreetLv},
        {"edit_headhunter", ApplyKind::Skill0},
        {"edit_netrunner", ApplyKind::Skill1},
        {"edit_shinobi", ApplyKind::Skill2},
        {"edit_solo", ApplyKind::Skill3},
        {"edit_engineer", ApplyKind::Skill4},
    };
    for (const auto& a : applies) {
        if (info.id == a.first) {
            return std::make_unique<ApplyFeature>(info, a.second);
        }
    }
    return std::make_unique<StubFeature>(info);
}

}  // namespace

FeatureManager::FeatureManager() {
    for (const auto& info : featureCatalog()) {
        auto f = makeFeature(info);
        byId_[f->info().id] = f.get();
        wanted_[f->info().id] = false;
        features_.push_back(std::move(f));
    }
}

void FeatureManager::bindMemory(ProcessMemory* memory) {
    memory_ = memory;
    for (auto& f : features_) {
        f->bindMemory(memory);
    }
}

void FeatureManager::resolveAll(SignatureScanner& scanner) {
    if (memory_) {
        GameHooks::instance().resolve(*memory_, scanner);
        bindMemory(memory_);
    }
    for (auto& f : features_) {
        f->resolve(scanner);
    }
    Logger::instance().dbg(hooksDebugDump());
    Logger::instance().dbg(featureDebugDump(*this));
}

void FeatureManager::shutdown() {
    disableAll();
    if (memory_) {
        GameHooks::instance().shutdown(*memory_);
    }
}

void FeatureManager::requestEnable(std::string_view id, bool on) {
    wanted_[std::string(id)] = on;
    syncStates();
}

void FeatureManager::requestApply(std::string_view id) {
    IFeature* f = find(id);
    if (!f || !memory_) {
        return;
    }
    auto& cfg = Config::instance();
    if (const auto it = cfg.featureInt.find(std::string(id)); it != cfg.featureInt.end()) {
        f->setIntValue(it->second);
    }
    Logger::instance().dbg(std::string("apply ") + std::string(id));
    f->applyNow();
    Logger::instance().dbg(std::string("apply ") + std::string(id) + " -> " + std::string(f->reason()));
}

IFeature* FeatureManager::find(std::string_view id) {
    const auto it = byId_.find(std::string(id));
    return it == byId_.end() ? nullptr : it->second;
}

void FeatureManager::syncStates() {
    std::lock_guard lock(mu_);
    for (auto& f : features_) {
        const auto& id = f->info().id;
        const bool want = wanted_[id].load();
        const bool isOn = f->status() == FeatureStatus::Enabled;
        if (f->status() == FeatureStatus::Unavailable) {
            continue;
        }
        if (want && !isOn && f->status() != FeatureStatus::Error) {
            Logger::instance().dbg(std::string("enable ") + id);
            f->enable();
            Logger::instance().dbg(std::string("enable ") + id + " -> " + std::string(f->reason()));
        } else if (!want && (isOn || f->status() == FeatureStatus::Error)) {
            Logger::instance().dbg(std::string("disable ") + id);
            f->disable();
            Logger::instance().dbg(std::string("disable ") + id + " -> " + std::string(f->reason()));
        }
    }
}

void FeatureManager::disableAll() {
    for (auto& f : features_) {
        if (f->status() == FeatureStatus::Enabled) {
            f->disable();
        }
        wanted_[f->info().id] = false;
    }
}

void FeatureManager::tick(float dt) {
    syncStates();
    if (memory_) {
        auto& h = GameHooks::instance();
        const bool flyWas = h.flySiteFound();
        const bool speedWas = h.speedReady();
        h.tick(*memory_);
        if ((!flyWas && h.flySiteFound()) || (!speedWas && h.speedReady())) {
            SignatureScanner scan(*memory_);
            for (auto& f : features_) {
                if (f->status() == FeatureStatus::Unavailable) {
                    f->resolve(scan);
                }
            }
        }
    }
    for (auto& f : features_) {
        if (f->status() == FeatureStatus::Enabled) {
            f->tick(dt);
        }
    }
}

}  // namespace er
