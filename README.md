# mod-temucraft-set-bonuses

Item sets with character: extra, server-side bonuses for old item sets (or single items), on top of their own
bonuses. Wear enough pieces and the set does something that fits where it comes from: the Wailing Caverns set turns a
druid's cat into a serpent, Deadmines leather makes your hits bleed, the Postmaster's set reflects a spell when you're
about to go down, Volcanic Armor makes you quicker in Searing Gorge.

## Shipped bonuses

| Set (pieces) | Bonus |
|---|---|
| Embrace of the Viper (5) | Druids in Cat Form become a serpent: viper for night elves, python for tauren, moccasin for trolls, adder for the rest. Melee hits may stun for 1 sec (3%, 20 sec cooldown) |
| Defias Leather (3 / 4) | +3% damage to humanoids / melee hits may make the target bleed (5%) |
| Chain of the Scarlet Crusade (3) | +3% damage to undead |
| Spirit of Eskhandar (2) | +3% damage to humanoids |
| The Postmaster (4) | Return to Sender: dropping below 25% health reflects the next spell (5 sec, 1 min cooldown) |
| Necropile Raiment (3) | Harmful spells may steal life (8%) |
| Bloodmail Regalia (3) | Melee attackers may start bleeding (10%) |
| Cadaverous Garb (2) | +5% mounted speed in the Western and Eastern Plaguelands |
| Dreadmist (4) / Deathmist (5) Raiment | Melee attackers may flee in fear for 3 sec (10%, 1 min cooldown) |
| Beaststalker Armor (4) | +3% damage to beasts |
| Volcanic Armor (3) | +5% run speed in Searing Gorge and Burning Steppes |
| Stormshroud Armor (3) | +5% run speed in Stranglethorn Vale and Swamp of Sorrows |
| Imperial Plate (4 / 5) | Krol'dok Mending: blocking may heal you for 14 to 23 (30%; twice as often in cities) / Ogre Might: hits dealt or taken may add 1 Stamina (15%), up to 20 stacks for 30 sec; at 20 stacks you become an ogre |
| Vestments of the Devout (4 / 5) | Below 15% mana, regain mana over 10 sec (1 min cooldown; doubled in haunted and blighted lands) / heals may also heal over time (10%) |
| Magister's Regalia (4 / 5) | Damaging spells may keep 15% of mana regeneration while casting for 8 sec (10%) / may summon a Violet Elemental for 15 sec (3%, 1 min cooldown) |
| Shadowcraft Armor (4 / 5) | +1 Swords skill / melee hits may give +2% critical strike chance for 20 sec (5%; doubled in swamps) |
| Chain of the Scarlet Crusade (6) | Scarlet Guardian: dropping below 20% health absorbs 480 damage for 10 sec (4 min cooldown) |
| Blessing of the Barrens (items) | Pridelord's Mantle, Swiftmane Striders and Takk's Leaping Cape (new greens, 35% from Humar the Pridelord, Swiftmane and Takk the Leaper): 2.5% run speed each in the Barrens and Stonetalon Mountains, 5% with two |

Bleeds and life steal scale with the wearer's level. Bonus texts aren't on the item tooltips (the client's ItemSet
data is unchanged); they are listed here and in the table's comments.

## The table

`mod_temucraft_set_bonus` (world database), one row per bonus. `itemset` (ItemSet.dbc id) with `pieces`, or `item`
for a single item. `race` / `class` 0 = anyone (specific rows are checked first). `enabled` 0 turns a row off.

| kind | What | Columns used |
|---|---|---|
| 1 creature damage | more damage to a creature type (pets included) | value1 = creature type (1 beast, 2 dragonkin, 3 demon, 4 elemental, 5 giant, 6 undead, 7 humanoid), value2 = % |
| 2 zone speed | faster in some zones | zones = zone ids, value2 = %, value1 = 1 mounted / 0 running; spell optional |
| 3 on hit | chance to cast a spell on whoever you hit | spell, chance, cooldown, value1 = 0 any / 1 melee and ranged / 2 spells |
| 4 when hit | chance to cast a spell when hit | as on hit; value2 = 1 casts it on yourself instead of the attacker |
| 5 low health | cast a spell on yourself when health drops below value2 % | spell, cooldown |
| 6 stacks | hits dealt or taken may add a stack of a buff | spell, chance, duration, value1 = most stacks, value2 = amount per stack |
| 7 transform | look like a creature | value1 = display id, value2 = form (-1 any, 0 none, 1 Cat, 5 Bear...), stacks = needs that many stacks of the set's stacks row |
| 8 aura | a spell aura while worn | spell, value2 = amount (0 = the spell's own); zones = only there |
| 9 low mana | cast a spell on yourself while mana is below value2 % (checked every second) | spell, cooldown |
| 10 on heal | chance to cast a spell on whoever your heal lands on | spell, chance, cooldown |

Spells cast for a bonus are triggered (free and instant). `bp_per_level` sets the spell's first value to that much per
level of the wearer (negative: exactly that value); for kinds 3, 4, 5, 9 and 10 `zones` are where the value is doubled; `duration` (ms) overrides how long it lasts; `cooldown` (ms) is per player and bonus. Hits from
bonus spells don't set off more bonuses.

## Install

Copy the module into `modules/`, re-run CMake and build. The world SQL creates the table with the bonuses above.

## Extras

`tools/build_set_extras.py [--client]` makes `mod_temucraft_set_bonuses_extras.sql`: spells 96394-96397 (Krol'dok
Mending passive, its heal, the city copy, Summon Violet Elemental) with their `spell_proc` rows, the Violet Elemental
(creature 610050, a guardian copy of the Syth Arcane Elemental at the summoner's level) and the three Barrens items
(610070-610072) with their drops. `--client` adds the spells and items to the client's Spell.dbc and Item.dbc.
Zone speed pieces of the same kind add up.

Part of the Temucraft modules for AzerothCore 3.3.5a.
