# SKELETON - Module template

[English](README.md) | [Español](README_ES.md)


## How to create your own module

1. Use the script `create_module.sh` located in [`modules/`](https://github.com/azerothcore/azerothcore-wotlk/tree/master/modules) to start quickly with all the files you need and your git repo configured correctly (heavily recommended).
1. You can then use these scripts to start your project: https://github.com/azerothcore/azerothcore-boilerplates
1. Do not hesitate to compare with some of our newer/bigger/famous modules.
1. Edit the `README.md` and other files (`include.sh` etc...) to fit your module. Note: the README is automatically created from `README_example.md` when you use the script `create_module.sh`.
1. Publish your module to our [catalogue](https://github.com/azerothcore/modules-catalogue).


## How to test your module?

Disable PCH (precompiled headers) and try to compile. To disable PCH, set `-DNOPCH=1` with Cmake (more info [here](http://www.azerothcore.org/wiki/CMake-options)).

If you forgot some headers, it is time to add them!

## Licensing

The default license of the skeleton-module template is the MIT but you can use a different license for your own modules.

So modules can also be kept private. However, if you need to add new hooks to the core, as well as improving existing ones, you have to share your improvements because the main core is released under the AGPL license. Please [provide a PR](https://www.azerothcore.org/wiki/How-to-create-a-PR) if that is the case.

Use of this module all but requires a clean enUS WoW client, available from https://www.chromiecraft.com/downloads. There have been reported issues with using this patch with the HD patch offered on the CC website, so be aware of that.
The way to install this patch is by:

A) Apply the [worgoblin.patch](https://github.com/benjymansy123/mod-worgoblin/blob/master/worgoblin.patch) to your AC source directory.

B) Replace the applicable DBC files in your AC Data folder with the ones provided in [DBC & MPQ.zip](https://github.com/benjymansy123/mod-worgoblin/blob/master/DBC%20%26%20MPQ.zip)

C) Run the [worgoblin.sql](https://github.com/benjymansy123/mod-worgoblin/blob/master/sql/world/worgoblin.sql) on your acore_world table in your DB.

D) Compile and install your AC server.

E) Drop the patch-A.MPQ file provided in [DBC & MPQ.zip](https://github.com/benjymansy123/mod-worgoblin/blob/master/DBC%20%26%20MPQ.zip) to your client's Data folder. (Note: This patch does require a patched WoW executable in order to be loaded in. Eventually I'll provide steps to patch your executable here, but until then use your Google-fu to figure it out.)

F) Profit
