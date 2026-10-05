# Lynxibear Valley

*Your cozyness loop.*

**Lynxibear Valley** is a native Nintendo DS/DSi homebrew farming game by
**lairLynx Studios**. You run **Lynxibear Farm** and visit the neighboring
town of **Lynxibear Valley**. It is an original game, not a port of any
existing title, however it is heavly inspired by Stardew Valley by Concerned Ape.

## Implemented features

- Farm scene with a walking farmer, grass, tilled soil, crop growth stages,
  and trees to chop
- Axe, hoe, and watering can with swing animations
- Nine seasonal crops across a 28-day calendar for each of four seasons, plus
  rain
- Nine-slot item bar and a 27-slot bag with drag-and-drop on the touch screen
- Town with a seasonal seed shop and a restoration fund (energy and shipping
  price upgrades)
- Harvest and overnight shipping loop
- Bottom-screen touch UI with info, controls, and settings panels
- Optional tile highlighter (on by default, toggle in settings)
- Streamed music: a dedicated menu theme and a looping in-game playlist
- Title menu with Continue / New Game and a single save slot
- SD-card saves with backup and recovery; the game autosaves when you sleep

## Controls

Farm:

| Input | Action |
| --- | --- |
| D-pad | Walk |
| A | Interact with the targeted tile (plant seeds, harvest) |
| B | Use the selected tool |
| Y | Sleep, ship produce, start a new day |
| L / R | Select inventory slot |
| START | Visit town |
| Touch | Select item-bar slots, open `BAG`, `INFO`, `CONTROLS`, `SETTINGS`; drag items between slots |

Town:

| Input | Action |
| --- | --- |
| D-pad | Choose shop, restoration board, or farm gate |
| A | Buy seeds, invest in an upgrade, or return to the farm |
| L / R | Choose a seed offer |
| B / START | Return to the farm |

## Build

Music is not stored in the repository. To include it, run
`python tools\convert_music.py` once (needs `pip install numpy scipy soundfile`); without it the game builds and runs silently.

Requires the [BlocksDS SDK](https://github.com/blocksds/sdk) and the
Wonderful Toolchain. On Windows the build script expects an MSYS2/Wonderful
install at `C:\msys64`.

```bat
compile.bat          :: build lynxibear.nds
compile.bat -B       :: force a full rebuild
compile.bat clean    :: remove generated files
```

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
- Built with [BlocksDS](https://github.com/blocksds/sdk) and libnds.