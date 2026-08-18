---
name: tmnt-sgdk
description: Use when working on this TMNT Mega Drive/Genesis SGDK project — for writing C code, editing .res resource files, debugging VDP/sprite/palette issues, building the ROM, or understanding game architecture. Covers SGDK API, Mega Drive hardware limits, project conventions, and known pitfalls.
---

# TMNT Mega Drive Port — SGDK Development Skill

## Build

```
& "$env:GDK\bin\make.exe" -f "$env:GDK\makefile.gen"
```

- `GDK` = `C:\SGDK`. make is NOT on PATH; always use `$GDK\bin\make.exe`.
- `rescomp` regenerates `res/*.h` from `.res` files during build. Never hand-edit generated headers.
- Output: `out/rom.bin`. Test in BlastEm, Gens, or another Genesis emulator.

## Source Layout

```
src/            C source — game logic
  main.c          Scene state machine (while + switch on SceneId)
  scenes/         One module per scene group
    scene_common.h/.c  Shared helpers: clearScene, playMusicVol, justPressedJoy,
                       charMove, levelFadeIn, camera constants, VOL_*, bgScrollTbl
    intro_menu.c  SEGA/Konami/SGDK intros, credits, level 1 title
    intro_arcade.c  Arcade intro (4-phase scroll: sky→buildings→speed→street)
    select.c      Player select + character select
    level1.c      Level 1 (showLevel1)
    level2.c      Level 2 + Rocksteady boss (showLevel2)
    fire.h/.c     Fire streaming animation
    hud.h/.c      P1/P2 HUD sprites + continue system
    ending.c      Ending cutscene
    gameover.c    Game over screen
  enemy.c         Foot soldier AI
  robot.c         Robot mini-boss
  rocksteady.c    Rocksteady boss

res/            Resources + headers (headers live HERE, not src/)
  player.h/.c    Player struct, movement, collision (in res/ by SGDK convention)
  scenes.h       SceneId enum + function prototypes
  *.res          Resource definitions processed by rescomp
  *.h            Generated headers (DO NOT hand-edit)
  sprites/       PNG spritesheets
  images/        Background images, fonts, HUD art
  audio/         WAV voice samples
  music/         VGM/XGM2 music files
```

**Critical quirk:** `player.h`, `player.c`, `scenes.h` live in `res/`, not `src/`.

## Resource Pipeline (.res files)

`rescomp` (SGDK Java tool) processes `.res` → generates C headers + compiled objects.

Split by category:
- `audio.res` — XGM2 music, VGM SFX, WAV voiceovers. Audio goes HERE only.
- `chars.res` — Player spritesheets (4 turtles)
- `enemies.res` — Foot soldier spritesheets
- `level1.res` — Background, fire, HUD, robot, cutscene images
- `level2.res` — Level 2 bg, smoke, boss drill, Rocksteady, boss bullets
- `intro.res` — SEGA intro sprites
- `menus.res` — Menu backgrounds and UI sprites

## Hard-Won Rules

### 1. `.res` comments must be pure ASCII
rescomp reads `.res` as Cp1252. UTF-8 chars like `Í` (0x8D) cause `Input length = 1` errors.

### 2. Measure unique tiles BEFORE choosing graphics technique
VRAM holds ~1400 tiles total. Budget: bg ~495 + fire 64 + sprites ~540 ≈ 1100. Wrong choice forces rewrite.

### 3. `NONE NONE` for streamable resources
Tiles indexed directly from ROM (fire, HP bar, arcade font) MUST use `NONE NONE` — no compression, no dedup. Without it, rescomp reorders tiles and ROM indexing breaks.

### 4. Arcade font: ASCII 32–126 only
No accented characters. All on-screen text must avoid accents.

### 5. Prepare WAV samples at proper amplitude
Mega Drive DAC is 8-bit. Quiet WAVs are inaudible over music. Normalize to ~24% RMS.

### 6. Sprite budget: `SPR_initEx()` not `SPR_init()`
Default 420 tiles is too small for 2 turtles + 4 foot soldiers. Use `SPR_initEx(752)` in main.c. Level 2 uses `SPR_initEx(768)` at scene entry, restores 752 on exit.

### 7. XGM2 for audio, not XGM
XGM2 supports real-time volume: `XGM2_setFMVolume()`/`XGM2_setPSGVolume()`. XGM does not. Use `XGM2_play()` not `XGM_startPlay`.

### 8. Level 1 music at 40% volume
`VOL_MUSIC_LEVEL1 = 40` — saturates FM chip at full volume.

### 9. Scroll reset in clearScene()
Failing to reset H/V scroll on both planes between scenes causes visual glitches. Also resets `HSCROLL_TILE` back to `HSCROLL_PLANE`.

### 10. Tiles in HORIZONTAL strip are NOT contiguous per frame
For `NONE NONE` side-by-side tilesets: tile `(r,c)` of frame N is at `r*STRIP_COLS + N*TILE_W + c`. Load per-tile in DMA loop. Only VERTICAL strips keep frames contiguous.

### 11. Never leave empty rows in spritesheets
rescomp SKIPS fully-transparent rows and TRIMS trailing transparent frames. Empty row silently shifts every animation index after it. Use placeholder row with 1 opaque pixel.

### 12. Images must have height divisible by 8
rescomp rejects images where height % 8 != 0. Pad with transparent rows using PIL: `Image.new('P', (w, target), 0)` + `putpalette()` + `paste()`.

## Palette Maps

### Level 1
| Line | Contents |
|------|----------|
| PAL0 | Background (street tiles) |
| PAL1 | Turtles, HUD frames, HP bar, attack bubble |
| PAL2 | Foot soldiers, fire animation, robot, sparks |
| PAL3 | Orange foot soldier, HUD text (white = PAL3[1]) |

### Level 2
Same as Level 1, with differences:
- PAL0 also hosts drill-capsule sprite (PAL0, low priority)
- PAL1 also hosts ceiling smoke + April hostage
- PAL3 overwritten with Rocksteady boss palette at spawn; PAL3[1] forced white for HUD text

## SGDK API Quick Reference

### VDP (Video Display Processor)
```c
VDP_setPlaneSize(64, 64, TRUE);        // 64×64 tiles, TRUE = keep SAT
VDP_clearPlane(BG_A, TRUE);            // TRUE = DMA
VDP_setBackgroundColor(index);          // CRAM index for backdrop
VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
VDP_setHorizontalScroll(plane, value);
VDP_setVerticalScroll(plane, value);
VDP_drawImageEx(plane, &image, tileAttr, col, row, flip, load);

// Tile attributes
TILE_ATTR_FULL(pal, prio, flipV, flipH, tileIndex)
// pal = 0..3, prio = BG priority, tileIndex = VRAM tile slot
```

### Palette
```c
PAL_setPalette(palLine, data, DMA);     // palLine = PAL0..PAL3 (0..3)
PAL_setColors(cramIndex, data, count, DMA); // cramIndex = global 0..63
PAL_setColor(cramIndex, rgb16);          // single color
PAL_fadeIn(from, to, data, frames, wait);
PAL_fadeOutAll(frames, wait);
PAL_isDoingFade();                       // TRUE while fade in progress
// CRAM layout: PAL0=0..15, PAL1=16..31, PAL2=32..47, PAL3=48..63
// PAL3 entry 1: PAL_setColor(PAL3*16 + 1, 0x0EEE)
```

### Sprites
```c
SPR_initEx(tileBudget);   // set sprite tile budget (default 420)
SPR_end();                // release all sprites
SPR_addSprite(&def, x, y, attr);  // returns Sprite*
SPR_releaseSprite(sprite);
SPR_setPosition(sprite, x, y);
SPR_setAnim(sprite, anim);
SPR_setAnimAndFrame(sprite, anim, frame);
SPR_setDepth(sprite, depth);  // SPR_MAX_DEPTH = behind everyone
SPR_update();              // call once per frame before VBlank
```

### DMA
```c
DMA_transfer(source, dest, len, wordSize, pause);
// VDP DMA during VBlank is safe. Queue before SYS_doVBlankProcess().
```

### System
```c
SYS_doVBlankProcess();    // wait for VBlank, process DMA queue, update sprites
JOY_readJoypad(joy);      // JOY_1 or JOY_2
justPressedJoy(joy, button); // custom helper (scene_common.c)
IS_PAL_SYSTEM;            // 50 Hz PAL vs 60 Hz NTSC
```

### Audio (XGM2)
```c
XGM2_play(&music);        // start music
XGM2_stop();              // stop music
XGM2_setFMVolume(vol);    // 0..15
XGM2_setPSGVolume(vol);   // 0..15
```

### Image/Tileset structs (from SGDK)
```c
typedef struct {
    Palette *palette;     // 16 colors (u16 data[16])
    TileSet *tileset;     // compressed tile data
    TileMap *tilemap;     // compressed tilemap
} Image;

// Tileset/TileMap are APLIB-compressed when BEST ALL is used.
// Raw access: unpackTileMap() / allocateTileMap() from tools.h
```

## VRAM Budget (Mega Drive)

Total VRAM: 64 KB = ~1792 tiles (32 bytes each).
Layout:
- System tiles (SGDK): ~96 tiles
- User tiles (game): up to ~1400 tiles
- Sprite SAT: 32×64 or 64×32 entries depending on plane size

With 64×64 plane: `maps_addr=0xA800`, `userTileMaxIndex≈828`.
With 32×32 plane: more room for sprites.

## Architecture Patterns

### Scene state machine
`main.c`: `while(1)` + `switch(SceneId)`. Each `showXxx()` runs a full scene, returns next SceneId. Adding screens = new enum + function + case.

### Background streaming (Level 1)
1376px wide bg on 64-tile plane. ~495 unique tiles loaded once. 64-tile circular window reveals new columns as camera advances right. Camera never moves left.

### Fire/smoke animation
Single 64×64 frame in VRAM on BG_A. Swapped every N frames via DMA from 8-frame strip in ROM. HP bar uses same technique.

### Multi-instance player
All player functions take `Player*`. Two instances (P1, P2) managed independently. Design was multi-instance from day one.

### Enemy group AI
Pool of 8 structs, max 4 alive, 2 attacking. Target re-evaluated every 32 frames with 48px hysteresis. Two variants: purple (flanks) and orange (kites with shurikens).

## Common Debugging

### Garbled graphics after scene transition
→ Missing `clearScene()` or scroll/plane reset. Check `VDP_setScrollingMode`, `VDP_setHorizontalScroll`, `VDP_setVerticalScroll`, `VDP_setPlaneSize` in cleanup.

### Wrong palette colors
→ CRAM index math: `PAL_setColor(PAL3*16 + 1, color)` not `PAL_setColor(PAL3, 1, ...)`. PAL3 entry 1 = CRAM index 49.

### Sprites invisible / wrong tiles
→ `SPR_initEx()` budget too low, or tile indices overlap with bg tiles. Check `TILE_ATTR_FULL` pal/index values.

### Build fails with "Input length = 1"
→ UTF-8 character in `.res` file. Replace with ASCII equivalent.

### rescomp rejects image height
→ Height must be multiple of 8. Pad with transparent rows.

### Missing tiles after scene switch
→ Old scene's tiles still in VRAM. Use `VDP_clearPlane()` or `clearScene()` before loading new content.
