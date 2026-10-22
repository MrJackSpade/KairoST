/*
 * KairoST: Hatari's main.c, for a machine its host runs one video frame at a time.
 *
 * Hatari's own main() parses the command line, opens an SDL window and runs the emulation until
 * the program quits, pacing each video frame (VBL) against the host clock. KairoST builds this
 * file in its place: KairoSt_Run sets Hatari up from options the machine builds (st_machine.cpp),
 * on the thread the machine runs it on, and each VBL hands control back to the host, which paces
 * frames itself. Input comes from the host instead of SDL events. The screen is the window
 * surface of SDL's "dummy" video driver, which keeps the picture in memory for the host to take.
 * Fatal errors end the machine instead of the process.
 *
 * Based on Hatari's main.c. KairoST is free software under the GNU General Public License,
 * version 2 or (at your option) any later version. See LICENSE.md.
 */

#include <setjmp.h>
#include <time.h>
#include <SDL.h>

#include "main.h"
#include "kairo_st.h"
#include "configuration.h"
#include "options.h"
#include "audio.h"
#include "joy.h"
#include "file.h"
#include "floppy.h"
#include "floppy_ipf.h"
#include "floppy_stx.h"
#include "gemdos.h"
#include "mfp.h"
#include "fdc.h"
#include "hdc.h"
#include "ide.h"
#include "acia.h"
#include "ikbd.h"
#include "ioMem.h"
#include "keymap.h"
#include "log.h"
#include "m68000.h"
#include "memorySnapShot.h"
#include "midi.h"
#include "ncr5380.h"
#include "nvram.h"
#include "paths.h"
#include "printer.h"
#include "reset.h"
#include "resolution.h"
#include "rs232.h"
#include "rtc.h"
#include "scc.h"
#include "screen.h"
#include "sdlgui.h"
#include "sound.h"
#include "statusbar.h"
#include "stMemory.h"
#include "str.h"
#include "tos.h"
#include "video.h"
#include "debugui.h"
#include "clocks_timings.h"
#include "utils.h"

#include "hatari-glue.h"

#include "falcon/dsp.h"
#include "falcon/videl.h"

bool bQuitProgram = false;                /* Flag to quit program cleanly */

static bool bEmulationActive = true;      /* Hatari's pause state; the host pauses by not running frames */
static uint32_t nRunVBLs;                 /* Hatari's --run-vbls count (unused by KairoST, kept for its API) */
static int nVBLSlowdown = 1;              /* Hatari's --slowdown factor (likewise) */

/* Where Main_ErrorExit returns to: KairoSt_Run, on Hatari's thread. */
static jmp_buf fatal_exit;
static bool fatal_exit_set;
/* A Main_ErrorExit left Hatari's state half torn down: no machine can run in this process again. */
static bool hatari_broken;

/* The joystick in port 1 is Hatari's keyboard-emulated joystick (--joy1 keys), with its default
 * keys: KairoSt_Joystick presses them through Hatari's own keyboard joystick handler. */
static const int joystick_keys[KAIRO_ST_JOYSTICK_CONTROLS] = { SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_RCTRL };

/*-----------------------------------------------------------------------*/
/* Hatari's main.h API. */

bool Main_PauseEmulation(bool visualize)
{
	if (!bEmulationActive)
		return false;
	Audio_EnableAudio(false);
	bEmulationActive = false;
	return true;
}

bool Main_UnPauseEmulation(void)
{
	if (bEmulationActive)
		return false;
	Sound_BufferIndexNeedReset = true;
	Audio_EnableAudio(ConfigureParams.Sound.bEnableSound);
	bEmulationActive = true;
	Screen_SetFullUpdate();
	return true;
}

/* Hatari quits without asking: there is no one to answer a dialog. */
void Main_RequestQuit(int exitval)
{
	Main_SetQuitValue(exitval);
}

void Main_SetQuitValue(int exitval)
{
	bQuitProgram = true;
	M68000_SetSpecial(SPCFLAG_BRK);
}

uint32_t Main_SetRunVBLs(uint32_t vbls)
{
	if (!vbls)
		return nRunVBLs;
	nRunVBLs = vbls;
	return 0;
}

const char* Main_SetVBLSlowdown(int factor)
{
	if (factor < 1 || factor > 30)
		return "invalid VBL slowdown factor, should be 1-30";
	nVBLSlowdown = factor;
	return NULL;
}

/* The end of each emulated VBL: the frame is drawn, so the host takes it and decides when the
 * next one starts. A machine that is quitting runs on to the end without waiting. */
void Main_WaitOnVbl(void)
{
	if (bQuitProgram)
		return;
	KairoStHost_Vbl();
}

/* The host owns the pointer: there is no host cursor to warp, show or hide. */
void Main_WarpMouse(int x, int y, bool restore)
{
}

bool Main_ShowCursor(bool show)
{
	return false;
}

/* Hatari polls for SDL events here; KairoST's input arrives between frames instead
 * (KairoSt_Key and the others), so there is nothing to poll. */
void Main_EventHandler(void)
{
}

void Main_SetTitle(const char *title)
{
}

/* Hatari exits the process here; KairoST ends the machine and reports the error. */
void Main_ErrorExit(const char *msg1, const char *msg2, int errval)
{
	char message[512];

	if (msg1 && msg2)
		snprintf(message, sizeof(message), "%s %s", msg1, msg2);
	else if (msg1)
		snprintf(message, sizeof(message), "%s", msg1);
	else
		snprintf(message, sizeof(message), "Hatari stopped with error %d", errval);
	Log_Printf(LOG_FATAL, "%s\n", message);
	KairoStHost_Fatal(message);
	if (!fatal_exit_set)
		abort();  /* Hatari only runs inside KairoSt_Run */
	longjmp(fatal_exit, 1);
}

/*-----------------------------------------------------------------------*/
/* Setting up and tearing down, as Hatari's main() does. */

static void Main_Init_HW(void)
{
	Joy_Init();
	FDC_Init();
	STX_Init();
	Video_InitTimings();
}

/* Hatari's Main_Init, without its window title and options dialog: a TOS image that does not
 * load is an error. */
static bool Main_Init(void)
{
	if (!Log_Init())
		Main_ErrorExit("Logging/tracing initialization failed", NULL, -1);

	/* KairoSt_Run is the entry point, not SDL's main(); Android's SDL requires saying so. */
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		Main_ErrorExit("Could not initialize the SDL library:", SDL_GetError(), -1);

	if (!IPF_Init())
		Main_ErrorExit("Could not initialize the IPF support", NULL, -1);

	ClocksTimings_InitMachine(ConfigureParams.System.nMachineType);
	Video_SetTimings(ConfigureParams.System.nMachineType, ConfigureParams.System.VideoTimingMode);

	Resolution_Init();
	SDLGui_Init();
	Printer_Init();
	MFP_Init(MFP_Array);
	RS232_Init();
	SCC_Init();
	Midi_Init();
	Videl_Init();
	Screen_Init();

	STMemory_Init(ConfigureParams.Memory.STRamSize_KB * 1024);

	ACIA_Init(ACIA_Array, MachineClocks.ACIA_Freq, MachineClocks.ACIA_Freq);
	IKBD_Init();			/* After ACIA_Init */

	DSP_Init();
	Floppy_Init();
	M68000_Init();
	Audio_Init();
	Keymap_Init();

	HDC_Init();
	Ncr5380_Init();
	Ide_Init();
	GemDOS_Init();
	if (ConfigureParams.HardDisk.bUseHardDiskDirectories)
		GemDOS_InitDrives();

	if (Reset_Cold() || !bTosImageLoaded)
	{
		KairoStHost_Fatal("The TOS image did not load");
		return false;
	}

	IoMem_Init();
	NvRam_Init();
	Sound_Init();
	Rtc_Init();

	DebugUI_Init();
	return true;
}

static void Main_UnInit(void)
{
	Floppy_UnInit();
	HDC_UnInit();
	Ncr5380_UnInit();
	Midi_UnInit();
	SCC_UnInit();
	RS232_UnInit();
	Printer_UnInit();
	IoMem_UnInit(ConfigureParams.System.nMachineType);
	NvRam_UnInit();
	GemDOS_UnInitDrives();
	Ide_UnInit();
	Joy_UnInit();
	Audio_UnInit();
	SDLGui_UnInit();
	DSP_UnInit();
	Screen_UnInit();
	Exit680x0();

	IPF_Exit();

	SDL_Quit();

	DebugUI_UnInit();
	Log_UnInit();

	Paths_UnInit();
}

static void set_env(const char *name, const char *value)
{
#ifdef WIN32
	_putenv_s(name, value);
#else
	setenv(name, value, 1);
#endif
}

bool KairoSt_Run(int argc, const char *const argv[], const char *home)
{
	bool ok;

	if (hatari_broken)
	{
		KairoStHost_Fatal("An earlier machine failed while starting; restart the app to run another");
		return false;
	}
	if (setjmp(fatal_exit))
	{
		/* Main_ErrorExit: Hatari gave up part way, as when it would have exited the process. */
		fatal_exit_set = false;
		hatari_broken = true;
		return false;
	}
	fatal_exit_set = true;

	bQuitProgram = false;
	bEmulationActive = true;

	Hatari_srand(time(NULL));
	Str_Init();
	Log_Default();
	/* Hatari keeps its own files (NVRAM, logs) in $HOME/.config/hatari. */
	set_env("HOME", home);
	/* Hatari's headless mode: SDL's dummy video driver, whose window surface is an in-memory
	 * picture (screen.c), drawn without an SDL renderer. */
	set_env("SDL_VIDEODRIVER", "dummy");
	Paths_Init(argv[0]);
	Main_Init_HW();
	Configuration_SetDefault();
	/* KairoST's options are the whole configuration: no configuration file is read. */
	if (!Opt_ParseParameters(argc, argv))
	{
		char message[1024];
		size_t used = (size_t)snprintf(message, sizeof(message), "Hatari rejected the options:");
		for (int i = 1; i < argc && used < sizeof(message); i++)
			used += (size_t)snprintf(message + used, sizeof(message) - used, " %s", argv[i]);
		KairoStHost_Fatal(message);
		Paths_UnInit();
		fatal_exit_set = false;
		return false;
	}
	Configuration_Apply(true);

	ok = Main_Init();
	if (ok)
	{
		Main_UnPauseEmulation();
		M68000_Start();		/* Runs until the machine quits */
	}
	Main_UnInit();
	fatal_exit_set = false;
	return ok;
}

/*-----------------------------------------------------------------------*/
/* The host's side (kairo_st.h), called while Hatari's thread waits in Main_WaitOnVbl. */

void KairoSt_Quit(void)
{
	Main_SetQuitValue(0);
}

const uint32_t *KairoSt_Screen(int *width, int *height, int *pitch)
{
	*width = sdlscrn->w;
	*height = sdlscrn->h;
	*pitch = sdlscrn->pitch;
	return (const uint32_t *)sdlscrn->pixels;
}

double KairoSt_FrameSeconds(void)
{
	return ClocksTimings_GetVBLDuration_micro(ConfigureParams.System.nMachineType, nScreenRefreshRate)
		* (double)nVBLSlowdown / 1e6;
}

void KairoSt_Key(int scancode, bool down)
{
	IKBD_PressSTKey((uint8_t)scancode, down);
}

void KairoSt_Joystick(int control, bool down)
{
	if (down)
		Joy_KeyDown(joystick_keys[control], 0);
	else
		Joy_KeyUp(joystick_keys[control], 0);
}

/* Hatari's mouse handling, without its window scaling: the host sends ST pixels. Motion right
 * after a reset is dropped, as Hatari drops it, or TOS may take it for key clicks. */
void KairoSt_MouseMove(int dx, int dy)
{
	if (nVBLs < 10)
		return;
	KeyboardProcessor.Mouse.dx += dx;
	KeyboardProcessor.Mouse.dy += dy;
}

void KairoSt_MouseButton(int button, bool down)
{
	int *state = button == 1 ? &Keyboard.bLButtonDown : &Keyboard.bRButtonDown;
	if (down)
		*state |= BUTTON_MOUSE;
	else
		*state &= ~BUTTON_MOUSE;
}

void KairoSt_Reset(bool cold)
{
	if (cold)
		Reset_Cold();
	else
		Reset_Warm();
}

void KairoSt_RequestSave(const char *file)
{
	MemorySnapShot_Capture(file, false);
}

void KairoSt_RequestRestore(const char *file)
{
	MemorySnapShot_Restore(file, false);
}

bool KairoSt_StateFailed(void)
{
	return MemorySnapShot_Failed();
}

bool KairoSt_InsertFloppy(int drive, const char *path)
{
	if (!*path)
	{
		Floppy_SetDiskFileNameNone(drive);
		Floppy_EjectDiskFromDrive(drive);
		return true;
	}
	if (!Floppy_SetDiskFileName(drive, path, NULL))
		return false;
	return Floppy_InsertDiskIntoDrive(drive);
}

const char *KairoSt_Floppy(int drive)
{
	return ConfigureParams.DiskImage.szDiskFileName[drive];
}
