Use of this module all but requires a clean enUS WoW client, available from https://www.chromiecraft.com/downloads. There have been reported issues with using this patch with the HD patch offered on the CC website, so be aware of that.
The way to install this patch is by:

A) Apply the [worgoblin.patch](https://github.com/benjymansy123/mod-worgoblin/blob/master/worgoblin.patch) to your AC source directory.

B) Replace the applicable DBC files in your AC Data folder with the ones provided in [DBC & MPQ.zip](https://github.com/benjymansy123/mod-worgoblin/blob/master/DBC%20%26%20MPQ.zip)

C) Run the [worgoblin.sql](https://github.com/benjymansy123/mod-worgoblin/blob/master/sql/world/worgoblin.sql) on your acore_world table in your DB.

D) Compile and install your AC server.

E) Drop the patch-A.MPQ file provided in [DBC & MPQ.zip](https://github.com/benjymansy123/mod-worgoblin/blob/master/DBC%20%26%20MPQ.zip) to your client's Data folder. (Note: This patch does require a patched WoW executable in order to be loaded in. Eventually I'll provide steps to patch your executable here, but until then use your Google-fu to figure it out.)

F) Profit
