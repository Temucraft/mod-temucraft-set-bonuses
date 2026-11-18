#!/usr/bin/env python3
"""Builds what some of mod-temucraft-set-bonuses' rows need beyond existing spells:
- 96394 Krol'dok Mending: hidden passive, blocking may heal you (spell_proc: on block, 30%) with
- 96395 Krol'dok Mending (heal 14-23)
- 96396 the same passive again, worn in cities as well (two chances there: the heal doubled)
- 96397 Summon Violet Elemental: a guardian (creature 610050, a copy of the Syth Arcane Elemental) for 15 sec
- items 610070-610072 (Blessing of the Barrens): greens from the Barrens rares Humar the Pridelord, Swiftmane and
  Takk the Leaper; each gives 2.5% run speed in the Barrens and Stonetalon Mountains, so two of them give 5%.
Writes data/sql/db-world/base/mod_temucraft_set_bonuses_extras.sql. With --client the spells and items also go into
the client's Spell.dbc and Item.dbc (patch-T.MPQ, game closed)."""
import os, sys
sys.path.insert(0, os.path.expanduser('~/tc'))
import dbctool as T

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
PROC, HEAL, PROC_CITY, SUMMON = 96394, 96395, 96396, 96397
ELEMENTAL, ELEMENTAL_FROM = 610050, 19205
# new item, copy of, name, dropped by, chance
ITEMS = [(610070, 4709, "Pridelord's Mantle", 5828, 35),
         (610071, 3057, "Swiftmane Striders", 5831, 35),
         (610072, 5971, "Takk's Leaping Cape", 5842, 35)]
ITEM_DESC = 'Blessing of the Barrens: 2.5% run speed in the Barrens and Stonetalon Mountains for each piece worn.'


def build(dbc):
    C = {name: i for i, (name, kind) in enumerate(T.table_columns('spell_dbc'))}
    mask = dbc.rows[633]

    def blank(spell_id, name, desc, icon_from):
        s = [0] * dbc.fc
        s[0] = spell_id
        s[C['CastingTimeIndex']] = 1
        s[C['RangeIndex']] = 1
        s[C['BaseLevel']] = s[C['SpellLevel']] = 1
        s[C['SpellIconID']] = dbc.rows[icon_from][C['SpellIconID']]
        s[C['SchoolMask']] = 1
        for k in ('Name_Lang_Mask', 'NameSubtext_Lang_Mask', 'Description_Lang_Mask', 'AuraDescription_Lang_Mask'):
            s[C[k]] = mask[C[k]]
        s[C['Name_Lang_enUS']] = dbc.add(name)
        s[C['Description_Lang_enUS']] = dbc.add(desc)
        for n in (1, 2, 3):
            s[C['EffectDieSides_%d' % n]] = 1
            s[C['EffectMultipleValue_%d' % n]] = T.f2u(1.0)
            s[C['EffectBonusMultiplier_%d' % n]] = T.f2u(1.0)
            s[C['EffectChainAmplitude_%d' % n]] = T.f2u(1.0)
        return s

    rows = []
    heal = blank(HEAL, "Krol'dok Mending", 'Heals you for $s1.', 2050)
    heal[C['SchoolMask']] = 2
    heal[C['Effect_1']] = 10                     # heal
    heal[C['EffectBasePoints_1']] = 13
    heal[C['EffectDieSides_1']] = 10             # 14-23
    heal[C['ImplicitTargetA_1']] = 1
    heal[C['DefenseType']] = 1                   # magic
    rows.append(heal)
    for spell_id in (PROC, PROC_CITY):
        p = blank(spell_id, "Krol'dok Mending", 'Blocking has a chance to heal you for 14 to 23.', 2050)
        p[C['Attributes']] = 0x40 | 0x80         # passive, hidden
        p[C['DurationIndex']] = 21
        p[C['ProcTypeMask']] = 0x8 | 0x20        # taken melee hits (spell_proc narrows it to blocks)
        p[C['ProcChance']] = 30
        p[C['Effect_1']] = 6
        p[C['EffectAura_1']] = 42                # proc trigger spell
        p[C['ImplicitTargetA_1']] = 1
        p[C['EffectTriggerSpell_1']] = HEAL
        rows.append(p)
    summon = list(dbc.rows[19804])               # Arcanite Dragonling: guardian summon
    summon[0] = SUMMON
    summon[C['EffectMiscValue_1']] = ELEMENTAL
    summon[C['DurationIndex']] = 8               # 15 sec
    summon[C['Name_Lang_enUS']] = dbc.add('Summon Violet Elemental')
    summon[C['Description_Lang_enUS']] = dbc.add('An arcane elemental fights at your side for $d.')
    summon[C['NameSubtext_Lang_enUS']] = 0
    summon[C['SpellIconID']] = dbc.rows[1953][C['SpellIconID']] if 1953 in dbc.rows else summon[C['SpellIconID']]
    rows.append(summon)
    for s in rows:
        dbc.put(s)
    return rows


base = T.DBC(T.SERVER_DBC + '/Spell.dbc')
for i in (PROC, HEAL, PROC_CITY, SUMMON):
    assert i not in base.rows, i
rows = build(base)
lines = [
    '-- mod-temucraft-set-bonuses: spells, a guardian and items some bonuses use (tools/build_set_extras.py).',
    'DELETE FROM `spell_dbc` WHERE `ID` BETWEEN %d AND %d;' % (PROC, SUMMON),
    T.sql_insert('spell_dbc', rows, base),
    'DELETE FROM `spell_proc` WHERE `SpellId` IN (%d, %d);' % (PROC, PROC_CITY),
    'INSERT INTO `spell_proc` (`SpellId`, `ProcFlags`, `SpellPhaseMask`, `HitMask`, `Chance`) VALUES',
    '(%d, 0x28, 0x2, 0x40, 30),\n(%d, 0x28, 0x2, 0x40, 30);' % (PROC, PROC_CITY),
    '-- the Violet Elemental: a Syth Arcane Elemental that fights for whoever summoned it',
    'DELETE FROM `creature_template` WHERE `entry` = %d;' % ELEMENTAL,
    'DROP TEMPORARY TABLE IF EXISTS `tmp_temucraft_ct`;',
    'CREATE TEMPORARY TABLE `tmp_temucraft_ct` SELECT * FROM `creature_template` WHERE `entry` = %d;' % ELEMENTAL_FROM,
    "UPDATE `tmp_temucraft_ct` SET `entry` = %d, `name` = 'Violet Elemental', `subname` = NULL, `minlevel` = 1, `maxlevel` = 80, "
    "`faction` = 35, `lootid` = 0, `pickpocketloot` = 0, `skinloot` = 0, `mingold` = 0, `maxgold` = 0, `AIName` = '', `ScriptName` = '', "
    "`npcflag` = 0, `flags_extra` = 0, `VerifiedBuild` = -1811;" % ELEMENTAL,
    'INSERT INTO `creature_template` SELECT * FROM `tmp_temucraft_ct`;',
    'DROP TEMPORARY TABLE `tmp_temucraft_ct`;',
    'DELETE FROM `creature_template_model` WHERE `CreatureID` = %d;' % ELEMENTAL,
    'INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) '
    'SELECT %d, `Idx`, `CreatureDisplayID`, 0.6, `Probability`, -1811 FROM `creature_template_model` WHERE `CreatureID` = %d;' % (ELEMENTAL, ELEMENTAL_FROM),
    '-- Blessing of the Barrens: three greens from the Barrens rares',
    'DELETE FROM `item_template` WHERE `entry` BETWEEN 610070 AND 610072;',
    'DROP TEMPORARY TABLE IF EXISTS `tmp_temucraft_it`;',
]
for new, src, name, creature, chance in ITEMS:
    lines += [
        'CREATE TEMPORARY TABLE `tmp_temucraft_it` SELECT * FROM `item_template` WHERE `entry` = %d;' % src,
        "UPDATE `tmp_temucraft_it` SET `entry` = %d, `name` = '%s', `description` = '%s', `bonding` = 2, `VerifiedBuild` = -1811;" % (
            new, name.replace("'", "''"), ITEM_DESC),
        'INSERT INTO `item_template` SELECT * FROM `tmp_temucraft_it`;',
        'DROP TEMPORARY TABLE `tmp_temucraft_it`;',
    ]
lines.append('DELETE FROM `creature_loot_template` WHERE `Item` BETWEEN 610070 AND 610072;')
lines.append('INSERT INTO `creature_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES')
lines.append(',\n'.join("(%d, %d, 0, %d, 0, 1, 0, 1, 1, '%s')" % (c, new, ch, name.replace("'", "''")) for new, src, name, c, ch in ITEMS) + ';')
out = os.path.join(MOD, 'data', 'sql', 'db-world', 'base', 'mod_temucraft_set_bonuses_extras.sql')
open(out, 'w').write('\n'.join(lines) + '\n')
print('wrote', out)

if '--client' in sys.argv:
    tmp = '/tmp/Spell_client.dbc'
    assert T.client_get('DBFilesClient\\Spell.dbc', tmp)
    client = T.DBC(tmp)
    build(client)
    client.save(tmp)
    itmp = '/tmp/Item_client.dbc'
    assert T.client_get('DBFilesClient\\Item.dbc', itmp)
    items = T.DBC(itmp)
    for new, src, name, creature, chance in ITEMS:
        row = list(items.rows[src]); row[0] = new
        items.put(row)
    items.save(itmp)
    T.client_put({'DBFilesClient\\Spell.dbc': tmp, 'DBFilesClient\\Item.dbc': itmp})
    print('client Spell.dbc and Item.dbc updated')
