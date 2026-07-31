# TMNT Arcade (1987) - Reverse Engineering Reference

## Hardware
- CPU: MC68000 @ 8 MHz
- Audio: Z80
- Sprite gen: K051960 (up to 128 sprites, 16x16 tiles, 8x8 grouping, zoom)
- Tilemap gen: K052109
- Sprite ROM: 2MB (32bit, at SDRAM 0x1200000 on MiSTer)
- Tile ROM: 1MB (32bit)

## Memory Map
| Range | Size | Description |
|-------|------|-------------|
| 0x000000-0x07FFFF | 512KB | ROM (main code + data) |
| 0x060000-0x063FFF | 16KB | Work RAM |
| 0x080000 | | Palettes |
| 0x100000 | | K052109 tilemap |
| 0x140400-0x1407FF | 1KB | K051960 sprite OAM (128 × 8 bytes) |
| 0x060100-0x060107 | 8B | Score BCD display (4 digits × 2 bytes) |
| 0x060120-0x060124 | | Level/section parameters |
| 0x060121 | byte | Section ID (0-9) |
| 0x060128 | byte | Stage ID (0-6) |
| 0x061207 | byte | Difficulty/wave parameter |
| 0x06120C | byte | Active enemy count |

## Entity Pools at 0x062000

Three entity pools form the main object system:

| Pool | Address | Slots | Stride | Size |
|------|---------|-------|--------|------|
| Player | 0x062000-0x06213F | 4 | 0x50 | 0x140 |
| Enemy 1 | 0x062140-0x0623BF | 8 | 0x50 | 0x280 |
| Enemy 2 | 0x0623C0-0x06277F | 12 | 0x50 | 0x3C0 |
| Total | 0x062000-0x06277F | 24 | 0x50 | 0x780 |

A secondary view uses 0x40 stride with wrap at 0x800 (32 entries), overlapping the first 4 enemy slots with player structs.

### Sprite DMA pools (indexed by type&3):
- 0x0623C0, 0x062780, 0x062140, 0x062A00

## Spawn Entry Format (8 bytes)

Both enemy and entity spawn use a common 8-byte table entry at A5:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0x00 | word | X | X offset (signed, ×256 for fixed-point) |
| +0x02 | word | Y | Y offset (signed, ×256 for fixed-point) |
| +0x04 | byte | Z | Z/depth layer |
| +0x05 | byte | type | Enemy type ID (0=player) |
| +0x06 | word | config | Config word (stored to entity+0x02) |

### Spawn tables (8 bytes per entry):
| Address | Count | Notes |
|---------|-------|-------|
| 0x04C6C8 | 3 | Wave A (difficulty ≥ 3): offset(+4,+16) type=2 config=0x0002 |
| 0x04C6D0 | 3 | Wave B: offset(-10,+20) type=2 config=0x0002 |
| 0x04C6D8 | 3 | Wave C (difficulty < 3, harder): offset(-3,+18) type=2 config=0x0002 |
| 0x04C4C6 | 4 | Generic enemy group: type=3, config=0x1A00-0x1A03 |
| 0x04C894 | 1 | Single: X=-32, type=2, config=0x6B00 |
| 0x04C9B6 | 2 | Pair: type=2 + type=1 |
| 0x04CBAC | 1 | Single: type=1, config=0x1F00 |
| 0x04CD3C | 1 | Single: type=1, config=0x1F80 |
| 0x04D1A6 | 1 | Boss? type=0, config=0x0004 |
| 0x04D6B8 | 1 | type=0, config=0x0003 |
| 0x04D75C | 1 | config=0x0002 |

Config word likely represents HP threshold: 0x0002 = grunt, 0x0003-0x0004 = tougher, 0x1F00+ = elite/boss.

## Entity Struct Fields

### Shared fields (both player/enemy, 0x50 bytes)

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| +0x00 | word | flags | Bit 7=active, Bit 6=facing (0=right,1=left), Bit 3=collision-linked, Bit 1=collision-ready. Init 0x0100 |
| +0x02 | word | config | Type or config word from spawn table. For enemies: variant/health tier |
| +0x04 | byte | variant | AI sub-type variant (0-7). Also FSM step counter |
| +0x05 | byte | state | AI state (0-3 after `andi #$3`). Sub-step in FSM |
| +0x06 | byte | attack_dmg | Attack damage value. Set on attack init: `move.b #$a, ($6,A6)` (10 = player punch) |
| +0x07 | byte | dmg_received | Damage received on hit. Copied from attacker's +0x06. Bit 7 = damage direction |
| +0x08 | word | hitstun | Hitstun/combo counter |
| +0x0A | byte | dmg_scale_a | Damage scaling factor A |
| +0x0B | byte | dmg_scale_b | Damage scaling factor B |
| +0x0C | word | screen_x | Screen X coordinate (pixels, 0-512) |
| +0x0E | word | screen_y | Screen Y coordinate (pixels, 0-256) |
| +0x10 | byte | z_pos | Z/height position |
| +0x11 | byte | | Cleared on spawn, observed 0x80 for enemies |
| +0x12 | long | x_pos | X position (24.8 fixed point) |
| +0x16 | long | y_pos | Y position (24.8 fixed point) |
| +0x1A | word | x_vel | X velocity (signed sub-pixels/frame) |
| +0x1C | word | y_vel | Y velocity (signed sub-pixels/frame) |
| +0x1E | byte | state_timer | State timer. Counts up to 24 (0x18), triggers state transition |
| +0x20 | word | x_bound_max | X scroll boundary max |
| +0x22 | word | y_bound_max | Y scroll boundary max |
| +0x24 | long | | Wave/spawn data pointer (used by wave handler) |
| +0x28 | word | parent_link | Copied from parent during spawn |
| +0x30 | byte | timer1 | Timer 1 |
| +0x31 | byte | timer2 | Timer 2 |
| +0x32 | byte | prox_timer | Modified by proximity scans: forward += 8, backward -= 8 |
| +0x33 | byte | anim_counter | Animation frame counter (counts down). Set by AI dispatch |
| +0x34 | word | hitbox_r | Hitbox radius (collision half-width). NOT health. Init: 2-18 per type |
| +0x36 | long | ai_ptr | AI data block pointer. Points to current 10-byte AI entry |
| +0x3A | long | | Secondary data/animation pointer |
| +0x3C | word | | Observed 0x2190 for enemies with active bits |
| +0x3E | word | link_next | Linked list next pointer |
| +0x42 | word | | Init to 0xFF00 on spawn |
| +0x43 | byte | anim_base | Animation base offset. Added to byte 0 of AI data |
| +0x46 | byte | | Timer / state timer (for wave handler) |
| +0x48 | byte | | Size marker? Init to 0x40 |
| +0x4A | long | attacker_link | Backlink to entity that hit us |
| +0x4C | word | victim_link | Link to entity we hit |
| +0x4E | word | collision_link | Collision entity chain |

### Observed Player (Slot 0, Frame 1080, Attract Mode)
```
+00: 81 00 00 00 00 00 00 00 00 00 00 00 01 00 00 10
+10: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
+20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
+30: 00 00 00 00 00 00 00 02 DA 30 00 00 00 00 00 00
+40: 00 00 FF 00 00 00 00 00 00 00 00 00 00 00 00 00
```
- flags=0x81 (bit 7+0), type=0, variant=0, state=0
- screen_x=0x0100, screen_y=0x0010
- ai_ptr=0x0000DA30

### Observed Foot Soldier (Slot 5, Frame 1080 - TYPE 2, VARIANT 2)
```
+00: 81 00 02 00 02 04 00 00 00 00 00 00 00 F4 00 14
+10: 31 80 00 00 F4 24 00 00 14 00 FE F8 FF F4 00 00
+20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
+30: 00 00 00 02 00 00 00 02 5F 2A 00 00 00 00 21 90
```
- flags=0x81, type=0x02, variant=0x02, state=0x04(->0 after &3)
- screen_x=0x00F4(=244), screen_y=0x0014(=20)
- x_vel=0xFEF8(=-264), y_vel=0xFFF4(=-12)
- ai_ptr=0x025F2A

### Observed Enemy (Slot 3, Frame 1080 - TYPE 1, VARIANT 2)
```
+00: 81 00 01 00 02 04 00 00 00 00 00 00 01 03 00 14
+10: 31 80 00 01 03 F4 00 00 14 00 00 58 00 04 00 00
+20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
+30: 00 00 00 02 00 00 00 02 34 4E 00 00 00 00 21 40
```
- flags=0x81, type=0x01, variant=0x02, state=0x04(->0)
- screen_x=0x0103(=259), screen_y=0x0014(=20)
- x_vel=0x0058(=88), y_vel=0x0004(=4)
- ai_ptr=0x02344E

## Damage/Collision System

### Collision Detection (at 0x47204, 0x4732C, 0x47438)
Three collision loops check entity pairs using hitbox radius at +0x34:
```
047258: add.w ($34,A1), D2       ; add defender hitbox to collision check
0472C0: move.w A0, ($4c,A1)      ; defender → attacker link
0472C4: move.l A1, ($4a,A0)      ; attacker → defender link
0472D4: bset   #$3, ($0,A0)      ; mark attacker as collision-linked
0472DA: bclr   #$1, ($0,A1)      ; clear defender collision flag
0472E0: move.b ($6,A0), ($7,A1)  ; COPY ATTACK DMG TO DEFENDER
0472E2: bchg   #$7, ($7,A1)      ; toggle bit 7 (direction marker)
```

### Hitbox radius init per type:
- 0x0002 (at 0x04BB4C): small enemy
- 0x0003 (at 0x04B9C2): medium enemy
- 0x0004 (at 0x04B7FE): standard enemy
- 0x0006 (at 0x04BA68): larger enemy
- 0x0012 (at 0x04C12C): boss/elite (18)

### Animation Data Table at 0x53756
Per-animation-frame data indexed via animation data structure:
```
04747E: move.w ($6,A3), D2       ; index from anim data
047482: lea    $53756.l, A3      ; table base
047488: adda.w D2, A3            ; index into table
```
Likely contains {hitbox_width, hitbox_height, damage, health} per frame.

## K051960 Sprite Format

### OAM Layout (8 bytes per sprite, 128 sprites at 0x140400)

| Byte | Bits | Field |
|------|------|-------|
| 0 | 7 | Active flag |
| 0 | 6-0 | Priority order (0-127, lower=higher pri) |
| 1 | 7-5 | Sprite size (0-7, see grouping table) |
| 1 | 4-0 | Tile code (high 5 bits) |
| 2 | 7-0 | Tile code (low 8 bits) |
| 3 | 7-0 | Color/palette (see callback for TMNT mapping) |
| 4 | 7-2 | Zoom Y (0=normal, >0=shrink) |
| 4 | 1 | Flip Y |
| 4 | 0 | Y position bit 8 (high bit of 9-bit Y) |
| 5 | 7-0 | Y position bits 7-0 |
| 6 | 7-2 | Zoom X (0=normal, >0=shrink) |
| 6 | 1 | Flip X |
| 6 | 0 | X position bit 8 (high bit of 9-bit X) |
| 7 | 7-0 | X position bits 7-0 |

### Coordinate Decoding (from MAME k051960.cpp)
```
ox = (256 * m_buffer[offs + 6] + m_buffer[offs + 7]) & 0x01FF
oy = 256 - ((256 * m_buffer[offs + 4] + m_buffer[offs + 5]) & 0x01FF)
```
In Lua: `raw_x = rw(p, addr+6)`, `raw_y = rw(p, addr+4)`
```
spr_screen_x = raw_x & 0x1FF
spr_screen_y = 256 - (raw_y & 0x1FF)
```
Note: Y is inverted (256 - value), so 0 = bottom, 256 = top.

### Sprite Size Grouping (from k051960.cpp)
| Size | Tiles Wide | Tiles High | Tile pattern layout |
|------|-----------|------------|-------------------|
| 0 | 1 | 1 | Single 16x16 |
| 1 | 2 | 1 | 2 tiles horizontal |
| 2 | 1 | 2 | 2 tiles vertical |
| 3 | 2 | 2 | 4 tiles (2x2) |
| 4 | 4 | 2 | 8 tiles |
| 5 | 2 | 4 | 8 tiles |
| 6 | 4 | 4 | 16 tiles (4x4) |
| 7 | 8 | 4 | 32 tiles |

Tile base code is masked for multi-tile groups:
```
if (w >= 2) code &= ~0x01;  if (h >= 2) code &= ~0x02;
if (w >= 4) code &= ~0x04;  if (h >= 4) code &= ~0x08;
if (w >= 8) code &= ~0x10;  if (h >= 8) code &= ~0x20;
```

### TMNT Sprite Callback (from tmnt.cpp)
```
code |= (color & 0x10) << 9;  // bit 4 of color byte = extra tile bit
color = m_sprite_colorbase + (color & 0x0f);
```

## AI Dispatch System

### Jump Table at 0x208C0
```
01B920: moveq #$0, D3
01B922: move.b ($2,A0), D3     ; D3 = enemy TYPE (offset 0x02)
01B926: lsl.w #2, D3           ; D3 *= 4
01B928: lea $208c0.l, A6       ; AI dispatch table base
01B92E: movea.l (A6,D3.w), A6  ; A6 = table[type] (variant table address)

01B932: move.b ($4,A0), D3     ; D3 = VARIANT (offset 0x04)
01B936: lsl.w #2, D3           ; D3 *= 4
01B938: movea.l (A6,D3.w), A6  ; A6 = subtable[variant] (state table address)

01B93C: move.b ($5,A0), D3     ; D3 = STATE (offset 0x05)
01B940: lsl.w #2, D3           ; D3 *= 4
01B942: movea.l (A6,D3.w), A6  ; A6 = state[state] (AI data block address)

01B946: jmp $47bf2.l           ; Jump to AI interpreter (A6 = data block)
```

### AI Handler Table at 0x1584E
Alternative AI dispatch: `type * 16 + player * 4` → handler address.
Each type has 4 entries (players 0-3). Handlers at $15886+.

### State Transition Timer (at 0x1B990)
```
addq.b #1, ($1e,A0)          ; increment state timer at offset 0x1E
cmpi.b #$18, ($1e,A0)        ; if timer < 24
bcs    $1ba0c                 ; keep same state
clr.b  ($1e,A0)               ; reset timer
addq.b #1, ($5,A0)           ; advance state byte
andi.b #$3, ($5,A0)          ; state = state & 3 (cycle 0-3)
bne    $1b920                 ; if state != 0, re-dispatch
move.b #$1, ($5,A0)          ; if state == 0, set to 1 instead
bra    $1b920                 ; re-dispatch
```
States cycle: 0→1→2→3→1→2→3→... (state 0 visited once then skipped)

### AI Interpreter at 0x47BF2
```
047BF2: move.b (A6), D1         ; D1 = byte 0 of AI data block (anim frame count)
047BF4: add.b ($43,A0), D1      ; D1 += anim_base from struct
047BF8: move.b D1, ($33,A0)     ; Store as animation frame counter
047BFC: move.l A6, ($36,A0)     ; Store AI data pointer
047C00: rts
```

### Animation Sequence Processor at 0x471AE
```
; Entry point
0471B6: subq.b #1, ($33,A0)     ; Decrement animation counter
0471BA: bne $471ee              ; If != 0, keep current frame

; Counter expired - advance to next entry in chain
0471BE: adda.w #$a, A3          ; A3 += 10 (size of one AI entry)
0471C2: tst.w (-$2,A3)          ; Check terminator at bytes 8-9
0471C6: bmi $471ce              ; If negative (0xFFFF), end of chain
0471CA: adda.w #$8, A3          ; Not end: skip 8 more bytes (18 total)

0471CE: move.b (A3), D0         ; D0 = byte 0 of new entry
0471D0: bne $471e2              ; If D0 != 0, use as frame count

; byte0 == 0: redirect or end
0471D4: move.b ($1,A3), D1      ; D1 = byte 1
0471D8: beq $471ea              ; If byte0=0 AND byte1=0: stop (stay at anim=0)
; byte0=0 AND byte1!=0: FOLLOW POINTER at bytes 2-5
0471DC: movea.l ($2,A3), A3     ; A3 = long pointer from bytes 2-5 (REDIRECT)
0471E0: move.b (A3), D0         ; D0 = byte 0 at new address

0471E2: move.l A3, ($36,A0)     ; Store updated pointer
0471E6: add.b ($43,A0), D0      ; Add anim_base
0471EA: move.b D0, ($33,A0)     ; Store as frame counter
0471EE: rts
```

### AI Data Block Format (10 bytes per entry)
```
[byte0] [byte1] [long_data (bytes 2-5)] [word_timer (bytes 6-7)] [word_term (8-9)]
```

| Bytes | Type | Description |
|-------|------|-------------|
| byte0 | u8 | Animation frame counter. 0 = special (redirect/stop) |
| byte1 | u8 | Flags. 0x01/0x02 observed. If byte0=0 and byte1!=0: redirect |
| bytes 2-5 | long | If redirect: pointer to next block. Otherwise: anim data |
| bytes 6-7 | word | Timer/delay value (e.g., 0x0004=4fr, 0x00E8=232fr) |
| bytes 8-9 | word | Terminator. 0xFFFF=chain end. Other values=continue |

Chaining: when `0xFFFF` terminator, advance +10; otherwise advance +18.

## Complete AI Type Table Dump

### Type-to-Table Mappings (from 0x208C0)
| Type | Table Addr | Identification |
|------|-----------|----------------|
| 0 | 0x208D0 | PLAYER / Generic |
| 1 | 0x231F8 | ENEMY1 (Mouser?) |
| 2 | 0x25CD4 | FOOT SOLDIER (confirmed) |
| 3 | 0x2883E | ENEMY3 |
| 4 | 0x208E8 | (alias into type 0 area) |
| 5 | 0x20974 | ENEMY5 |
| 6 | 0x20ADC | ENEMY6 |
| 7 | 0x20BAC | ENEMY7 |
| 8 | 0x20FEE | EFFECT/Projectile |
| 9-31 | (invalid) | Point to non-ROM data |

### Type Mapping Table at 0x139A2 (sprite variation $2C → type $2F)
```
$2C=00-01: 0x81  $2C=02-04: 0x82  $2C=05: 0x02
$2C=06-08: 0x01  $2C=09-0B: 0x00  $2C=0C-0D: 0x80  $2C=0E: 0x81
```
Type at $2F then used with `lsl.w #4` (×16) to index handler table at $1584E.

### Type-Sprite Tile Correlation (from attract mode captures)

**Type 0 (PLAYER):** Tiles 0x0010-0x0040 (HUD), 0x00A8/0x00B0/0x00CC/0x00D4 (UI cursors)

**Type 1 (ENEMY1):** Tiles 0x0032/0x003A/0x0042/0x0052, 0x00C1/0x00C6/0x00C9/0x00CB/0x00CE
- Sequence 0042→00C1→00C6 across frames = animation cycle
- Multiple facing variants (0052, 00D9 variants)

**Type 2 (FOOT_SOLDIER):** Tiles 0x004A, 0x00D9/0x00DB/0x00DE
- 004A at frame 1060, 00D9/00DE at frames 1120+

**Type 34/20/31:** These appear at frames 1720+ (attract mode title/logo screens)
- Type 34: tiles 0x004A/0x005A/0x006E/0x007C/0x0088/0x008E/0x00A9/0x00C5/0x00C9/0x00D5/0x00DB/0x00DD
- These are OUT-OF-RANGE reads beyond the 32-entry AI table (34*4=0x88 reads garbage from $20948)

## Stage Data Table at 0x5078

6 entries × 16 bytes. Indexed by ($60128) stage number (0-5):
```
00504C: move.b $60128.l, D7      ; stage number
005054: lsl.w #4, D7             ; * 16
005056: adda.l D7, A6
005058: move.w (A6)+, ($c,A0)    ; X start
00505C: move.w (A6)+, ($e,A0)    ; Y start
005060: move.w (A6)+, ($20,A0)   ; X bound max
005064: move.w (A6)+, ($22,A0)   ; Y bound max
005068: move.w (A6)+, ($1a,A0)   ; X scroll velocity
00506C: move.w (A6)+, ($1c,A0)   ; Y scroll velocity
```

| Stage | Name | X start | Y start | X bound | Y bound | X scroll | Y scroll |
|-------|------|---------|---------|---------|---------|----------|----------|
| 0 (0x5078) | Alley | 0x01C0 | 0x0060 | 0x00A0 | 0x0060 | 0xFFF0 | 0x0000 |
| 1 (0x5088) | Channel 6 | 0x00D0 | 0x00F0 | 0x0160 | 0x0060 | 0x0008 | 0xFFF8 |
| 2 (0x5098) | City St | 0x0040 | 0x0060 | 0x0160 | 0x0060 | 0x0010 | 0x0000 |
| 3 (0x50A8) | Turtle Van | 0x0170 | 0xFF90 | 0x00A0 | 0x0060 | 0xFFF8 | 0x0008 |
| 4 (0x50B8) | Pier | 0x0090 | 0xFF90 | 0x0160 | 0x0060 | 0x0008 | 0x0008 |
| 5 (0x50C8) | Technodrome | 0x0130 | 0x00F0 | 0x00A0 | 0x0060 | 0xFFF8 | 0xFFF8 |

## Wave Spawn System

### Wave handler at 0x04C716
```
04C716: movea.l ($24,A0), A1    ; A1 = wave data structure
04C71A: move.w (A1), D0         ; D0 = trigger X position
04C71C: cmp.w ($c,A0), D0       ; compare with entity X
04C720: bcs skip                ; if trigger X > current X, skip
04C724: move.b ($2,A1), ($46,A0); store next state byte
04C72A: addq.b #1, ($4,A0)      ; advance FSM
```
Wave data format: word(trigger_X), byte(next_state)

### Difficulty-based selection at 0x04C752
```
04C752: cmpi.b #$3, $61207.l    ; check difficulty
04C75A: bcs    $4c776           ; if < 3, use wave C (harder)
04C75E: lea    $4c6c8.l, A5     ; wave A (difficulty >= 3)
04C764: jsr    $47c30.l
04C76A: lea    $4c6d0.l, A5     ; wave B
04C770: jmp    $47c30.l
04C776: lea    $4c6d8.l, A5     ; wave C (difficulty < 3)
04C77C: jmp    $47c30.l
```

## Proximity/Scan System

### Forward Scan (0x01D8AC)
Decrements `($32,A0)` by 8 for up to 20 active enemies ahead.
```
subi.b #$8, ($32,A0)
adda.w #$40, A6          ; advance to next slot
wrap at $800             ; 32 slots x $40
```

### Backward Scan (0x01D7E4)
Increments `($32,A0)` by 8 for up to 20 active enemies behind.
```
addi.b #$8, ($32,A0)
suba.w #$40, A6          ; go to previous slot
wrap at $800
```

## Score System

### Score display at 0x003D1E
```
003D1E: lea $60300.l, A0     ; score buffer (4 entries × 8 bytes)
003D24: lea $60100.l, A1     ; score display RAM (BCD digits)
003D2A: moveq #$3, D7        ; 4 score digits
003D2E: move.w ($6,A0), D6   ; digit counter
003D36: bset D3, $600a2.l    ; mark digit active
003D3C: subq.b #1, D6
003D46: move.w ($0,A1), D4   ; BCD value
003D4E: abcd D5, D4          ; BCD add
003D64: move.w D4, ($0,A1)   ; write back
003D68: subq.w #1, ($6,A0)   ; decrement counter
```

Score events queued via function at 0x004164 (writes to circular buffer at $60010/$60380-$603FF).

### Score RAM:
- $60100-$60107: BCD score digits (4 × 2-byte entries)
- $60300-$6031F: Score digit counters/timers (4 × 8-byte entries)

## RAM Map (from code analysis)
| Range | Purpose |
|-------|---------|
| $60001 | Game state (0-3) |
| $60007 | Stage substate |
| $60010 | Score event queue |
| $60020-$60023 | Input/joystick |
| $60034-$60037 | Input states |
| $60080-$60083 | Flags/timers |
| $600A0-$600A5 | Player/controller status |
| $60100-$60107 | Sprite control slots / BCD score |
| $60120-$60124 | Level/section parameters |
| $60121 | Section ID (0-9) |
| $60128 | Stage ID (0-6) |
| $60300-$6031F | Score digit buffers |
| $61207 | Difficulty/wave parameter |
| $6120C | Active enemy count |
| $62000-$627FF | Entity struct area (24 slots × $50) |
| $62C80-$62FFF | Secondary table (8 × $50) |
| $63510 | Stage scroll data |

## Key Code Addresses
| Address | Function |
|---------|----------|
| 0x003D1E | Score BCD display update |
| 0x004164 | Score event queue writer |
| 0x005046 | Stage data table reader at $5078 |
| 0x006CBC | Foot Soldier AI behavior |
| 0x019B3A | Player init loop (4 × $50 at $62000) |
| 0x019C94 | Enemy struct init |
| 0x01B920 | AI dispatch (type→variant→state→block) |
| 0x01B990 | State transition timer (24-frame cycle) |
| 0x01D7E4 | Backward enemy scan |
| 0x01D8A8 | Forward enemy scan |
| 0x0208C0 | AI dispatch table (32 × 4 bytes) |
| 0x0139A2 | Type mapping table ($2C→$2F) |
| 0x01584E | AI handler table (type×16+player×4) |
| 0x046F88 | Render loop: reads ai_ptr, calls 0x471AE |
| 0x0471AE | Animation sequence processor |
| 0x047204 | Collision detection (entity pair checks) |
| 0x047BF2 | AI data block interpreter |
| 0x047C02 | AI pattern lookup / slot finder |
| 0x047C30 | Enemy spawn |
| 0x047C5A | Player spawn |
| 0x047C88 | Position setup (fixed-point) |
| 0x04C716 | Wave trigger handler |
| 0x053756 | Animation data table (hitbox/damage per frame) |
