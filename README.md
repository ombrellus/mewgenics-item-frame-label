## GUIDE FOR PLAYERS

Simply download the mod from [here](https://github.com/ombrellus/mewgenics-item-frame-label/releases) or the nexus page (add when i make it), load the mod through mewtator, with mewjector installed and enabled, and put it on top of the other mods to prevent issues.

## GUIDE FOR MODDERS

To start simply download the mod like written above, after that, in your mod's items declaration you can add a new field ```alt_clip```, as input of this field you need put the name of the new item icon motion clip, you will need 3 movie clips like the base game found in catparts.swf:
* ALTCLIPNAME
* ALTCLIPNAME_Worn
* ALTCLIPNAME_Broken
> [WARN!]
> Currently the alt_clip field only accepts strings with maximum 15 characters, i have no idea how to make it work for now

For the item's appearence on the actual cat character, you need to append new frames onto the movieclips found inside catparts.swf (like FaceItemF), then in a Label Layer add a label to the frame you want to use with your item's name.

> [WARN!]
> Currently the mod only checks for labels inside the front version of the equipment, so for head, neck and face slots the frames have to match

An example can be found here