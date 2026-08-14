#include "audiolib/fx_man.h"
#include "audiolib/music.h"

#include <string.h>

int FX_ErrorCode = FX_Ok;
int MUSIC_ErrorCode = MUSIC_Ok;

char *FX_ErrorString(int error_number)
{
    (void)error_number;
    return "KyleOS sound is disabled";
}

int FX_SetupCard(int card, fx_device *device)
{
    (void)card;
    if (device != NULL)
        memset(device, 0, sizeof(*device));
    return FX_Ok;
}

int FX_GetBlasterSettings(fx_blaster_config *blaster)
{
    if (blaster != NULL)
        memset(blaster, 0, sizeof(*blaster));
    return FX_Ok;
}

int FX_SetupSoundBlaster(fx_blaster_config blaster, int *voices, int *bits,
                         int *channels)
{
    (void)blaster;
    if (voices != NULL) *voices = 0;
    if (bits != NULL) *bits = 0;
    if (channels != NULL) *channels = 0;
    return FX_Ok;
}

int FX_Init(int card, int voices, int channels, int bits, unsigned rate)
{
    (void)card; (void)voices; (void)channels; (void)bits; (void)rate;
    return FX_Ok;
}

int FX_Shutdown(void) { return FX_Ok; }
int FX_SetCallBack(void (*function)(int32_t))
{
    (void)function;
    return FX_Ok;
}
void FX_SetVolume(int volume) { (void)volume; }
int FX_GetVolume(void) { return 0; }
void FX_SetReverseStereo(int setting) { (void)setting; }
int FX_GetReverseStereo(void) { return 0; }
void FX_SetReverb(int reverb) { (void)reverb; }
void FX_SetFastReverb(int reverb) { (void)reverb; }
int FX_GetMaxReverbDelay(void) { return 0; }
int FX_GetReverbDelay(void) { return 0; }
void FX_SetReverbDelay(int delay) { (void)delay; }
int FX_VoiceAvailable(int priority) { (void)priority; return 0; }
int FX_EndLooping(int handle) { (void)handle; return FX_Ok; }
int FX_SetPan(int handle, int volume, int left, int right)
{
    (void)handle; (void)volume; (void)left; (void)right;
    return FX_Ok;
}
int FX_SetPitch(int handle, int pitch)
{
    (void)handle; (void)pitch; return FX_Ok;
}
int FX_SetFrequency(int handle, int frequency)
{
    (void)handle; (void)frequency; return FX_Ok;
}

#define IGNORE_COMMON() do { (void)ptr; (void)pitchoffset; (void)vol; \
                             (void)left; (void)right; (void)priority; \
                             (void)callbackval; } while (0)

int FX_PlayVOC(uint8_t *ptr, int pitchoffset, int vol, int left, int right,
               int priority, uint32_t callbackval)
{
    IGNORE_COMMON(); return FX_Error;
}
int FX_PlayWAV(uint8_t *ptr, int pitchoffset, int vol, int left, int right,
               int priority, uint32_t callbackval)
{
    IGNORE_COMMON(); return FX_Error;
}
int FX_PlayLoopedVOC(uint8_t *ptr, int32_t loopstart, int32_t loopend,
                     int32_t pitchoffset, int32_t vol, int32_t left,
                     int32_t right, int32_t priority, uint32_t callbackval)
{
    (void)loopstart; (void)loopend; IGNORE_COMMON(); return FX_Error;
}
int FX_PlayLoopedWAV(uint8_t *ptr, int32_t loopstart, int32_t loopend,
                     int32_t pitchoffset, int32_t vol, int32_t left,
                     int32_t right, int32_t priority, uint32_t callbackval)
{
    (void)loopstart; (void)loopend; IGNORE_COMMON(); return FX_Error;
}
int FX_PlayVOC3D(uint8_t *ptr, int32_t pitchoffset, int32_t angle,
                 int32_t distance, int32_t priority, uint32_t callbackval)
{
    (void)ptr; (void)pitchoffset; (void)angle; (void)distance;
    (void)priority; (void)callbackval; return FX_Error;
}
int FX_PlayWAV3D(uint8_t *ptr, int pitchoffset, int angle, int distance,
                 int priority, uint32_t callbackval)
{
    (void)ptr; (void)pitchoffset; (void)angle; (void)distance;
    (void)priority; (void)callbackval; return FX_Error;
}
int FX_PlayRaw(uint8_t *ptr, uint32_t length, uint32_t rate,
               int32_t pitchoffset, int32_t vol, int32_t left, int32_t right,
               int32_t priority, uint32_t callbackval)
{
    (void)length; (void)rate; IGNORE_COMMON(); return FX_Error;
}
int FX_PlayLoopedRaw(uint8_t *ptr, uint32_t length, char *loopstart,
                     char *loopend, uint32_t rate, int32_t pitchoffset,
                     int32_t vol, int32_t left, int32_t right,
                     int32_t priority, uint32_t callbackval)
{
    (void)length; (void)loopstart; (void)loopend; (void)rate;
    IGNORE_COMMON(); return FX_Error;
}

#undef IGNORE_COMMON

int32_t FX_Pan3D(int handle, int angle, int distance)
{
    (void)handle; (void)angle; (void)distance; return FX_Ok;
}
int32_t FX_SoundActive(int32_t handle) { (void)handle; return 0; }
int32_t FX_SoundsPlaying(void) { return 0; }
int32_t FX_StopSound(int handle) { (void)handle; return FX_Ok; }
int32_t FX_StopAllSounds(void) { return FX_Ok; }
int32_t FX_StartDemandFeedPlayback(void (*function)(char **, uint32_t *),
                                   int32_t rate, int32_t pitch, int32_t vol,
                                   int32_t left, int32_t right,
                                   int32_t priority, uint32_t callbackval)
{
    (void)function; (void)rate; (void)pitch; (void)vol; (void)left;
    (void)right; (void)priority; (void)callbackval; return FX_Error;
}
int FX_StartRecording(int rate, void (*function)(char *, int))
{
    (void)rate; (void)function; return FX_Error;
}
void FX_StopRecord(void) {}

char *MUSIC_ErrorString(int error_number)
{
    (void)error_number;
    return "KyleOS music is disabled";
}
int MUSIC_Init(int card, int address)
{
    (void)card; (void)address; return MUSIC_Ok;
}
int MUSIC_Shutdown(void) { return MUSIC_Ok; }
void MUSIC_SetMaxFMMidiChannel(int channel) { (void)channel; }
void MUSIC_SetVolume(int volume) { (void)volume; }
void MUSIC_SetMidiChannelVolume(int channel, int volume)
{ (void)channel; (void)volume; }
void MUSIC_ResetMidiChannelVolumes(void) {}
int MUSIC_GetVolume(void) { return 0; }
void MUSIC_SetLoopFlag(int loopflag) { (void)loopflag; }
int MUSIC_SongPlaying(void) { return 0; }
void MUSIC_Continue(void) {}
void MUSIC_Pause(void) {}
int MUSIC_StopSong(void) { return MUSIC_Ok; }
int MUSIC_PlaySong(char *data, int loopflag)
{ (void)data; (void)loopflag; return MUSIC_Ok; }
void MUSIC_SetContext(int context) { (void)context; }
int MUSIC_GetContext(void) { return 0; }
void MUSIC_SetSongTick(uint32_t tick) { (void)tick; }
void MUSIC_SetSongTime(uint32_t milliseconds) { (void)milliseconds; }
void MUSIC_SetSongPosition(int measure, int beat, int tick)
{ (void)measure; (void)beat; (void)tick; }
void MUSIC_GetSongPosition(songposition *pos)
{ if (pos != NULL) memset(pos, 0, sizeof(*pos)); }
void MUSIC_GetSongLength(songposition *pos)
{ if (pos != NULL) memset(pos, 0, sizeof(*pos)); }
int MUSIC_FadeVolume(int volume, int milliseconds)
{ (void)volume; (void)milliseconds; return MUSIC_Ok; }
int MUSIC_FadeActive(void) { return 0; }
void MUSIC_StopFade(void) {}
void MUSIC_RerouteMidiChannel(int channel,
                              int cdecl (*function)(int, int, int))
{ (void)channel; (void)function; }
void MUSIC_RegisterTimbreBank(unsigned char *timbres) { (void)timbres; }
void PlayMusic(char *filename) { (void)filename; }
