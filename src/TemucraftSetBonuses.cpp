/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

// Set bonuses with character: extra, server-side bonuses for item sets (or single items), on top of their own.
// Rows in `mod_temucraft_set_bonus` (world database), each needing a number of pieces of a set worn (or one item):
//
//   1 CREATURE_DAMAGE  more damage against one creature type (value1 = type, value2 = %)
//   2 ZONE_SPEED       faster in some zones (zones, value2 = %; value1 = 1 mounted speed, 0 run speed)
//   3 ON_HIT           chance to cast a spell on whoever you hit (value1: 0 any hit, 1 melee/ranged, 2 spells)
//   4 WHEN_HIT         chance to cast a spell when hit (value1 as above; value2 = 1 on yourself instead of the attacker)
//   5 LOW_HEALTH       cast a spell on yourself when your health drops below value2 %
//   6 STACKS           hits dealt or taken may add a stack (value1 = most stacks) of a buff (spell), value2 per stack
//   7 TRANSFORM        look like a creature (value1 = display id) while in a form (value2: -1 any, 0 none, else the
//                      shapeshift form), optionally only at `stacks` stacks of the set's STACKS row
//   8 AURA             a spell aura while the set is worn (zones: only there)
//   9 LOW_MANA         cast a spell on yourself when your mana is below value2 % (checked every second; give it a cooldown)
//  10 ON_HEAL          chance to cast a spell on whoever your heals land on
//
// Spells cast for a bonus are triggered (free, instant). bp_per_level sets the spell's first value to that much per
// level of the wearer (negative: that value, flat); duration (ms) overrides how long it lasts; cooldown (ms) is per
// player and bonus. For the spell-casting kinds (3, 4, 5, 9, 10) zones are where the effect is doubled.

#include "TemucraftLocked.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    enum Kind : uint8
    {
        CREATURE_DAMAGE = 1, ZONE_SPEED, ON_HIT, WHEN_HIT, LOW_HEALTH, STACKS, TRANSFORM, AURA, LOW_MANA, ON_HEAL
    };

    struct Bonus
    {
        uint32 id = 0;
        uint32 itemset = 0;
        uint32 item = 0;
        uint8 pieces = 0;
        uint8 kind = 0;
        uint8 race = 0;
        uint8 cls = 0;
        uint32 spell = 0;
        float chance = 0.0f;
        uint32 cooldown = 0;
        int32 duration = 0;
        float bpPerLevel = 0.0f;
        int32 value1 = 0;
        float value2 = 0.0f;
        uint32 stacks = 0;
        std::unordered_set<uint32> zones;
    };

    bool enabled = true;
    int32 zoneMultiplier = 2;
    std::vector<Bonus> bonuses;
    std::unordered_map<uint32, std::vector<Bonus const*>> bySet;    // itemset -> bonuses
    std::unordered_map<uint32, std::vector<Bonus const*>> byItem;   // item -> bonuses
    std::unordered_set<uint32> transformDisplays;
    std::unordered_set<uint32> bonusSpells;    // spells the bonuses cast: their hits don't proc bonuses again

    struct PlayerState
    {
        std::unordered_map<uint32, uint32> ready;     // bonus -> game ms when it may fire again
        std::unordered_map<uint32, uint32> stacks;    // STACKS bonus -> stacks
        std::unordered_map<uint32, uint32> stackEnd;  // STACKS bonus -> game ms when the stacks fall off
        std::unordered_set<uint32> auras;             // spells applied by ZONE_SPEED / AURA bonuses
        uint32 transformed = 0;                       // display id put on by a TRANSFORM bonus
        bool setting = false;                         // changing the display ourselves
        uint32 timer = 0;
    };
    Temucraft::LockedMap<uint32, PlayerState> states;

    PlayerState& State(Player* p) { return states[p->GetGUID().GetCounter()]; }
    uint32 Now() { return uint32(GameTime::GetGameTimeMS().count()); }

    void Load()
    {
        bonuses.clear();
        bySet.clear();
        byItem.clear();
        transformDisplays.clear();
        bonusSpells.clear();
        QueryResult r = WorldDatabase.Query("SELECT `id`, `itemset`, `item`, `pieces`, `kind`, `race`, `class`, `spell`, `chance`, `cooldown`, "
            "`duration`, `bp_per_level`, `value1`, `value2`, `stacks`, `zones` FROM `mod_temucraft_set_bonus` WHERE `enabled` = 1 ORDER BY `race` DESC, `class` DESC, `id`");
        if (!r)
            return;
        do
        {
            Field* f = r->Fetch();
            Bonus b;
            b.id = f[0].Get<uint32>();
            b.itemset = f[1].Get<uint32>();
            b.item = f[2].Get<uint32>();
            b.pieces = f[3].Get<uint8>();
            b.kind = f[4].Get<uint8>();
            b.race = f[5].Get<uint8>();
            b.cls = f[6].Get<uint8>();
            b.spell = f[7].Get<uint32>();
            b.chance = f[8].Get<float>();
            b.cooldown = f[9].Get<uint32>();
            b.duration = f[10].Get<int32>();
            b.bpPerLevel = f[11].Get<float>();
            b.value1 = f[12].Get<int32>();
            b.value2 = f[13].Get<float>();
            b.stacks = f[14].Get<uint32>();
            std::string zones = f[15].Get<std::string>();
            std::replace(zones.begin(), zones.end(), ',', ' ');
            std::istringstream in(zones);
            uint32 z;
            while (in >> z)
                b.zones.insert(z);
            if (b.spell && !sSpellMgr->GetSpellInfo(b.spell))
            {
                LOG_ERROR("sql.sql", "mod_temucraft_set_bonus {}: spell {} doesn't exist, skipped.", b.id, b.spell);
                continue;
            }
            if (!b.itemset && !b.item)
                continue;
            bonuses.push_back(b);
        } while (r->NextRow());
        for (Bonus const& b : bonuses)
        {
            if (b.item)
                byItem[b.item].push_back(&b);
            else
                bySet[b.itemset].push_back(&b);
            if (b.kind == TRANSFORM)
                transformDisplays.insert(uint32(b.value1));
            if (b.spell && (b.kind == ON_HIT || b.kind == WHEN_HIT || b.kind == LOW_HEALTH || b.kind == LOW_MANA || b.kind == ON_HEAL))
                bonusSpells.insert(b.spell);
        }
        LOG_INFO("server.loading", ">> mod-temucraft-set-bonuses: {} bonuses", bonuses.size());
    }

    bool Fits(Player* p, Bonus const& b)
    {
        return (!b.race || b.race == p->getRace()) && (!b.cls || b.cls == p->getClass());
    }

    // the bonuses of a kind the player has right now
    template <class F>
    void ForEach(Player* p, uint8 kind, F&& f)
    {
        if (!enabled || !p)
            return;
        for (ItemSetEffect const* eff : p->ItemSetEff)
        {
            if (!eff)
                continue;
            auto itr = bySet.find(eff->setid);
            if (itr == bySet.end())
                continue;
            for (Bonus const* b : itr->second)
                if (b->kind == kind && eff->item_count >= b->pieces && Fits(p, *b))
                    f(*b);
        }
        if (!byItem.empty())
            for (auto const& [item, list] : byItem)
                if (p->HasItemOrGemWithIdEquipped(item, 1))
                    for (Bonus const* b : list)
                        if (b->kind == kind && Fits(p, *b))
                            f(*b);
    }

    bool Ready(Player* p, Bonus const& b)
    {
        if (!b.cooldown)
            return true;
        uint32& ready = State(p).ready[b.id];
        uint32 const now = Now();
        if (ready > now)
            return false;
        ready = now + b.cooldown;
        return true;
    }

    // cast the bonus spell a moment later (never in the middle of a damage calculation)
    void Fire(Player* p, Bonus const& b, Unit* target)
    {
        if (!b.spell || !target)
            return;
        ObjectGuid const targetGuid = target->GetGUID();
        uint32 const spell = b.spell;
        int32 bp = b.bpPerLevel > 0.0f ? std::max(1, int32(b.bpPerLevel * p->GetLevel())) : b.bpPerLevel < 0.0f ? int32(-b.bpPerLevel) : 0;
        // doubled in the bonus' zones (Temucraft.SetBonuses.ZoneMultiplier)
        if (!b.zones.empty() && b.zones.count(p->GetZoneId()))
        {
            if (!bp)
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spell))
                    bp = info->Effects[EFFECT_0].CalcValue(p);
            bp *= zoneMultiplier;
        }
        int32 const duration = b.duration;
        p->m_Events.AddEventAtOffset([p, targetGuid, spell, bp, duration]()
        {
            Unit* t = targetGuid == p->GetGUID() ? p : ObjectAccessor::GetUnit(*p, targetGuid);
            if (!t || !t->IsAlive() || !p->IsAlive())
                return;
            if (bp)
                p->CastCustomSpell(spell, SPELLVALUE_BASE_POINT0, bp, t, true);
            else
                p->CastSpell(t, spell, true);
            if (duration > 0)
                if (Aura* aura = t->GetAura(spell, p->GetGUID()))
                {
                    aura->SetMaxDuration(duration);
                    aura->SetDuration(duration);
                }
        }, 1ms);
    }

    uint32 CurrentStacks(Player* p, uint32 itemset)
    {
        PlayerState& s = State(p);
        uint32 best = 0;
        for (Bonus const& b : bonuses)
            if (b.kind == STACKS && b.itemset == itemset)
                if (auto itr = s.stacks.find(b.id); itr != s.stacks.end() && s.stackEnd[b.id] > Now())
                    best = std::max(best, itr->second);
        return best;
    }

    uint32 WantedDisplay(Player* p)
    {
        uint32 display = 0;
        int32 const form = int32(p->GetShapeshiftForm());
        ForEach(p, TRANSFORM, [&](Bonus const& b)
        {
            if (display)
                return;
            if (b.value2 >= 0.0f && int32(b.value2) != form)
                return;
            if (b.stacks && CurrentStacks(p, b.itemset) < b.stacks)
                return;
            display = uint32(b.value1);
        });
        return display;
    }

    void RefreshLook(Player* p)
    {
        if (!p->IsInWorld())
            return;
        PlayerState& s = State(p);
        uint32 const want = WantedDisplay(p);
        if (want)
        {
            if (p->GetDisplayId() != want)
            {
                s.setting = true;
                p->SetDisplayId(want);
                s.setting = false;
            }
            s.transformed = want;
        }
        else if (s.transformed)
        {
            s.transformed = 0;
            s.setting = true;
            p->RestoreDisplayId();
            s.setting = false;
        }
    }

    // ZONE_SPEED and AURA bonuses: the auras that should be on, put on or taken off
    void RefreshAuras(Player* p)
    {
        if (!p->IsInWorld())
            return;
        PlayerState& s = State(p);
        std::unordered_map<uint32, float> want;   // spell -> amount (0 = the spell's own)
        uint32 const zone = p->GetZoneId();
        ForEach(p, ZONE_SPEED, [&](Bonus const& b)
        {
            if (b.zones.count(zone))
            {
                uint32 const spell = b.spell ? b.spell : (b.value1 ? 27881u : 13889u);
                want[spell] += b.value2;   // pieces with the same zone bonus add up
            }
        });
        ForEach(p, AURA, [&](Bonus const& b)
        {
            if (b.spell && (b.zones.empty() || b.zones.count(zone)))
                want.emplace(b.spell, b.value2 > 0.0f ? b.value2 : 0.0f);
        });

        for (auto itr = s.auras.begin(); itr != s.auras.end();)
        {
            if (!want.count(*itr))
            {
                p->RemoveAurasDueToSpell(*itr, p->GetGUID());
                itr = s.auras.erase(itr);
            }
            else
                ++itr;
        }
        for (auto const& [spell, amountF] : want)
        {
            int32 const amount = int32(std::lround(amountF));
            Aura* aura = p->GetAura(spell, p->GetGUID());
            if (!aura)
                aura = p->AddAura(spell, p);
            if (!aura)
                continue;
            s.auras.insert(spell);
            if (amount)
                if (AuraEffect* eff = aura->GetEffect(EFFECT_0))
                    if (eff->GetAmount() != amount)
                        eff->ChangeAmount(amount);
        }
    }

    void Refresh(Player* p)
    {
        RefreshAuras(p);
        RefreshLook(p);
    }

    void AddStack(Player* p)
    {
        bool changed = false;
        ForEach(p, STACKS, [&](Bonus const& b)
        {
            if (!roll_chance_f(b.chance) || !Ready(p, b))
                return;
            PlayerState& s = State(p);
            uint32 const now = Now();
            uint32& count = s.stacks[b.id];
            if (s.stackEnd[b.id] <= now)
                count = 0;
            uint32 const most = std::max<int32>(1, b.value1);
            if (count < most)
            {
                ++count;
                changed = true;
            }
            int32 const duration = b.duration > 0 ? b.duration : 30 * IN_MILLISECONDS;
            s.stackEnd[b.id] = now + duration;
            if (!b.spell)
                return;
            uint32 const spell = b.spell;
            int32 const amount = int32(count * b.value2);
            p->m_Events.AddEventAtOffset([p, spell, amount, duration]()
            {
                Aura* aura = p->GetAura(spell, p->GetGUID());
                if (!aura)
                    aura = p->AddAura(spell, p);
                if (!aura)
                    return;
                aura->SetMaxDuration(duration);
                aura->SetDuration(duration);
                if (amount)
                    if (AuraEffect* eff = aura->GetEffect(EFFECT_0))
                        eff->ChangeAmount(amount);
            }, 1ms);
        });
        if (changed)
            p->m_Events.AddEventAtOffset([p]() { RefreshLook(p); }, 2ms);
    }

    float CreatureBonus(Player* p, Unit* target)
    {
        if (!target)
            return 0.0f;
        uint32 const type = target->GetCreatureType();
        float pct = 0.0f;
        ForEach(p, CREATURE_DAMAGE, [&](Bonus const& b) { if (uint32(b.value1) == type) pct += b.value2; });
        return pct;
    }

    // damage dealt by a player (or their pet): creature bonus and on-hit procs; damage taken: when-hit procs
    void Dealt(Unit* attacker, Unit* target, bool melee, float& pct)
    {
        Player* p = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!p || !target || target == p)
            return;
        pct = CreatureBonus(p, target);
        if (attacker != p)
            return;   // procs only from the player's own hits
        ForEach(p, ON_HIT, [&](Bonus const& b)
        {
            if ((b.value1 == 1 && !melee) || (b.value1 == 2 && melee))
                return;
            if (roll_chance_f(b.chance) && Ready(p, b))
                Fire(p, b, target);
        });
        if (melee)
            AddStack(p);
    }

    void Taken(Unit* target, Unit* attacker, bool melee)
    {
        Player* p = target ? target->ToPlayer() : nullptr;
        if (!p || !attacker || attacker == p)
            return;
        ForEach(p, WHEN_HIT, [&](Bonus const& b)
        {
            if ((b.value1 == 1 && !melee) || (b.value1 == 2 && melee))
                return;
            if (roll_chance_f(b.chance) && Ready(p, b))
                Fire(p, b, b.value2 >= 1.0f ? static_cast<Unit*>(p) : attacker);
        });
        if (melee)
            AddStack(p);
    }
}

class TemucraftSetBonusesWorld : public WorldScript
{
public:
    TemucraftSetBonusesWorld() : WorldScript("TemucraftSetBonusesWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        enabled = sConfigMgr->GetOption<bool>("Temucraft.SetBonuses.Enable", true);
        zoneMultiplier = sConfigMgr->GetOption<int32>("Temucraft.SetBonuses.ZoneMultiplier", 2);
        if (reload)
            Load();
    }

    void OnStartup() override { Load(); }
};

class TemucraftSetBonusesUnit : public UnitScript
{
public:
    TemucraftSetBonusesUnit() : UnitScript("TemucraftSetBonusesUnit", true,
        { UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
          UNITHOOK_ON_DAMAGE, UNITHOOK_ON_DISPLAYID_CHANGE, UNITHOOK_MODIFY_HEAL_RECEIVED }) { }

    // ON_HEAL: heals that land
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        Player* p = healer ? healer->ToPlayer() : nullptr;
        if (!enabled || !p || !heal || !target || (spellInfo && bonusSpells.count(spellInfo->Id)))
            return;
        ForEach(p, ON_HEAL, [&](Bonus const& b)
        {
            if (roll_chance_f(b.chance) && Ready(p, b))
                Fire(p, b, target);
        });
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (!enabled || !damage)
            return;
        float pct = 0.0f;
        Dealt(attacker, target, true, pct);
        if (pct)
            damage = uint32(damage * (1.0f + pct / 100.0f));
        Taken(target, attacker, true);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!enabled || damage <= 0 || (spellInfo && bonusSpells.count(spellInfo->Id)))
            return;
        float pct = 0.0f;
        Dealt(attacker, target, false, pct);
        if (pct)
            damage = int32(damage * (1.0f + pct / 100.0f));
        Taken(target, attacker, false);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (!enabled || !damage || !attacker)
            return;
        if (Player* p = attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            if (float pct = CreatureBonus(p, target))
                damage = uint32(damage * (1.0f + pct / 100.0f));
    }

    // LOW_HEALTH: falling below the threshold
    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        Player* p = victim ? victim->ToPlayer() : nullptr;
        if (!enabled || !p || !damage || !p->IsAlive())
            return;
        uint64 const health = p->GetHealth();
        uint64 const after = health > damage ? health - damage : 0;
        ForEach(p, LOW_HEALTH, [&](Bonus const& b)
        {
            uint64 const line = uint64(p->GetMaxHealth() * b.value2 / 100.0f);
            if (health >= line && after < line && after > 0 && Ready(p, b))
                Fire(p, b, p);
        });
    }

    // keep a TRANSFORM look through form changes and anything else that resets the model
    void OnDisplayIdChange(Unit* unit, uint32 displayId) override
    {
        Player* p = unit ? unit->ToPlayer() : nullptr;
        if (!enabled || !p || transformDisplays.empty())
            return;
        PlayerState& s = State(p);
        if (s.setting)
            return;
        uint32 const want = WantedDisplay(p);
        if (want && displayId != want)
        {
            s.setting = true;
            p->SetDisplayId(want);
            s.setting = false;
            s.transformed = want;
        }
        else if (!want)
            s.transformed = 0;
    }
};

class TemucraftSetBonusesPlayer : public PlayerScript
{
public:
    TemucraftSetBonusesPlayer() : PlayerScript("TemucraftSetBonusesPlayer",
        { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_EQUIP, PLAYERHOOK_ON_UNEQUIP_ITEM, PLAYERHOOK_ON_UPDATE_ZONE, PLAYERHOOK_ON_UPDATE }) { }

    static void Later(Player* p) { p->m_Events.AddEventAtOffset([p]() { Refresh(p); }, 200ms); }

    void OnPlayerLogin(Player* p) override { if (enabled) Later(p); }
    void OnPlayerLogout(Player* p) override { states.erase(p->GetGUID().GetCounter()); }
    void OnPlayerEquip(Player* p, Item* /*it*/, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override { if (enabled) Later(p); }
    void OnPlayerUnequip(Player* p, Item* /*it*/) override { if (enabled) Later(p); }
    void OnPlayerUpdateZone(Player* p, uint32 /*zone*/, uint32 /*area*/) override { if (enabled) Later(p); }

    // stacks falling off end a stack-based look; low mana
    void OnPlayerUpdate(Player* p, uint32 diff) override
    {
        if (!enabled)
            return;
        PlayerState& s = State(p);
        s.timer += diff;
        if (s.timer < 1000)
            return;
        s.timer = 0;
        if (s.transformed)
            RefreshLook(p);
        // LOW_MANA
        if (p->IsAlive() && p->GetMaxPower(POWER_MANA) > 0)
            ForEach(p, LOW_MANA, [&](Bonus const& b)
            {
                if (p->GetPower(POWER_MANA) < uint32(p->GetMaxPower(POWER_MANA) * b.value2 / 100.0f) && Ready(p, b))
                    Fire(p, b, p);
            });
    }
};

void AddTemucraftSetBonusesScripts()
{
    new TemucraftSetBonusesWorld();
    new TemucraftSetBonusesUnit();
    new TemucraftSetBonusesPlayer();
}
