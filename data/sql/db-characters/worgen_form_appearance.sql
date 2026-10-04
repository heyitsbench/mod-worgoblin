-- Both appearances of a worgen: w_ = wolf form (canonical), h_ = human form.
--
-- The five appearance values live in PLAYER_BYTES (byte 0 skin, 1 face,
-- 2 hair style, 3 hair colour) and PLAYER_BYTES_2 (byte 0 facial hair). Those
-- fields are both what the client draws and what _SaveCharacter() saves. If
-- they were simply swapped for the human form, the human appearance would be
-- written into `characters`, and the worgen appearance would be gone -
-- including on the character list. So this table holds both sets and is the
-- source of truth:
--   * it is loaded on login, and PLAYER_BYTES is forced to the wolf set,
--   * the five bytes are swapped on every form change,
--   * the wolf set is written back in OnPlayerBeforeLogout, which runs BEFORE
--     SaveToDB() in WorldSession::LogoutPlayer.
--
-- A character without a row gets one on its next login, with the human form
-- equal to its worgen choices. That is always valid: the worgen customization
-- ranges are a subset of the human ones in every dimension.
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
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Worgen appearances. w_ = wolf form (canonical), h_ = human form.';
