#ifndef STARDSI_AUDIO_H
#define STARDSI_AUDIO_H

enum
{
    MUSIC_MENU,
    MUSIC_GAME
};

// Starts the music stream. The game stays silent if NitroFS or Maxmod fail.
void audioInitialize(void);
// Plays a theme on a loop; the game theme cycles through its playlist.
void audioPlay(int theme);
// Refills the stream buffer; call once per frame.
void audioUpdate(void);

#endif