/* AmiXcom: the music of the Amiga UFO (1994), played from the player's own
 * game files - the tunes (Geo.mus, Inter.mus ...) and the instrument bank
 * (UFO.bank) of the Amiga release, which are never redistributed.
 *
 * The format is MicroProse's: IFF "DDAT" tunes holding MIDI-like event
 * streams for up to 16 channels, folded onto Paula's four voices, and an
 * IFF "ADAT" bank of 40 8-bit instruments. This replayer follows the
 * EaglePlayer "UFO" replayer (meynaf & Wanted Team, 2009), which adapted the
 * game's own: the same events, period table, envelopes and 24-ticks-a-beat
 * clock driven 60 times a second. Paula's four voices are emulated and mixed
 * into the port's 8-bit music stream (amiga_audio.c), so the sound effects
 * keep their channels.
 *
 * Plain C, no OS calls: buildable on the host (UFOMUS_HOST_TEST).
 */
#ifndef AMIGA_UFOMUS_H
#define AMIGA_UFOMUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Load UFO.bank. 1 on success; safe to call again (no-op). */
int UfoMus_LoadBank(const char *path);
int UfoMus_HaveBank(void);
void UfoMus_FreeBank(void);

/* Start a tune (the whole .mus file in memory; copied). 'loop' repeats it
 * where the tune says so, else it stops there. 1 if it parsed. */
int UfoMus_Play(const unsigned char *mus, unsigned long len, int loop);
void UfoMus_Stop(void);
int UfoMus_Playing(void);

/* Master volume 0..64. */
void UfoMus_SetVolume(int vol);

/* The next 'max' 8-bit signed mono samples at MUSIC_RATE (amiga_music.h);
 * the refill callback of AmigaAudio_MusicStart(). */
int UfoMus_Refill(void *ud, signed char *dst, int max);

/* The .mus file for a GM.CAT position of the PC game (catPos in the
 * ruleset), or NULL when the Amiga release has no such tune (the battles,
 * the intro). */
const char *UfoMus_FileFor(int catpos);

#ifdef __cplusplus
}
#endif
#endif
