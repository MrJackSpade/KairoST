/*
 * KairoST: Hatari's audio.c, for a host that takes each frame's sound itself.
 *
 * Hatari mixes its sound into AudioMixBuffer (sound.c), and its audio.c hands that buffer to an
 * SDL audio device from SDL's callback thread. KairoST builds this file in its place: the host
 * takes the mixed sound between frames (KairoSt_ReadAudio) and plays it through its own audio
 * stream, which also paces the frames. Hatari's thread waits while the host reads, so nothing
 * here needs a lock.
 *
 * Based on Hatari's audio.c. KairoST is free software under the GNU General Public License,
 * version 2 or (at your option) any later version. See LICENSE.md.
 */

#include "main.h"
#include "audio.h"
#include "kairo_st.h"
#include "configuration.h"
#include "sound.h"
#include "dmaSnd.h"
#include "falcon/crossbar.h"

int nAudioFrequency = 44100;			/* Sound playback frequency */
bool bSoundWorking = false;			/* Is sound OK */
/* Sound_Reset primes the mix buffer with this much silence plus a frame; the host's own stream
 * buffers the sound, so there is no device buffer to fill. */
int SoundBufferSize = 0;
int SdlAudioBufferSize = 0;			/* Hatari's SDL device buffer option; unused */
/* Hatari speeds its pacing up or down by this much to follow its audio device; the host paces
 * by its own device, so it stays 0. */
int pulse_swallowing_count = 0;

void Audio_Init(void)
{
	bSoundWorking = ConfigureParams.Sound.bEnableSound;
}

void Audio_UnInit(void)
{
	bSoundWorking = false;
}

void Audio_Lock(void)
{
}

void Audio_Unlock(void)
{
}

/* Hatari's own, without reopening a device. */
void Audio_SetOutputAudioFreq(int nNewFrequency)
{
	if (nNewFrequency != nAudioFrequency)
	{
		nAudioFrequency = nNewFrequency;

		if (Config_IsMachineFalcon())
		{
			/* Compute Ratio between host computer sound frequency and Hatari's sound frequency. */
			Crossbar_Compute_Ratio();
		}
		else if (!Config_IsMachineST())
		{
			/* Adapt LMC filters to this new frequency */
			DmaSnd_Init_Bass_and_Treble_Tables();
		}
	}

	/* Apply YM2149 C10 low pass filter ? (except if forced to NONE) */
	if ( YM2149_LPF_Filter != YM2149_LPF_FILTER_NONE )
	{
		if ( Config_IsMachineST() && nAudioFrequency >= 40000 )
			YM2149_LPF_Filter = YM2149_LPF_FILTER_LPF_STF;
		else
			YM2149_LPF_Filter = YM2149_LPF_FILTER_PWM;
	}
}

/* The host stops taking sound when it pauses. */
void Audio_EnableAudio(bool bEnable)
{
}

int KairoSt_SampleRate(void)
{
	return nAudioFrequency;
}

uint32_t KairoSt_ReadAudio(int16_t *stereo, uint32_t max_frames)
{
	uint32_t count = nGeneratedSamples < 0 ? 0 : (uint32_t)nGeneratedSamples;
	if (count > max_frames)
		count = max_frames;
	for (uint32_t i = 0; i < count; i++)
	{
		int index = (AudioMixBuffer_pos_read + (int)i) & AUDIOMIXBUFFER_SIZE_MASK;
		*stereo++ = AudioMixBuffer[index][0];
		*stereo++ = AudioMixBuffer[index][1];
	}
	AudioMixBuffer_pos_read = (AudioMixBuffer_pos_read + (int)count) & AUDIOMIXBUFFER_SIZE_MASK;
	nGeneratedSamples -= (int)count;
	return count;
}
