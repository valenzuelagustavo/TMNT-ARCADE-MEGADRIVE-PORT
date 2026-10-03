#include "boss_flash.h"

void bossFlashBurn(const u16* src, u16* dst) {
    dst[0] = src[0];
    for (u16 i = 1; i < 16; i++) {
        u16 c = src[i];
        u16 r = (c >> 8) & 0xF, g = (c >> 4) & 0xF, b = c & 0xF;
        r = (u16)((r << 1) | (r >> 3)); if (r > 0xF) r = 0xF;
        g = (u16)((g << 1) | (g >> 3)); if (g > 0xF) g = 0xF;
        b = (u16)((b << 1) | (b >> 3)); if (b > 0xF) b = 0xF;
        dst[i] = (u16)((r << 8) | (g << 4) | b);
    }
}

void bossFlashLoadLine(u16 palLine, const u16* src) {
    u16 burned[16];
    bossFlashBurn(src, burned);
    PAL_setPalette(palLine, burned, DMA);
}

void bossFlashReset(BossFlash* f) {
    f->tick = 0;
    f->on   = 0;
}

bool bossFlashStep(BossFlash* f, s16 hp, s16 maxHp) {
    bool active = hp > 0 && hp <= maxHp / BOSS_FLASH_DIV;
    if (!active) {
        f->tick = 0;
        if (!f->on) return FALSE;
        f->on = 0;
        return TRUE;
    }
    if (f->tick > 0) f->tick--;
    if (f->tick > 0) return FALSE;
    f->tick = BOSS_FLASH_TICKS;
    f->on ^= 1;
    return TRUE;
}
