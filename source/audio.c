#include "audio.h"

#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static bool streamOpen;
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

    if (!nitroReady || !mmInitNoSoundbank())
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

void audioUpdate(void)
{
    if (streamOpen)
        mmStreamUpdate();
}