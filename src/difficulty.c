#include "difficulty.h"
#include "player.h"     // dificultad, DIFICULTAD_ARCADE

static const u8 waveExtraPct[DIFF_TIER_MAX + 1] = {   0,  50,  75, 100 };
static const u8 cooldownPct [DIFF_TIER_MAX + 1] = { 100,  80,  70,  60 };
static const u8 bossHpPct   [DIFF_TIER_MAX + 1] = { 100, 150, 175, 200 };

u8 diffTier         = 0;
u8 diffMaxAttackers = DIFF_ATTACKERS_PER_PLAYER;

static u8 waveCarry;                 // centesimos de soldier arrastrados

#define DIFF_BOSS_SLOTS 12
static s16* bossHp[DIFF_BOSS_SLOTS];

__attribute__((noinline)) void difficultySetPlayers(u8 numPlayers) {
    if (numPlayers < 1) numPlayers = 1;
    u8 arcade = (dificultad == DIFICULTAD_ARCADE) ? 1 : 0;
    u8 tier = (u8)(numPlayers - 1 + arcade);
    if (tier > DIFF_TIER_MAX) tier = DIFF_TIER_MAX;
    if (tier != diffTier) {
        // Jefes en plena pelea: la vida que les queda se escala.
        u16 oldP = bossHpPct[diffTier], newP = bossHpPct[tier];
        for (u8 i = 0; i < DIFF_BOSS_SLOTS; i++) {
            s16* hp = bossHp[i];
            if (hp && *hp > 0) {
                s32 v = ((s32)*hp * newP + oldP / 2) / oldP;
                if (v < 1) v = 1;
                if (v > 32000) v = 32000;
                *hp = (s16)v;
            }
        }
    }
    diffTier = tier;
    diffMaxAttackers = (u8)(DIFF_ATTACKERS_PER_PLAYER * numPlayers + arcade * DIFF_ATTACKERS_ARCADE);
}

__attribute__((noinline)) u8 diffWaveExtra(u8 n) {
    u16 acc = (u16)(waveCarry + (u16)n * waveExtraPct[diffTier]);
    waveCarry = (u8)(acc % 100);
    return (u8)(acc / 100);
}

__attribute__((noinline)) u16 diffCooldown(u16 base) {
    return (u16)(((u32)base * cooldownPct[diffTier] + 50) / 100);
}

__attribute__((noinline)) s16 diffBossHp(s16 base) {
    return (s16)(((s32)base * bossHpPct[diffTier] + 50) / 100);
}

__attribute__((noinline)) void diffBossTrack(s16* hp) {
    u8 freeSlot = DIFF_BOSS_SLOTS;
    for (u8 i = 0; i < DIFF_BOSS_SLOTS; i++) {
        if (bossHp[i] == hp) return;
        if (!bossHp[i] && freeSlot == DIFF_BOSS_SLOTS) freeSlot = i;
    }
    if (freeSlot < DIFF_BOSS_SLOTS) bossHp[freeSlot] = hp;
}
