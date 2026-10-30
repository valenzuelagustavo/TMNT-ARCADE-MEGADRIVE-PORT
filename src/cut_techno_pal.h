// GENERADO por tools/gen_cut_techno.py: animacion por paleta del fondo del Technodrome.
// Clase k: el indice TCH_PAL_IDX[k] de PAL0 toma el color TCH_PAL_ANIM[paso][k] en cada paso.
#ifndef CUT_TECHNO_PAL_H
#define CUT_TECHNO_PAL_H
#define TCH_PAL_STEPS 5
#define TCH_PAL_N     4
static const u8 TCH_PAL_IDX[TCH_PAL_N] = { 3, 4, 8, 12 };
static const u16 TCH_PAL_ANIM[TCH_PAL_STEPS][TCH_PAL_N] = {
    { 0x0444, 0x0466, 0x0406, 0x0444 },
    { 0x0466, 0x0444, 0x0406, 0x0444 },
    { 0x0444, 0x0466, 0x0406, 0x0444 },
    { 0x0444, 0x0444, 0x0608, 0x0466 },
    { 0x0444, 0x0466, 0x0406, 0x0444 },
};
#endif
