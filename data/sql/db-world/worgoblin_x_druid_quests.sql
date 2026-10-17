-- Night elf druid class quests for worgen.
--
-- Worgen start in Shadowglen, but a worgen druid never learns Bear Form: the
-- spell comes only from the reward of "Body and Heart", the end of the
-- Moonglade chain, and every quest in the druid chains is limited to night
-- elves (AllowableRaces = 8). worgoblin.sql only adds worgen to quests that
-- are open to humans, so these stay closed. The druid trainers (Kal,
-- Mathrengyl Bearwalker), Dendrite Starblaze, Tajarri and the Great Bear
-- Spirit have no race condition; the quests are the only gate.
--
-- Opening the existing quests keeps the chains tied to the scripts that
-- complete them: the Great Bear Spirit only shows its gossip option and
-- gives credit for quests 5929 and 5930, so copies with their own IDs can't
-- finish the chain. This replaces the worgen copies (35000-35010) that
-- supplementary/optional-class-quests.sql used to add.
--
-- Chains opened:
--   Bear Form (level 10): Moonglade -> Great Bear Spirit -> Back to Darnassus
--   -> Body and Heart, entered through one of the three "Heeding the Call"
--   quests from a druid trainer.
--   Aquatic Form (level 16): A Lesson to Learn -> Trial of the Lake ->
--   Trial of the Sea Lion -> Aquatic Form.

UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` | 2048
  WHERE `ID` IN (
    26, 29, 272, 5061,        -- A Lesson to Learn, Trial of the Lake, Trial of the Sea Lion, Aquatic Form
    5921,                     -- Moonglade
    5923, 5924, 5925,         -- Heeding the Call (Kal, Theridran, Shalannius)
    5929, 5931, 6001          -- Great Bear Spirit, Back to Darnassus, Body and Heart
  ) AND `AllowableRaces` <> 0 AND (`AllowableRaces` & 8) <> 0 AND (`AllowableRaces` & 2048) = 0;
