// ===========================================================================
// garage_van.h - la camioneta de las tortugas, fin del nivel 4 (garage) (30/10)
// ===========================================================================
// Cuando termina la capsula y April esta a salvo, la camioneta entra por la
// izquierda de la pantalla, abre la puerta corrediza, las tortugas saltan
// adentro una tras otra, la puerta se cierra, la camioneta vibra dos veces y
// sale por la derecha. April desaparece cuando la camioneta la tapa.
// Secuencia sacada de capturas del arcade (res/images/lvl_4_garage/secuencia/);
// los tiempos son ESTIMADOS (las capturas no traen reloj). Arte:
// res/images/lvl_4_garage/TURTLE_VAN.png via tools/gen_garage_van.py.
//
// COMO SE DIBUJA: como dibujo de BG_A (30x18 tiles) que se desliza con el
// scroll del plano (stageAOffX / stageAOffY de stage_level; en el garage BG_A
// esta libre). Como sprites serian ~48 sprites de hardware y pasaria el limite
// de tiles por linea con el HUD y las tortugas. Los 468 tiles se guardan en la
// VRAM de sprites con un sprite OCULTO que reserva el bloque (maxNumTile). Se
// dibuja con prioridad baja: las tortugas (sprites) quedan por delante.
// El plano tiene 512 px de ancho: lo que se sale por un lado volveria a
// aparecer por el otro; garage_van.c blanquea las columnas que se repetirian.
// El HUD (WINDOW) tapa las filas de arriba (los 32 px del techo), como los
// paneles del HUD del arcade.
//
// Uso (src/level4_1.c): vanInit() en bossInit; vanUpdate() cada frame despues
// de la capsula (TRUE cuando termino todo); vanRelease() en bossRelease. Si no
// hay VRAM para los tiles se saltea la camioneta (vanUpdate devuelve TRUE).
// ===========================================================================
#ifndef _GARAGE_VAN_H_
#define _GARAGE_VAN_H_

#include <genesis.h>
#include "player.h"

void vanInit(void);
void vanRelease(void);
// Un frame de la secuencia. TRUE cuando termino (o no se pudo hacer).
bool vanUpdate(Player** pls, u8 nPl, s16 camX);
// TRUE desde que la camioneta tapa a April (el nivel libera su sprite).
bool vanAprilCovered(void);

#endif
