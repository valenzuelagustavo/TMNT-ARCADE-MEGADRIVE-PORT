#ifndef _BOSS_FLASH_H_
#define _BOSS_FLASH_H_

#include <genesis.h>

// =============================================================================
// boss_flash -- parpadeo rojo de los jefes con poca vida (03/10)
// =============================================================================
// Ritmo del arcade (medido en el video del arcade):
// desde el ultimo TERCIO de la vida, 4 frames "quemado" / 4 normal, constante
// hasta que muere (sin fase mas rapida al final).
//
// Dos formas de hacerlo:
//   - Cambiando los colores de la linea de paleta del jefe (PAL_setPalette):
//     sirve cuando la linea es SOLO del jefe (2-1, 1-2).
//   - Cambiando la linea del SPRITE (SPR_setPalette) a otra linea que tiene la
//     version quemada de la misma paleta: sirve cuando varios comparten la
//     paleta (garage: Bebop, Rocksteady y April en PAL3; la quemada en PAL2,
//     que en las peleas con jefes nunca usan los foot soldiers). Asi parpadea
//     solo el jefe que tiene poca vida.
// =============================================================================

#define BOSS_FLASH_TICKS   4     // frames por fase (4 quemado / 4 normal)
#define BOSS_FLASH_DIV     3     // parpadea con hp <= vida maxima / 3

typedef struct {
    u8 tick;
    u8 on;
} BossFlash;

// Version "quemada" de una paleta: cada canal x2 (clampeado). El 0 queda igual.
void bossFlashBurn(const u16* src, u16* dst);
// Carga en 'palLine' la version quemada de 'src' (para el modo por sprite).
void bossFlashLoadLine(u16 palLine, const u16* src);
void bossFlashReset(BossFlash* f);
// Avanza el parpadeo. Devuelve TRUE si cambio f->on en este frame.
bool bossFlashStep(BossFlash* f, s16 hp, s16 maxHp);

#endif
