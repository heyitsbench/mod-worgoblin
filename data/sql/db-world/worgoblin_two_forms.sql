-- Two Forms for worgen: the spell, its scripts, and the Gilnean Barber.
-- The behaviour is in src/Worgoblin.cpp; the appearance table is in
-- data/sql/db-characters/worgen_form_appearance.sql.

-- Spell 68996 "Two Forms" is complete in Spell.dbc, but skill line 789
-- "Racial - Worgen" only has 68975 Viciousness, 68976 Aberration, 68978 Flayer
-- and 68992 Darkflight, so nobody learned it. Blizzard shipped it as a gender
-- pair (68995 -> 20707 Human Male Illusion, 68996 -> 20708 Human Female
-- Illusion); only 68996 is taught, because SkillLineAbility cannot gate on
-- gender. The script picks the display by gender instead.
--
-- ID 31500 is above every row in SkillLineAbility.dbc and in this module's
-- skilllineability_dbc rows, so it is added instead of overwriting one
-- (DBCDatabaseLoader extends the index table to maxID + 1).
-- AcquireMethod 2 = SKILL_LINE_ABILITY_LEARNED_ON_SKILL_LEARN, the value the
-- other four worgen racials use; Player::learnSkillRewardedSpells() skips
-- anything else. The client shows the spell without a client-side row.
DELETE FROM `skilllineability_dbc` WHERE `ID` = 31500;
INSERT INTO `skilllineability_dbc`
  (`ID`, `SkillLine`, `Spell`, `RaceMask`, `ClassMask`, `ExcludeRace`, `ExcludeClass`,
   `MinSkillLineRank`, `SupercededBySpell`, `AcquireMethod`,
   `TrivialSkillLineRankHigh`, `TrivialSkillLineRankLow`,
   `CharacterPoints_1`, `CharacterPoints_2`)
VALUES
  (31500, 789, 68996, 2048, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0);

-- Darkflight ends the human form: its own description in Spell.dbc is
-- "Activates your true form, increasing current movement speed...".
DELETE FROM `spell_script_names` WHERE `spell_id` IN (68996, 68992);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
  (68996, 'spell_worgen_two_forms'),
  (68992, 'spell_worgen_darkflight');

-- Gilnean Barber (creature 9000001): skin and face for the human form, which
-- the barbershop cannot change in WotLK.
--
-- Cloned from 29142 Jelinek Sharpshear (Stormwind's barber) instead of written
-- with a column list: creature_template has 90+ columns that change between
-- AzerothCore versions, and a clone cannot fail on a column it did not know.
DROP TEMPORARY TABLE IF EXISTS `tmp_gilnean_barber`;
CREATE TEMPORARY TABLE `tmp_gilnean_barber` LIKE `creature_template`;
INSERT INTO `tmp_gilnean_barber` SELECT * FROM `creature_template` WHERE `entry` = 29142;
UPDATE `tmp_gilnean_barber` SET
  `entry`      = 9000001,
  `name`       = 'Gilnean Barber',
  `subname`    = 'Human Form',
  `ScriptName` = 'npc_gilnean_barber',
  `npcflag`    = 1,          -- GOSSIP
  `faction`    = 35,         -- friendly to all
  `unit_flags` = 768;        -- IMMUNE_TO_PC | IMMUNE_TO_NPC
DELETE FROM `creature_template` WHERE `entry` = 9000001;
INSERT INTO `creature_template` SELECT * FROM `tmp_gilnean_barber`;
DROP TEMPORARY TABLE `tmp_gilnean_barber`;

-- A worgen, not the barber's own goblin model (29142 has display 25624).
-- 36445 is this module's male worgen NPC display, with a baked texture in
-- CreatureDisplayInfoExtra 24081. 29422 is the player display: the same model
-- without a baked texture, which stands untextured on an NPC.
DELETE FROM `creature_template_model` WHERE `CreatureID` = 9000001;
INSERT INTO `creature_template_model`
  (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES (9000001, 0, 36445, 1, 1, 0);

-- Spawns: Shadowglen, next to where worgen start (playercreateinfo race 12:
-- map 1, 10311.3 832.463 1326.41), and Stormwind's barbershop, next to
-- Jelinek Sharpshear (-8744.62 657.759 105.175).
DELETE FROM `creature` WHERE `id` = 9000001 AND `guid` IN (9000001, 9000002);
INSERT INTO `creature`
  (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`,
   `position_x`, `position_y`, `position_z`, `orientation`,
   `spawntimesecs`, `wander_distance`, `currentwaypoint`, `curhealth`, `curmana`,
   `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`, `ScriptName`,
   `VerifiedBuild`, `CreateObject`, `Comment`)
VALUES
  (9000001, 9000001, 1, 0, 0, 1, 1, 0, 10314.5, 835.2, 1326.41, 5.69632,
   300, 0, 0, 1, 0, 0, 0, 0, 0, '', 0, 0, 'Gilnean Barber - worgen start zone'),
  (9000002, 9000001, 0, 0, 0, 1, 1, 0, -8746.5, 659.6, 105.175, 3.1765,
   300, 0, 0, 1, 0, 0, 0, 0, 0, '', 0, 0, 'Gilnean Barber - Stormwind');
