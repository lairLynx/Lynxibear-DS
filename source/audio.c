#include "audio.h"

#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// The soundbank header only exists when audio/ holds WAV files at build time.
#if __has_include("soundbank.h")
#include "soundbank.h"
#define HAVE_SOUNDBANK 1
#endif

enum
{
    STREAM_RATE = 22050,
    STREAM_SAMPLES = 4096,
    GAIN_ONE = 32768,
    // Fade time of about 0.4 s at the stream rate.
    GAIN_STEP = GAIN_ONE / (STREAM_RATE * 2 / 5)
};

static const char *const menuTracks[] = { "nitro:/music/menu.pcm" };
static const char *const gameTracks[] = {
    "nitro:/music/game1.pcm",
    "nitro:/music/game2.pcm",
};

#ifdef HAVE_SOUNDBANK
static const u8 soundIds[SOUND_COUNT] = {
    [SOUND_HOE] = SFX_HOE_DIG,
    [SOUND_AXE] = SFX_AXE_CHOP,
    [SOUND_TREE_FALL] = SFX_TREE_FALL,
    [SOUND_WATER] = SFX_WATERING,
    [SOUND_SEED] = SFX_SEED_PLANT,
    [SOUND_HARVEST] = SFX_HARVEST_POP,
    [SOUND_STEP_GRASS] = SFX_STEP_GRASS_L,
    [SOUND_STEP_DIRT] = SFX_STEP_DIRT_L,
    [SOUND_SELL] = SFX_SELL_COINS,
    [SOUND_BUY] = SFX_BUY,
    [SOUND_NO_MONEY] = SFX_NOT_ENOUGH_MONEY,
    [SOUND_LEVEL_UP] = SFX_JINGLE_LEVEL_UP,
    [SOUND_SLEEP] = SFX_SLEEP,
    [SOUND_ERROR] = SFX_UI_ERROR,
    [SOUND_CLICK] = SFX_UI_CLICK,
    [SOUND_HOVER] = SFX_UI_HOVER,
    [SOUND_CONFIRM] = SFX_UI_CONFIRM,
    [SOUND_BACK] = SFX_UI_BACK,
    [SOUND_MENU_OPEN] = SFX_MENU_OPEN,
    [SOUND_MENU_CLOSE] = SFX_MENU_CLOSE,
    [SOUND_BAG] = SFX_INVENTORY_OPEN,
    [SOUND_PLACE] = SFX_PLACE_ITEM,
    [SOUND_GATE] = SFX_GATE,
    [SOUND_SHOP_BELL] = SFX_SHOP_BELL,
};
#endif

static bool streamOpen;
static bool soundsReady;
static bool altHoe;
static bool rightFoot;
static int currentTheme = -1;
static const char *const *playlist;
static unsigned playlistCount;
static unsigned playlistIndex;
static FILE *trackFile;
static int gain;
static bool fadingOut;
static int pendingTheme = -1;

static bool openCurrentTrack(void)
{
    if (trackFile != NULL)
        fclose(trackFile);
    trackFile = fopen(playlist[playlistIndex], "rb");
    return trackFile != NULL;
}

static void selectPlaylist(int theme)
{
    currentTheme = theme;
    playlist = theme == MUSIC_MENU ? menuTracks : gameTracks;
    playlistCount = theme == MUSIC_MENU ? 1 : sizeof(gameTracks) / sizeof(gameTracks[0]);
    playlistIndex = 0;
    openCurrentTrack();
}

// Raw 16-bit mono PCM is read straight into the stream buffer, moving to the
// next track in the playlist (or back to the start) when a file ends.
static mm_word fillStream(mm_word length, mm_addr dest, mm_stream_formats format)
{
    (void)format;
    // A theme change waits for the fade-out to finish, then starts the new
    // track from silence so the switch never clicks.
    if (fadingOut && gain == 0)
    {
        selectPlaylist(pendingTheme);
        fadingOut = false;
    }

    u8 *out = dest;
    s16 *samples = dest;
    size_t remaining = length * sizeof(s16);
    unsigned failedOpens = 0;

    while (remaining > 0)
    {
        size_t got = trackFile != NULL ? fread(out, 1, remaining, trackFile) : 0;
        out += got;
        remaining -= got;
        if (remaining == 0)
            break;

        // Give up with silence if no track in the playlist has any data.
        if (got != 0)
            failedOpens = 0;
        else if (++failedOpens > playlistCount)
        {
            memset(out, 0, remaining);
            break;
        }

        playlistIndex = (playlistIndex + 1) % playlistCount;
        openCurrentTrack();
    }

    for (mm_word i = 0; i < length; i++)
    {
        if (fadingOut)
            gain = gain > GAIN_STEP ? gain - GAIN_STEP : 0;
        else if (gain < GAIN_ONE)
            gain = gain + GAIN_STEP < GAIN_ONE ? gain + GAIN_STEP : GAIN_ONE;
        samples[i] = (s16)(((int)samples[i] * gain) >> 15);
    }

    return length;
}

void audioInitialize(void)
{
    // nitroFSInit makes nitro:/ the working directory, which would send the
    // save file's "/" paths to the read-only ROM instead of the SD card.
    char previousDirectory[PATH_MAX];
    bool hadDirectory = getcwd(previousDirectory, sizeof(previousDirectory)) != NULL;
    bool nitroReady = nitroFSInit(NULL);
    if (hadDirectory)
        chdir(previousDirectory);

    if (!nitroReady)
        return;

#ifdef HAVE_SOUNDBANK
    if (mmInitDefault("nitro:/soundbank.bin"))
    {
        for (int i = 0; i < SOUND_COUNT; i++)
            mmLoadEffect(soundIds[i]);
        mmLoadEffect(SFX_HOE_DIG_2);
        mmLoadEffect(SFX_STEP_GRASS_R);
        mmLoadEffect(SFX_STEP_DIRT_R);
        soundsReady = true;
    }
    else
#endif
    if (!mmInitNoSoundbank())
        return;

    mm_stream stream = {
        .sampling_rate = STREAM_RATE,
        .buffer_length = STREAM_SAMPLES,
        .callback = fillStream,
        .format = MM_STREAM_16BIT_MONO,
        .timer = MM_TIMER0,
        .manual = true,
    };
    gain = 0;
    selectPlaylist(MUSIC_MENU);
    if (trackFile == NULL)
        return;

    mmStreamOpen(&stream);
    streamOpen = true;
}

void audioPlay(int theme)
{
    if (!streamOpen || theme == (fadingOut ? pendingTheme : currentTheme))
        return;

    pendingTheme = theme;
    fadingOut = true;
}

void audioPlaySound(SoundId sound)
{
#ifdef HAVE_SOUNDBANK
    if (!soundsReady || sound < 0 || sound >= SOUND_COUNT)
        return;

    mm_word id = soundIds[sound];
    if (sound == SOUND_HOE)
    {
        altHoe = !altHoe;
        if (altHoe)
            id = SFX_HOE_DIG_2;
    }

    if (sound == SOUND_STEP_GRASS || sound == SOUND_STEP_DIRT)
    {
        // Left and right feet alternate; the right-foot sounds follow the left.
        rightFoot = !rightFoot;
        if (rightFoot)
            id = sound == SOUND_STEP_GRASS ? SFX_STEP_GRASS_R : SFX_STEP_DIRT_R;
    }

    mm_sound_effect effect = {
        .id = id,
        .rate = 1024,
        .handle = 0,
        .volume = 190,
        .panning = 128,
    };
    mmEffectEx(&effect);
#else
    (void)sound;
#endif
}

void audioUpdate(void)
{
    if (streamOpen)
        mmStreamUpdate();
}