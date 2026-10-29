/*
 * KairoST: the seam between Hatari, which runs as a C program on its own thread, and the
 * frame-driven machine (st_machine.h) that hands control to it one video frame at a time.
 *
 * Hatari keeps its whole state in process-wide variables and was written to run once per
 * process. So that every machine starts from Hatari's initial state, as a new Hatari process
 * would, Hatari is built as a library of its own (kairost_hatari) and the machine loads a fresh
 * copy of it for each run (hatari_library.h), discarding it afterwards: nothing one machine leaves
 * behind (its configuration, a CPU halted part way through an exception, a sound filter) can
 * reach the next. The library's only entry point is KairoSt_Api, which returns its functions.
 *
 * KairoSt_* are KairoST's replacements for Hatari's main.c and audio.c (kairo/main.c,
 * kairo/audio.c). KairoStHost is the machine's side, which Hatari's thread calls.
 *
 * Every function other than run is called while Hatari's thread waits in the host's vbl, so the
 * two threads never touch Hatari's state at once.
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

/* A control of the joystick in port 1, in this order. */
enum KairoSt_JoystickControl { KAIRO_ST_UP, KAIRO_ST_DOWN, KAIRO_ST_LEFT, KAIRO_ST_RIGHT, KAIRO_ST_FIRE,
                               KAIRO_ST_JOYSTICK_CONTROLS };

/* The machine's side, called on Hatari's thread with its context. */
struct KairoStHost {
	void *context;
	/* A frame ended: returns when the host wants the next one. */
	void (*vbl)(void *context);
	/* Hatari hit an error it cannot continue past. run returns false after it. */
	void (*fatal)(void *context, const char *message);
	/* A message Hatari would show in a dialog box and carry on. */
	void (*notice)(void *context, const char *message);
	/* Where a file a state being restored names is now: the host's working copy of a game can have
	 * another name since the state was saved. Returns recorded when it has not moved. */
	const char *(*state_path)(void *context, const char *recorded);
	/* The seed of Hatari's random numbers (the floppy's index pulse, keyboard timings): the app
	 * gives the time, a measurement a fixed seed so that two runs are the same. */
	unsigned int seed;
};

struct KairoStApi {
	/* Runs a machine on the calling thread until it quits. argv holds Hatari command line options
	 * (argv[0] is the program name), home a private folder for Hatari's own files. Returns true
	 * when the machine quit as asked, false after an error host->fatal reported. A library copy
	 * runs one machine: the host loads a fresh copy for the next. */
	bool (*run)(int argc, const char *const argv[], const char *home, const struct KairoStHost *host);
	/* Asks the machine to stop: run returns once Hatari's thread resumes. */
	void (*quit)(void);

	/* The last picture: rows of R, G, B, A bytes (SDL's ABGR8888), pitch bytes apart. */
	const uint32_t *(*screen)(int *width, int *height, int *pitch);
	/* How many picture lines each line of the screen stands for: 2 for an ST low or medium
	 * resolution screen, kept at one line per ST line (patch hatari-single-lines), 1 otherwise. */
	int (*line_repeat)(void);
	/* How long the frame that just ended lasts on the guest. */
	double (*frame_seconds)(void);
	/* The sound sample rate. */
	int (*sample_rate)(void);
	/* Takes up to max_frames interleaved stereo frames produced so far. */
	uint32_t (*read_audio)(int16_t *stereo, uint32_t max_frames);

	/* Whether the machine is booting: from the start until the first floppy load is over, or
	 * until the player presses a key, joystick control or mouse button. The host runs it at full
	 * speed. */
	bool (*booting)(void);
	/* Whether the machine boots or loads from a floppy (its motor runs), which the host runs at full
	 * speed (Kairo#82). */
	bool (*loading)(void);

	/* A key by its ST keyboard scan code (1 to 0x72). */
	void (*key)(int scancode, bool down);
	/* A control of the joystick in port 1 (KairoSt_JoystickControl). */
	void (*joystick)(int control, bool down);
	/* The mouse in port 0: motion in ST pixels, and button 1 (left) or 2 (right). */
	void (*mouse_move)(int dx, int dy);
	void (*mouse_button)(int button, bool down);
	/* A point of the picture (screen) in the ST screen's own pixels, inside its borders, and that
	 * screen's size: 320 by 200, 640 by 200 or 640 by 400. */
	void (*guest_point)(int x, int y, int *guest_x, int *guest_y, int *width, int *height);
	/* Whether the mouse motion given so far has all gone to the ST: none waits to be sent, and
	 * the keyboard processor has passed on every byte. */
	bool (*mouse_idle)(void);

	/* Resets the machine: a cold reset clears memory, as switching the ST off and on does. */
	void (*reset)(bool cold);
	/* Asks Hatari to save or restore the whole machine at the end of the current CPU
	 * instruction, the way its own menu does. The request is carried out when Hatari's thread
	 * resumes; state_failed then tells how it went. */
	void (*request_save)(const char *file);
	void (*request_restore)(const char *file);
	bool (*state_failed)(void);
	/* Inserts a floppy image in drive 0 (A) or 1 (B), or ejects it for ""; false when Hatari
	 * refused it. */
	bool (*insert_floppy)(int drive, const char *path);
	/* Puts a hard disk image on the machine's own bus (ACSI on an ST or STE, SCSI on a TT, IDE on a
	 * Falcon), or takes it off for "", and restarts the machine, as Hatari's own dialog changes one;
	 * false when Hatari could not open the image. */
	bool (*hard_disk)(const char *path);
	/* The hard disk image on the machine's bus, "" for none. */
	const char *(*hard_disk_path)(void);
	/* The image in a drive, "" when it is empty. */
	const char *(*floppy)(int drive);
	/* Whether the guest crashed since the machine started: TOS (bombs) and EmuTOS (its panic
	 * screen) both record a CPU exception that ended a program with the value 0x12345678 at 0x380
	 * (proc_lives). Both then end the program and carry on (EmuTOS after a key press), so a disk
	 * whose boot sector crashes can still start its game: this is a signal for tests, not an end. */
	bool (*guest_crashed)(void);
	/* 1 when TOS runs its GEM desktop (no game program runs), 0 when not, -1 when the TOS cannot
	 * tell (TOS 1.00). */
	int (*on_desktop)(void);
	/* Whether Caps Lock is on, as TOS keeps it: the ST keyboard has no lamp, TOS its kbshift. */
	bool (*caps_lock)(void);
	/* How the guest reads its input, for Auto touch mode: the keys it took from TOS's console
	 * (Bconin(2)), and its calls to the AES, which only a GEM program makes. */
	void (*guest_input)(unsigned long *console_reads, unsigned long *aes_calls);
	/* For host diagnostics: the CPU's registers (D0-D7, A0-A7), PC and SR, and a copy of the
	 * guest's memory as the CPU sees it (0 where nothing is mapped). */
	void (*cpu_state)(uint32_t registers[16], uint32_t *pc, uint16_t *sr);
	void (*read_memory)(uint32_t address, uint8_t *out, uint32_t length);
};

#if defined(_WIN32)
#define KAIRO_ST_EXPORT __declspec(dllexport)
#else
#define KAIRO_ST_EXPORT __attribute__((visibility("default")))
#endif

/* The library's one exported symbol. */
#define KAIRO_ST_API_SYMBOL "KairoSt_Api"
KAIRO_ST_EXPORT const struct KairoStApi *KairoSt_Api(void);

/* Inside the library: the machine's side, through the host run was given. */
void KairoStHost_Vbl(void);
void KairoStHost_Fatal(const char *message);
void KairoStHost_Notice(const char *message);
/* Inside the library: kairo/audio.c's half of the API. */
int KairoSt_SampleRate(void);
uint32_t KairoSt_ReadAudio(int16_t *stereo, uint32_t max_frames);

#ifdef __cplusplus
}
#endif

#endif /* KAIRO_ST_H */
