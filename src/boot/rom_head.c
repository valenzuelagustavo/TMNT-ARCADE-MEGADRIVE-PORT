#include "genesis.h"
#include "build_config.h"

__attribute__((externally_visible))
const ROMHeader rom_header = {
#if TMNT_FULL
    "SEGA SSF        ",
#elif (MODULE_MEGAWIFI != 0)
    "SEGA MEGAWIFI   ",
#else
    "SEGA MEGA DRIVE ",
#endif
    "(C)SGDK 2022    ",
    "TMNT: Arcade - MEGADRIVE                        ",
    "TMNT: Arcade - MEGADRIVE                        ",
    "GM 00000000-00",
    0x000,
    "JD              ",
    0x00000000,
    // (23/10) Fin de la ROM: el real (4 MB). Antes decia 0x0FFFFF (1 MB, el
    // valor del ejemplo de SGDK) y la SRAM, en 0x200000, quedaba "afuera" de
    // la ROM segun la cabecera: algunos emuladores / flash carts deciden con
    // esto como mapear la SRAM.
    // Con el mapper SSF (rama mapper) la ROM puede llegar a 8 MB.
#if TMNT_FULL
    0x007FFFFF,
#else
    0x003FFFFF,
#endif
    0xE0FF0000,
    0xE0FFFFFF,
    // SRAM de respaldo de 8 bits en las direcciones IMPARES (0xF820): segun
    // la norma de Sega arranca en 0x200001 (SGDK escribe en 0x200001 + 2n).
    "RA",
    0xF820,
    0x00200001,
    0x0020FFFF,
    "            ",
    "DEMONSTRATION PROGRAM                   ",
    "JUE             "
};
