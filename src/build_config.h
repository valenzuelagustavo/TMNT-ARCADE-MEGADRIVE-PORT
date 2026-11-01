#ifndef BUILD_CONFIG_H
#define BUILD_CONFIG_H

// ---------------------------------------------------------------------------
// build_config.h (01/11) -- EDICION del build: FULL (con mapper) o LITE (sin mapper)
// ---------------------------------------------------------------------------
// Una sola base de codigo, dos ediciones. La edicion sale del flag de SGDK
// ENABLE_BANK_SWITCH, que ponen los scripts de tools/mapper/:
//
//   build_rom_ssf.bat    -> -DENABLE_BANK_SWITCH=1 -> TMNT_FULL (mapper SSF, puede pasar de 4 MB)
//   build_rom_plain.bat  -> sin flag               -> TMNT_LITE (sin mapper, hasta 4 MB)
//
// LITE es para flash carts viejos / cartuchos sin mapper (Everdrive MD v2.6 y
// anteriores no arrancan con la cabecera SSF). El contenido "extra" que no
// entre en 4 MB se envuelve asi:
//
//   #if TMNT_FULL
//       ...escenarios nuevos, voice overs extra, etc...
//   #endif
//
// y sus recursos van en .res propios que el build LITE no incluye. Hoy las dos
// ediciones tienen el MISMO contenido (la ROM mide menos de 4 MB).
// ---------------------------------------------------------------------------
#include "genesis.h"

#if (ENABLE_BANK_SWITCH != 0)
  #define TMNT_FULL     1
  #define TMNT_LITE     0
  #define TMNT_EDITION  "FULL"
#else
  #define TMNT_FULL     0
  #define TMNT_LITE     1
  #define TMNT_EDITION  "LITE"
#endif

#endif // BUILD_CONFIG_H
