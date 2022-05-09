# Worgoblin Module

This is a module for [AzerothCore](http://www.azerothcore.org) that adds worgen, goblins, and numerous features related to their playability.

## Important Notes

This module makes *heavy* use of client patching and modification. Because of this, it is recommended to use a [clean, unmodified enUS WoW client](https://www.chromiecraft.com/downloads) as issues have been reported due to use of other clients, such as the one offered from Warmane. There have also been reports of problems with using the HD patch as offered on the ChromieCraft website, so be aware of that.

## How to Install

As of right now, the hooks for allowing custom races is not present in AzerothCore. Because of this, you'll have to manually apply the change required to your AzerothCore source.

### 1) Apply the [worgoblin.patch](https://github.com/benjymansy123/mod-worgoblin/blob/master/worgoblin.patch) to your AzerothCore source directory.

This can be accomplished in a couple different ways. You could do it using Git by using the following command:

`git apply --ignore-space-change --ignore-whitespace modules/mod-worgoblin/worgoblin.patch`

Or you can do it manually through a text editor of your choice by changing the lines required where a "-" indicates the original line and a "+" indicates the change necessary. There are a total of seven lines to change and one line to add.

### 2) Replace the DBC files in your AzerothCore Data directory with the ones provided in [DBC & MPQ.zip](https://github.com/benjymansy123/mod-worgoblin/blob/master/DBC%20%26%20MPQ.zip).

Upon extraction of the zip archive, you'll have a DBC folder and a patch-A.MPQ file. Move all the contents of the DBC folder to your AzerothCore Data directory. Feel free to make a backup of the DBCs you'll be replacing, as backups never hurt.

### 3) Run the [worgoblin.sql](https://github.com/benjymansy123/mod-worgoblin/blob/master/sql/world/worgoblin.sql) on your world table in your DB with any DB management program of your choice. (e.g. HeidiSQL)

### 4) Compile and install AzerothCore.

### 5) Move the [DBC & MPQ.zip](https://github.com/benjymansy123/mod-worgoblin/blob/master/DBC%20%26%20MPQ.zip) file to your Data folder in your WoW client.

### 6) Remove signature checks from your WoW executable using [the patcher](https://github.com/benjymansy123/mod-worgoblin/blob/master/wow_unsig(12340).exe).

Because WoW uses signature checks so as to not allow Interface files to be modified, we need to remove those checks in order to be able to use custom races. This is accomplished using the patcher included in the repo. Move the patcher into your WoW client directory and run it. Again, feel free to make a backup of your original executable.

And with that, you are all done!

## Credits

* mthsena for creating a repository for the [original script](https://github.com/mthsena/trinitycore_scripts/tree/master/scripts/CustomRaces) for TrinityCore.
* [yuan2105](https://github.com/yuanf225) for racing me to get these working and helping me out on multiple occasions.
* Various users on various Discords for helping me out on a slew of issues with almost everything. (ragestriker#8037 and Mr.MA#0957 in particular)
