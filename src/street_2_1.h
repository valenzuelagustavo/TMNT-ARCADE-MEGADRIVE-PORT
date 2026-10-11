#ifndef _STREET_2_1_H_
#define _STREET_2_1_H_

#include <genesis.h>
#include "props_2_1.h"   // l21_win_*, l21_hydrant*, l21_bill_*, l21_skategirl
#include "player.h"

// ===========================================================================
// UTILERIA DE LA CALLE del nivel 2-1 (11/10)
// ===========================================================================
// Lo que el arcade dibuja ENCIMA del fondo de la SCENE 2 y que el rip no
// tiene. Las posiciones salen del video (gameplayARCADE/tmnt.avi, SCENE 2
// desde 3:07): cada frame se ubico dentro del mapa del nivel por template
// matching (el video es 320x224 a escala 1:1 con el PNG del nivel), y con eso
// cada objeto quedo en coordenadas de mundo. Detalle en street_2_1.c.
//
//   VENTANAS ROTAS   seis ventanas por las que entra un foot soldier. Hasta
//                    que pasa el soldier se ve la del fondo (sana); cuando
//                    salta se pone encima la rota y queda asi.
//   HIDRANTES        dos. Al segundo golpe de tortuga: sale la tapa volando con el
//                    chorro (mata al soldier que toca) y despues queda tirando
//                    agua para siempre (el rocio es solo estetico).
//   CARTEL           el "Stunner" de la pared de abajo. Se cae cuando
//                    aparecen los dos amarillos parados en su andamio.
//   PIZZA            la caja de la cornisa (sobre los portones). La tortuga
//                    que la toca recupera TODA la vida; la caja se abre, sube
//                    el "ENERGY" y desaparece. Una sola vez.
//   CHICA EN SKATE   cruza la vereda de derecha a izquierda una vez. Si una
//                    tortuga le pega, grita (el grito de April) y se agacha.
// ===========================================================================

// Indices de ventana (L21K_WINDOW usa el indice como argumento).
#define L21W_FRONT      0   // edificio del principio (la del E1)
#define L21W_ROBO_1     1   // pared en diagonal, Robodyne
#define L21W_ROBO_2     2
#define L21W_ROBO_3     3
#define L21W_K_1        4   // pared en diagonal, edificio K
#define L21W_K_2        5
#define L21W_COUNT      6

void streetInit(void);
void streetReleaseAll(void);

// Sprites cerca de camara, animaciones, vuelos. Una vez por frame.
void streetUpdate(Player** pls, u8 nPl, s16 camX, s16 camY);

// TRUE si los pies (fx, fy) caen en la base de un hidrante (solido, como los
// parquimetros).
bool streetBlock(s16 fx, s16 fy);

// Golpes de las tortugas a los hidrantes y a la chica. Devuelve TRUE si un
// hidrante recibio un golpe este frame (para el SFX). Se abren al segundo.
bool streetPlayerHits(Player** pls, u8 nPl);

// TRUE si la tapa con el chorro de un hidrante alcanza a este enemigo (lo
// mata: *kill = TRUE). El rocio que queda despues es solo estetico.
bool streetHitEnemy(u16 enemyIdx, s16 ex, s16 ey, s16 halfW, s8* owner, bool* kill);

// --- Ventanas ---------------------------------------------------------------
// Geometria de la ventana 'w' en el mundo (esquina sup. izq. y tamano).
void streetWindowRect(u8 w, s16* x, s16* y, s16* wd, s16* ht);
// TRUE si la ventana entra entera en X y su borde de abajo esta en pantalla
// (para que el soldier salga a la vista).
bool streetWindowVisible(u8 w, s16 camX, s16 camY);
// Rompe la ventana (si ya estaba rota no pasa nada).
void streetBreakWindow(u8 w);

// --- Cartel -----------------------------------------------------------------
void streetBillboardFall(void);

// --- Chica en skate ---------------------------------------------------------
// Entra por el borde derecho de la pantalla (una sola vez).
void streetGirlStart(s16 camX);

#endif
