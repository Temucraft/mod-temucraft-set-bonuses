-- mod-temucraft-set-bonuses: extra bonuses for item sets (itemset = ItemSet.dbc id) or single items (item, itemset 0).
-- kind: 1 creature damage, 2 zone speed, 3 on hit, 4 when hit, 5 low health, 6 stacks, 7 transform, 8 aura.
-- See the module README for what value1/value2/stacks/zones mean for each kind.
CREATE TABLE IF NOT EXISTS `mod_temucraft_set_bonus` (
    `id`           INT UNSIGNED NOT NULL,
    `itemset`      INT UNSIGNED NOT NULL DEFAULT 0,
    `item`         INT UNSIGNED NOT NULL DEFAULT 0,
    `pieces`       TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `kind`         TINYINT UNSIGNED NOT NULL,
    `race`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `class`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `spell`        INT UNSIGNED NOT NULL DEFAULT 0,
    `chance`       FLOAT NOT NULL DEFAULT 0,
    `cooldown`     INT UNSIGNED NOT NULL DEFAULT 0,       -- ms
    `duration`     INT NOT NULL DEFAULT 0,                -- ms, 0 = the spell's own
    `bp_per_level` FLOAT NOT NULL DEFAULT 0,
    `value1`       INT NOT NULL DEFAULT 0,
    `value2`       FLOAT NOT NULL DEFAULT 0,
    `stacks`       INT UNSIGNED NOT NULL DEFAULT 0,
    `zones`        VARCHAR(255) NOT NULL DEFAULT '',
    `enabled`      TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `comment`      VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DELETE FROM `mod_temucraft_set_bonus` WHERE `id` BETWEEN 1 AND 999;
INSERT INTO `mod_temucraft_set_bonus` (`id`, `itemset`, `pieces`, `kind`, `race`, `class`, `spell`, `chance`, `cooldown`, `duration`, `bp_per_level`, `value1`, `value2`, `stacks`, `zones`, `comment`) VALUES
-- Embrace of the Viper (Wailing Caverns): druids in Cat Form become a serpent, coloured by race; hits may stun briefly
(1,  162, 5, 7,  4, 11, 0,     0,  0,     0,    0,   4312,  1, 0, '', 'Embrace of the Viper: night elf druid, Cat Form -> green viper'),
(2,  162, 5, 7,  6, 11, 0,     0,  0,     0,    0,   8173,  1, 0, '', 'Embrace of the Viper: tauren druid, Cat Form -> python'),
(3,  162, 5, 7,  8, 11, 0,     0,  0,     0,    0,   4305,  1, 0, '', 'Embrace of the Viper: troll druid, Cat Form -> moccasin'),
(4,  162, 5, 7,  0, 11, 0,     0,  0,     0,    0,   4317,  1, 0, '', 'Embrace of the Viper: other druids, Cat Form -> adder'),
(5,  162, 5, 3,  0,  0, 56,    3,  20000, 1000, 0,   1,     0, 0, '', 'Embrace of the Viper (Dream Venom): melee hits may stun for 1 sec'),
-- Defias Leather (Deadmines): hunting people, and bleeding them
(10, 161, 3, 1,  0,  0, 0,     0,  0,     0,    0,   7,     3, 0, '', 'Defias Leather: +3% damage to humanoids'),
(11, 161, 4, 3,  0,  0, 772,   5,  0,     0,    0.5, 1,     0, 0, '', 'Defias Leather (Devious Strike): melee hits may make the target bleed'),
-- Chain of the Scarlet Crusade (Scarlet Monastery)
(20, 163, 3, 1,  0,  0, 0,     0,  0,     0,    0,   6,     3, 0, '', 'Chain of the Scarlet Crusade: +3% damage to undead'),
-- Spirit of Eskhandar
(30, 261, 2, 1,  0,  0, 0,     0,  0,     0,    0,   7,     3, 0, '', 'Spirit of Eskhandar: +3% damage to humanoids'),
-- The Postmaster (Stratholme): Return to Sender
(40, 81,  4, 5,  0,  0, 47981, 0,  60000, 5000, 0,   0,    25, 0, '', 'The Postmaster (Return to Sender): falling below 25% health reflects the next spell (5 sec, 1 min cooldown)'),
-- Scholomance sets
(50, 122, 3, 3,  0,  0, 20004, 8,  0,     0,    0.5, 2,     0, 0, '', 'Necropile Raiment: harmful spells may steal life'),
(51, 123, 3, 4,  0,  0, 772,   10, 0,     0,    0.4, 1,     0, 0, '', 'Bloodmail Regalia: melee attackers may start bleeding'),
(52, 121, 2, 2,  0,  0, 0,     0,  0,     0,    0,   1,     5, 0, '28,139', 'Cadaverous Garb (Rider of the Plaguelands): +5% mounted speed in the Plaguelands'),
-- Warlock dungeon sets: Corrupted Fear
(60, 183, 4, 4,  0,  0, 5782,  10, 60000, 3000, 0,   1,     0, 0, '', 'Dreadmist Raiment (Corrupted Fear): melee attackers may flee in fear for 3 sec (1 min cooldown)'),
(61, 518, 5, 4,  0,  0, 5782,  10, 60000, 3000, 0,   1,     0, 0, '', 'Deathmist Raiment (Corrupted Fear): melee attackers may flee in fear for 3 sec (1 min cooldown)'),
-- Hunter dungeon set
(70, 186, 4, 1,  0,  0, 0,     0,  0,     0,    0,   1,     3, 0, '', 'Beaststalker Armor: +3% damage to beasts'),
-- Biome sets: faster where they come from
(80, 141, 3, 2,  0,  0, 0,     0,  0,     0,    0,   0,     5, 0, '51,46', 'Volcanic Armor: +5% run speed in Searing Gorge and Burning Steppes'),
(81, 142, 3, 2,  0,  0, 0,     0,  0,     0,    0,   0,     5, 0, '33,8', 'Stormshroud Armor: +5% run speed in Stranglethorn Vale and Swamp of Sorrows'),
-- Imperial Plate: Ogre Might. Hits dealt or taken add Stamina, up to 20 stacks; at 20 stacks you become an ogre.
(90, 321, 5, 6,  0,  0, 8099,  15, 0,     30000,0,   20,    1, 0, '', 'Imperial Plate (Ogre Might): hits dealt or taken may add 1 Stamina, up to 20 stacks, 30 sec'),
(91, 321, 5, 7,  0,  0, 0,     0,  0,     0,    0,   12473, -1, 20, '', 'Imperial Plate: at 20 stacks of Ogre Might, become an ogre'),
-- Vestments of the Devout (Rotmender's Raiment): low mana comes back, heals may leave a heal over time
(100, 182, 4, 9,  0,  0, 15604, 0,  60000, 0,     1.1,  0,     15, 0, '10,85,130,28,139,41,361,3,405,4,3483,3520,3433,210', 'Vestments of the Devout: below 15% mana, regain mana over 10 sec, 1 min cooldown (doubled in haunted and blighted lands)'),
(101, 182, 5, 10, 0,  0, 139,   10, 3000,  0,     0.5,  0,     0,  0, '', 'Vestments of the Devout: heals may also heal over time'),
-- Magister's Regalia (Violet Sorcerer's Vestments): mana regeneration while casting, an arcane elemental
(110, 181, 4, 3,  0,  0, 38346, 10, 15000, 8000,  0,    2,     0,  0, '', 'Magister''s Regalia: damaging spells may keep 15% of mana regeneration while casting for 8 sec'),
(111, 181, 5, 3,  0,  0, 96397, 3,  60000, 0,     0,    2,     0,  0, '', 'Magister''s Regalia: damaging spells may summon a Violet Elemental for 15 sec (1 min cooldown)'),
-- Shadowcraft Armor (Defias Enforcer's Garb): swords, critical strikes
(120, 184, 4, 8,  0,  0, 1201338, 0, 0,    0,     0,    0,     1,  0, '', 'Shadowcraft Armor: +1 Swords skill'),
(121, 184, 5, 3,  0,  0, 34837, 5,  0,     20000, -2,   1,     0,  0, '8,15,11,3521', 'Shadowcraft Armor: melee hits may give +2% critical strike chance for 20 sec (doubled in swamps)'),
-- Imperial Plate (Krol'dok Battlegear): blocking may heal (a second chance in cities)
(130, 321, 4, 8,  0,  0, 96394, 0,  0,     0,     0,    0,     0,  0, '', 'Imperial Plate: blocking may heal you for 14 to 23'),
(131, 321, 4, 8,  0,  0, 96396, 0,  0,     0,     0,    0,     0,  0, '1519,1537,1657,3557,1637,1638,1497,3487,3703,4395', 'Imperial Plate: in cities blocking heals twice as often'),
-- Chain of the Scarlet Crusade (Scarlet Guardian)
(140, 163, 6, 5,  0,  0, 23991, 0,  240000, 10000, -480, 0,    20, 0, '', 'Chain of the Scarlet Crusade (Scarlet Guardian): dropping below 20% health absorbs 480 damage for 10 sec (4 min cooldown)');

-- Blessing of the Barrens: single items from the Barrens rares (tools/build_set_extras.py), 2.5% run speed each
INSERT INTO `mod_temucraft_set_bonus` (`id`, `item`, `kind`, `value1`, `value2`, `zones`, `comment`) VALUES
(150, 610070, 2, 0, 2.5, '17,406', 'Pridelord''s Mantle: 2.5% run speed in the Barrens and Stonetalon Mountains'),
(151, 610071, 2, 0, 2.5, '17,406', 'Swiftmane Striders: 2.5% run speed in the Barrens and Stonetalon Mountains'),
(152, 610072, 2, 0, 2.5, '17,406', 'Takk''s Leaping Cape: 2.5% run speed in the Barrens and Stonetalon Mountains');
