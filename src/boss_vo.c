#include "boss_vo.h"
#include "scenes.h"     // playMusicVol

// Rate del driver XGM2 para los WAV (rescomp los convierte a este rate).
#define BOSS_VO_RATE       13300
// Aire despues de la frase, antes del tema (y margen por la latencia del Z80).
#define BOSS_VO_TAIL_F     6

static const u8* pendingMusic = NULL;
static u16       pendingVol   = 0;
static const u8* pendingVo    = NULL;
static u32       pendingLen   = 0;
static u16       startDelay   = 0;    // frames hasta tocar la voz
static u16       framesLeft   = 0;    // frames hasta arrancar el tema

// La voz sale DOS frames despues del XGM2_stop: el stop es un "play" de un
// tema nulo y el Z80 lo procesa en su propio ciclo; tocar el PCM en el mismo
// frame arriesga que el arranque del tema nulo lo pise en CH1.
#define BOSS_VO_STOP_DELAY  2

void bossVoReset(void) {
    pendingMusic = NULL;
    pendingVo    = NULL;
    startDelay   = 0;
    framesLeft   = 0;
}

void bossVoStart(const u8* vo, u32 len, const u8* music, u16 musicVol) {
    const u32 fps = IS_PAL_SYSTEM ? 50 : 60;
    // Silencio: se corta lo que este sonando (el tema del nivel) para que la
    // voz quede sola.
    XGM2_stop();
    pendingVo    = vo;
    pendingLen   = len;
    pendingMusic = music;
    pendingVol   = musicVol;
    startDelay   = BOSS_VO_STOP_DELAY;
    framesLeft   = (u16)((len * fps) / BOSS_VO_RATE + BOSS_VO_TAIL_F);
}

bool bossVoActive(void) { return (startDelay > 0) || (framesLeft > 0); }

void bossVoUpdate(void) {
    if (startDelay > 0) {
        if (--startDelay == 0 && pendingVo) {
            // CH1 (el de la musica, libre con el tema parado) y prioridad 15:
            // ningun efecto del juego usa CH1, asi que nada la corta.
            XGM2_playPCMEx(pendingVo, pendingLen, SOUND_PCM_CH1, 15, FALSE, FALSE);
            pendingVo = NULL;
        }
        return;
    }
    if (framesLeft == 0) return;
    if (--framesLeft > 0) return;
    if (pendingMusic) {
        XGM2_setLoopNumber(-1);     // SIEMPRE antes del play (el driver lo latchea)
        playMusicVol(pendingMusic, pendingVol);
        pendingMusic = NULL;
    }
}
