#ifndef STARDSI_AUDIO_H
#define STARDSI_AUDIO_H

enum
{
    MUSIC_MENU,
    MUSIC_GAME
};

typedef enum
{
    SOUND_HOE,
    SOUND_AXE,
    SOUND_TREE_FALL,
    SOUND_WATER,
    SOUND_SEED,
    SOUND_HARVEST,
    SOUND_STEP_GRASS,
    SOUND_STEP_DIRT,
    SOUND_SELL,
    SOUND_BUY,
    SOUND_NO_MONEY,
    SOUND_LEVEL_UP,
    SOUND_SLEEP,
    SOUND_ERROR,
    SOUND_CLICK,
    SOUND_HOVER,
    SOUND_CONFIRM,
    SOUND_BACK,
    SOUND_MENU_OPEN,
    SOUND_MENU_CLOSE,
    SOUND_BAG,
    SOUND_PLACE,
    SOUND_GATE,
    SOUND_SHOP_BELL,
    SOUND_COUNT
} SoundId;

// Starts the audio system. The game stays silent if NitroFS or Maxmod fail,
// and plays music without effects if the soundbank is missing.
void audioInitialize(void);
// Plays a theme on a loop; the game theme cycles through its playlist.
void audioPlay(int theme);
// Plays a short sound effect over the music.
void audioPlaySound(SoundId sound);
// Refills the stream buffer; call once per frame.
void audioUpdate(void);

#endif