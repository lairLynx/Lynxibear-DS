# Lynxibear Valley

*Your cozyness loop.*

**Lynxibear Valley** is a native Nintendo DS/DSi homebrew farming game by
**lairLynx Studios**. You run **Lynxibear Farm** and visit the neighboring
town of **Lynxibear Valley**. It is an original game, not a port of any
existing title, however it is heavly inspired by Stardew Valley by Concerned Ape.

## Implemented features

- Title menu with Continue / New Game and a single save slot; Continue shows
  the saved season, day, and gold
- A 3x3 grid of farm fields with 8x6 plots each; walk off a screen edge to
  reach the neighboring field. New games start on the top-right field
- A fenced crossroads east of the starting field, with trees and bushes, that
  leads to the town (the mines to the north and the beach to the south are
  closed for now)
- Walking farmer with four-direction walk animations; axe, hoe, and watering
  can with full-body swing animations in every direction
- Grass, tilled soil, crop growth stages, and trees to chop
- Nine seasonal crops across a 28-day calendar for each of four seasons, plus
  rain
- Nine-slot item bar and a 27-slot bag with drag-and-drop on the touch screen
- Town with a seasonal seed shop and a restoration fund (energy and shipping
  price upgrades)
- Harvest and overnight shipping loop
- Bottom-screen touch UI with info, controls, and settings panels
- Optional tile highlighter (on by default, toggle in settings)
- Streamed music: a dedicated menu theme and a looping in-game playlist
- Cozy sound effects for tools, planting, harvesting, footsteps, the shop, sleeping, and menus
- SD-card saves with backup and recovery; the game autosaves when you sleep,
  and older saves are upgraded automatically

## Controls

Farm and crossroads:

| Input | Action |
| --- | --- |
| D-pad | Walk |
| A | Interact with the targeted tile (plant seeds, harvest) |
| B | Use the selected tool |
| Y | Sleep, ship produce, start a new day |
| L / R | Select inventory slot |
| START | Visit town (walking east from the crossroads also leads there) |
| Touch | Select item-bar slots, open `BAG`, `INFO`, `CONTROLS`, `SETTINGS`; drag items between slots |

Title menu:

| Input | Action |
| --- | --- |
| D-pad up / down, A | Choose Continue or New Game |
| Touch | Tap a button |
| B | Cancel the New Game confirmation |

Town:

| Input | Action |
| --- | --- |
| D-pad | Choose shop, restoration board, or town gate |
| A | Buy seeds, invest in an upgrade, or leave town |
| L / R | Choose a seed offer |
| B / START | Leave town (back to where you came from) |

## Build

Requires the [BlocksDS SDK](https://github.com/blocksds/sdk) and the
Wonderful Toolchain. On Windows the build script expects an MSYS2/Wonderful
install at `C:\msys64`.

```bat
compile.bat          :: build lynxibear.nds
compile.bat -B       :: force a full rebuild
compile.bat clean    :: remove generated files
```

Music and sound effects are not stored in the repository. Before building, run
`python tools\convert_music.py` and `python tools\convert_sfx.py` once (they
need `pip install numpy scipy soundfile py7zr`). They download the CC0 music and the
CC-BY sound effects and write them to `nitrofs/` and `audio/`, which makes the
ROM about 16 MB. Without them the game builds and runs silently.

Copy `lynxibear.nds` to the SD card of a DSi (or DS with a flashcart) or run
it in an emulator. Saves are written to `lynxibear.sav`.

## Credits

- Game by **lairLynx Studios**.
- Art: **Assets from Sprout Lands by Cup Nooble** (free Basic packs),
  adapted for this project:
  - [Sprout Lands - Asset Pack](https://cupnooble.itch.io/sprout-lands-asset-pack)
  - [Sprout Lands - UI Pack](https://cupnooble.itch.io/sprout-lands-ui-pack)

  The packs permit modification and non-commercial use only; commercial use,
  NFT-related use, and AI training are prohibited. The original asset packs
  are not included in this repository and must not be redistributed.
- Source code is released under the [MIT License](LICENSE). The MIT license
  does not cover the Sprout Lands art, which stays under its creator's terms.
- Music (CC0, from OpenGameArt.org): "Apple Cider" and "Hush Hamlet" by Zane Little Music,
  and "Hot Springs Town" by Kistol. Converted for the DS by `tools/convert_music.py`.
- Sound effects: "Cozy Farm SFX" by Bramble & Byte (https://aquumgifts.itch.io),
  licensed CC-BY 4.0, from
  [OpenGameArt.org](https://opengameart.org/content/cozy-farm-sfx-50-farming-game-sound-effects-tools-harvest-shop-ui-jingles).
  Footsteps are from [Fantozzi's Footsteps](https://opengameart.org/content/fantozzis-footsteps-grasssand-stone)
  by Fantozzi (CC0). Resampled for the DS by `tools/convert_sfx.py`.
- Built with [BlocksDS](https://github.com/blocksds/sdk) and libnds.