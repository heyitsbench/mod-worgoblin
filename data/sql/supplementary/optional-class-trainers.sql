DELETE FROM `creature_template` WHERE `entry` IN (35869, 35872, 49963, 49968);
DELETE FROM `creature_template_model` WHERE `CreatureID` IN (35869, 35872, 49963, 49968);
DELETE FROM `npc_trainer` WHERE `ID` IN (35869, 35872, 49963, 49968);
DELETE FROM `creature_default_trainer` WHERE `CreatureId` IN (35869, 35872, 49963, 49968);

-- Worgen Trainers
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
-- Warlock
(35869, 0, 0, 0, 0, 0, 'Vitus Darkwalker', 'Warlock Trainer', NULL, 1503, 5, 5, 0, 12, 51, 1, 1.14286, 1, 1, 18, 0, 0, 1, 1500, 2000, 1, 1, 1, 768, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 2, '', 0),
(49963, 0, 0, 0, 0, 0, 'Laera Dubois', 'Warlock Trainer', NULL, 4667, 10, 10, 0, 12, 49, 1, 1.14286, 1, 1, 18, 0, 0, 1, 1500, 2000, 1, 1, 1, 512, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 2, '', 0),
-- Mage
(35872, 0, 0, 0, 0, 0, 'Myriam Spellwaker', 'Mage Trainer', NULL, 4660, 5, 5, 0, 12, 51, 1, 1.14286, 1, 1, 18, 0, 0, 1, 1500, 2000, 1, 1, 1, 768, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 2, '', 0),
(49968, 0, 0, 0, 0, 0, 'Lareth Beld', 'Mage Trainer', NULL, 4661, 13, 13, 0, 12, 51, 1, 1.14286, 1, 1, 18, 0, 0, 1, 1500, 2000, 1, 1, 1, 512, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 2, '', 0);

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(35869, 0, 5001, 1, 1, 0),
(35872, 0, 3345, 1, 1, 0),
(49963, 0, 3345, 1, 1, 0),
(49968, 0, 3345, 1, 1, 0);

INSERT INTO `creature_default_trainer` (`CreatureId`, `TrainerId`) VALUES
(35869, 32),
(35872, 17),
(49963, 31),
(49968, 16);
