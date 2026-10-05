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
--   * this table holds the human set, and `human` = 1 when the last save was
--     in human form. If the server goes down then, w_ is the wolf set to
--     restore on the next login,
--   * the five bytes are swapped on every form change.
--
-- A character without a row gets one on its next login, with the human form
-- set to its worgen choices, each moved to the next valid human value where
-- the worgen one has no human equivalent.
CREATE TABLE IF NOT EXISTS `worgen_form_appearance` (
  `guid`          INT UNSIGNED     NOT NULL COMMENT 'characters.guid',
  `human`         TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '1 = saved in human form, w_ is the wolf set',
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
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Worgen appearances. w_ = wolf form (canonical), h_ = human form.';
