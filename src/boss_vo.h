#ifndef _BOSS_VO_H_
#define _BOSS_VO_H_

#include <genesis.h>

// ===========================================================================
// VOICE OVER DE ENTRADA DE LOS JEFES (01/10, pedido de Gustavo)
// ===========================================================================
// Cuando un jefe con voice over entra a escena: primero se escucha la frase
// COMPLETA y recien despues arranca el tema del jefe. Asi la voz no queda
// tapada por los canales FM del tema (antes arrancaban juntos y el grito se
// perdia).
//
// La voz tiene prioridad sobre cualquier otro sonido: va en el canal PCM 1
// (SOUND_PCM_CH1, el que usa la musica para sus samples) con prioridad 15. La
// musica esta parada mientras habla, y todos los efectos del juego van por
// CH2/CH3, asi que nada la corta.
//
// Uso: bossVoStart(...) en el instante en que aparece el jefe, y bossVoUpdate()
// UNA vez por frame en el bucle del nivel (cuenta los frames que dura el wav y
// al terminar arranca el tema, con loop infinito). bossVoReset() al iniciar el
// nivel, para no heredar un tema pendiente.
// ---------------------------------------------------------------------------
void bossVoStart(const u8* vo, u32 len, const u8* music, u16 musicVol);
void bossVoUpdate(void);
bool bossVoActive(void);    // TRUE mientras habla (el tema todavia no entro)
void bossVoReset(void);

#endif
