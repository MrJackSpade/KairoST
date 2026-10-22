/*
 * KairoST: the seam between Hatari, which runs as a C program on its own thread, and the
 * frame-driven machine (st_machine.h) that hands control to it one video frame at a time.
 *
 * KairoSt_* are KairoST's replacements for Hatari's main.c and audio.c (kairo/main.c,
 * kairo/audio.c). KairoStHost_* are the machine's, called from Hatari's thread.
 *
 * Hatari keeps process-wide state: a process runs one machine at a time. Every KairoSt_* call
 * other than KairoSt_Run is made while Hatari's thread waits in KairoStHost_Vbl, so the two
 * threads never touch Hatari's state at once.
 *
 * KairoST is free software under the GNU General Public License, version 2 or (at your option)
 * any later version. See LICENSE.md.
 */

#ifndef KAIRO_ST_H
#define KAIRO_ST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runs a machine on the calling thread until it quits. argv holds Hatari command line options
 * (argv[0] is the program name), home a private folder for Hatari's own files. Returns true
 * when the machine quit as asked, false after an error KairoStHost_Fatal reported. */
bool KairoSt_Run(int argc, const char *const argv[], const char *home);
/* Asks the machine to stop: KairoSt_Run returns once Hatari's thread resumes. */
void KairoSt_Quit(void);

/* The last picture: XRGB8888 rows, pitch bytes apart. */
const uint32_t *KairoSt_Screen(int *width, int *height, int *pitch);
/* How long the frame that just ended lasts on the guest. */
double KairoSt_FrameSeconds(void);
/* The sound sample rate. */
int KairoSt_SampleRate(void);
/* Takes up to max_frames interleaved stereo frames produced so far. */
uint32_t KairoSt_ReadAudio(int16_t *stereo, uint32_t max_frames);

/* Whether the machine is booting: from the start until the first floppy load is over, or until
 * the player presses a key, joystick control or mouse button. The host runs it at full speed. */
bool KairoSt_Booting(void);

/* A key by its ST keyboard scan code (1 to 0x72). */
void KairoSt_Key(int scancode, bool down);
/* A control of the joystick in port 1, in KairoSt_JoystickControl order. */
enum KairoSt_JoystickControl { KAIRO_ST_UP, KAIRO_ST_DOWN, KAIRO_ST_LEFT, KAIRO_ST_RIGHT, KAIRO_ST_FIRE,
                               KAIRO_ST_JOYSTICK_CONTROLS };
void KairoSt_Joystick(int control, bool down);
/* The mouse in port 0: motion in ST pixels, and button 1 (left) or 2 (right). */
void KairoSt_MouseMove(int dx, int dy);
void KairoSt_MouseButton(int button, bool down);

/* Resets the machine: a cold reset clears memory, as switching the ST off and on does. */
void KairoSt_Reset(bool cold);
/* Asks Hatari to save or restore the whole machine at the end of the current CPU instruction,
 * the way its own menu does. The request is carried out when Hatari's thread resumes;
 * KairoSt_StateFailed then tells how it went. */
void KairoSt_RequestSave(const char *file);
void KairoSt_RequestRestore(const char *file);
bool KairoSt_StateFailed(void);
/* Inserts a floppy image in drive 0 (A) or 1 (B), or ejects it for ""; false when Hatari
 * refused it. */
bool KairoSt_InsertFloppy(int drive, const char *path);
/* The image in a drive, "" when it is empty. */
const char *KairoSt_Floppy(int drive);

/* The machine's, called on Hatari's thread. */
/* A frame ended: returns when the host wants the next one. */
void KairoStHost_Vbl(void);
/* Hatari hit an error it cannot continue past. KairoSt_Run returns false after it. */
void KairoStHost_Fatal(const char *message);
/* A message Hatari would show in a dialog box and carry on. */
void KairoStHost_Notice(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* KAIRO_ST_H */
