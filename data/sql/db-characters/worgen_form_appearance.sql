-- Both appearances of a worgen: w_ = wolf form (canonical), h_ = human form.
--
-- The five appearance values live in PLAYER_BYTES (byte 0 skin, 1 face,
-- 2 hair style, 3 hair colour) and PLAYER_BYTES_2 (byte 0 facial hair). Those
-- fields are both what the client draws and what _SaveCharacter() saves. If
-- they were simply swapped for the human form, an autosave in human form would
-- write the human appearance into `characters`, and the worgen appearance
-- would be gone - including on the character list. So:
--   * `characters` holds the wolf set, including barbershop and
--     character-screen changes; the wolf set is written back in
--     OnPlayerBeforeLogout, which runs BEFORE SaveToDB() in
--     WorldSession::LogoutPlayer,
--   * this table holds the human set, and the wolf set as of the last save,
--   * a save in human form does write the human set into `characters`. hs_
--     and hp_ list the human sets `characters` may hold until the row is next
--     written (hs_ the last save's, hp_ the one the save writing the row
--     writes; NULL = none). A login that finds one of them in `characters` -
--     the server went down before the next save - restores w_,
--   * the five bytes are swapped on every form change.
--
-- A character without a row gets one on its next login, with the human form
-- set to its worgen choices, each moved to the next valid human value where
-- the worgen one has no human equivalent.
CREATE TABLE IF NOT EXISTS `worgen_form_appearance` (
  `guid`          INT UNSIGNED     NOT NULL COMMENT 'characters.guid',
  `w_skin`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `w_face`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `w_hair`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `w_haircolor`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `w_facialhair`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `h_skin`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `h_face`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `h_hair`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `h_haircolor`   TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `h_facialhair`  TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `hs_skin`       TINYINT UNSIGNED NULL DEFAULT NULL COMMENT 'human set the last save wrote into characters',
  `hs_face`       TINYINT UNSIGNED NULL DEFAULT NULL,
  `hs_hair`       TINYINT UNSIGNED NULL DEFAULT NULL,
  `hs_haircolor`  TINYINT UNSIGNED NULL DEFAULT NULL,
  `hs_facialhair` TINYINT UNSIGNED NULL DEFAULT NULL,
  `hp_skin`       TINYINT UNSIGNED NULL DEFAULT NULL COMMENT 'human set the save writing this row writes into characters',
  `hp_face`       TINYINT UNSIGNED NULL DEFAULT NULL,
  `hp_hair`       TINYINT UNSIGNED NULL DEFAULT NULL,
  `hp_haircolor`  TINYINT UNSIGNED NULL DEFAULT NULL,
  `hp_facialhair` TINYINT UNSIGNED NULL DEFAULT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Worgen appearances. w_ = wolf form (canonical), h_ = human form.';
