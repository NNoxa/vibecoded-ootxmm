# [VIBECODED] SoHx2s2h Randomizer - Alpha Out NOW

# [PLEASE READ!]

[NEED PLAYTESTERS!] - Expect Bugs/Crashes!

Github Link: https://github.com/NNoxa/vibecoded-ootxmm


This uses SoH ver. Ackbar Delta 9.2.3 and 2ship ver. Keiichi Charlie 4.0.2. This is in no way associated with official Harbour Masters 64 Projects.
Expect to run into many bugs and crashes along the way. This is a vibecoded project, I just wanted to make something fun to play until someone makes a real 
version. It is built using Skijer's Not Enough Items Mod for SoH, but translated to the current latest build of SoH and 2ship (Big thanks to him for laying 
the groundwork! Check out his mod below, there are so many cool items that are not in this combo randomizer mod). I thought the introduction of MM transformation 
masks would be a nice addition to this, so I decided to just bake it into this mod. These aren't perfect implementations by any means, but I figured there's still fun to be had.

Skijer's Not Enough Items SoH Mod (Download not required for this mod): https://github.com/skijer/Shipwright/tree/Not-Enough-Items

Switch between Ship of Harkinian and 2ship2harkinian via the Happy Mask Shop (OoT) and Clock Tower Interior (MM). Items are shuffled between both games.
The first time loading 2ship after generating the o2r can take upwards of ~30s
I recommend enabling pause menu saving and autosaving in MM

MM Transformation Masks in OoT:
Including:
  - Deku Mask
  - Goron Mask
  - Zora Mask
  - Fierce Deity Mask

Finding these masks in either game results in them being found in both games. 
In OoT, these masks can be found in the typical mask slot (last inventory slot on the items page).

Some sub-par items/features:

  - '(in OoT)' - Stray fairies use a default "Fairy Refill" texture in OoT, rather than the 2D sprite found in MM. The color val of these also do not reflect where the stray fairy belongs to (all pink).
  - '(in OoT)' - Progressive Sword (MM) drops, like Razor Sword and Gilded Sword have 2 separate models, so you may find Gilded Sword item model in OoT before Razor Sword, but it will still give you the next progressive sword regardless.
  - '(in OoT)' - Typically, the first MM item model that loads in a new scene may result in about a ~0.5s lagspike. Don't panic, this should only happen for the first foreign model.
  - '(in OoT)' - Any item/model from MM that has a "fog" backgound (ie. Moon's Tear, Boss Souls), will not have the fog
  - '(in OoT)' - All MM skulltula tokens will look like an OoT skulltula token
  - '(in MM)' - Animated item textures from OoT are glitchy, such as the Spiritual Stones
  - '(in MM)' - OoT enemy and boss souls will use the model of a Fairy Refill
  - '(in OoT/MM)' - Hylian Shield found in MM will use the Hero's Shield model, and vice versa; OoT Hookshot found in MM will use MM Hookshot model, and vice versa *trying to get this fixed ASAP*
  - '(in OoT/MM)' - You are free to use entrance randomizer, but expect some issues, there isn't any additional logic added to prevent it from breaking
  - '(in OoT/MM)' - Expect some items to have non-identical names to their true names
  - '(in OoT/MM)' - Non-Randomizer settings (Audio, Controls, Enhancements, DevTools) are only accessible for the game you are currently playing. Go to MM to change the MM settings.
  - '(in OoT/MM)' - Some shuffles in the Combo Randomizer Menu may not behave exactly as expected.
  - *please describe any other faulty logic not mentioned here on Discord! I haven't completed a full run yet*

A new tab has been added to the SoH & 2ship Menu called "Combo Randomizer", the settings in here are the only settings that will affect seed generation; don't worry about changing the individual Randomizer tabs in each game.
This menu is also shared and synced between the two games.

Some items/features that are in the Combo Randomizer Menu:

  - Shuffle Owl Statues
  - Shuffle Stray Fairies
  - Shuffle Gold Skulltulas
  - Shuffle Boss Souls
  - Shuffle Enemy Souls (MM)
  - Shuffle Pots/Grass/Crates 
  - Shuffle Ocarina Buttons (1 set total for the two games)
  - Shuffle Fishing Pole
  - Shuffle Roc's Feather
  - Shuffle Fairy Fountains
  - Shuffle MM Days/Nights
  - Shuffle Shops & Scrub Merchants
  - Containers Match Content (There may be some incorrect categorization, would probably be a less frustrating experience with this OFF)
  - Shuffle Dungeon Entrances (Works cross-game!)
  - Gossip Stone Hints (For the Alpha, I would be iffy on trusting these all the time)


Some features that are not included in the Combo Randomizer Menu (sorry!):

  - Shuffle Trees & Bushes
  - Shuffle Bean Faires 
  - Shuffle Child's Wallet
  - Shuffle Swim, Grab, Climb, Crawl, Jabber Nuts, Bean Souls
  - Triforce Hunt
  - MM Shuffle Frogs
  

Some things to note:

  - Happy Mask Shop -> Clock Tower Interior: going from one location to the other will result in the current game running to be closed, and the opposite game to be opened. This is behaving correctly, don't panic!
  - A combined Randomizer Menu has been added to both games, and each are synced to one another. 
  - When in MM, using the restart key combination (Ctrl + R) will send the player back to the OoT title screen. Saves automatically.
  - If the game is closed while MM is currently open (ie. hitting the 'X' for the window), it will close MM then open OoT. You can safely close OoT after it opens to fully leave. Alternatively, instead of closing MM, you can use Ctrl + C on the .exe that opens with this mod.
  - The seed logic is Nearly No Logic, where there are hard blocks preventing cases like: Sonata of Awakening / Deku Mask being in Woodfall Temple, or Megaton Hammer being the Volvagia Reward.
  - If you're deleting a save file, make sure the last location you were in in that save file was OoT! If you forget this and load in the new save, whatever you found last time in MM will be given to you in the new save. So all you have to do then is delete the save again and regenerate seed.
  - Placeholder Models are used when the proper model for an item cannot be found - This is only for bugs/edge-cases, you will still see most non-native models in game (will fix later!)
    1. OoT Item Placeholder: Chest Model
    2. MM Item Placeholder: Powder Keg



 # SETUP GUIDE:
    
  1. Look under the Releases tab in GitHub (Right hand side)
  2. Download the Latest Release
  3. This zip should have 5 folders: ootxmm/, soh/, 2ship/, mods/, roms/
  4. Place your OoT & MM ROMs inside of roms/
  ROMs I use:
    -OoT: (EU)(Beta)(GameCube)(Debug)
    -MM:  (USA)
    -*Not saying this won't work with different ROMs(?), but these were the baseline*
  5. Enter ootxmm/
  6. Find ootxmm.exe

Done.

Please describe issues you run into on Discord, under soh-modding

Some frustrating known bugs can be found in todo.txt

    



