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
 * A copy of this library runs one machine (kairo_st.h): Hatari's variables start from their
 * initial values only once, so the host loads a fresh copy for each machine.
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
#include "inffile.h"

#include "falcon/dsp.h"
#include "falcon/videl.h"

bool bQuitProgram = false;                /* Flag to quit program cleanly */

static bool bEmulationActive = true;      /* Hatari's pause state; the host pauses by not running frames */
static uint32_t nRunVBLs;                 /* Hatari's --run-vbls count (unused by KairoST, kept for its API) */
static int nVBLSlowdown = 1;              /* Hatari's --slowdown factor (likewise) */

/* The machine's side, for this copy's one run. */
static const struct KairoStHost *host;
/* The host named the program to start (Hatari's --auto): no program is picked from drive A. */
static bool autostart_given;

/* Where Main_ErrorExit returns to: KairoSt_Run, on Hatari's thread. */
static jmp_buf fatal_exit;
static bool fatal_exit_set;

/* The boot (KairoSt_Booting), which the host runs at full speed, from the start until the player
 * presses something or: with a disk in drive A, until the game's first load is over: the drive's
 * motor has run and stopped, and the screen is no longer the one TOS showed when the load began
 * (a loader that pauses between reads, to unpack what it read, stops the motor while TOS's boot
 * screen still shows: KairoST#5); for a program, until TOS has read the autostart file that starts
 * it (it then loads from drive C at once). */
static enum { BOOT_OVER, BOOT_WAITING, BOOT_LOADING } boot;
static bool boot_program;
/* The screen when the first load began, while it loads. */
static uint8_t *boot_screen;
static size_t boot_screen_size;

static size_t screen_size(void)
{
	return (size_t)sdlscrn->h * (size_t)sdlscrn->pitch;
}

static void end_boot(void)
{
	boot = BOOT_OVER;
	free(boot_screen);
	boot_screen = NULL;
}

static void update_boot(void)
{
	if (boot == BOOT_OVER)
		return;
	if (boot_program)
	{
		/* The virtual DESKTOP.INF is open from TOS's boot until TOS has read it. */
		if (INF_Overriding(AUTOSTART_FOPEN))
			boot = BOOT_LOADING;
		else if (boot == BOOT_LOADING)
			end_boot();
		return;
	}
	if (FDC_MotorIsOn())
	{
		if (boot == BOOT_WAITING)
		{
			boot_screen_size = screen_size();
			boot_screen = malloc(boot_screen_size);
			if (!boot_screen)
				Main_ErrorExit("Out of memory for the boot screen", NULL, -1);
			memcpy(boot_screen, sdlscrn->pixels, boot_screen_size);
		}
		boot = BOOT_LOADING;
	}
	else if (boot == BOOT_LOADING
	         && (screen_size() != boot_screen_size || memcmp(boot_screen, sdlscrn->pixels, boot_screen_size) != 0))
		end_boot();
}

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
	update_boot();
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

/* A disk that does not boot is opened from the desktop, by its program: when drive A's disk does
 * not boot and its top folder has exactly one program, it starts the way the player would start
 * it, through Hatari's autostart (--auto). Disks with several programs, or none, open on the
 * desktop. Only images Hatari keeps as plain sectors (ST, MSA, DIM) are read; STX images are
 * originals, which boot themselves. */
static void autostart_floppy_program(void)
{
	static const char *const programs[] = { "PRG", "TOS", "APP", "TTP", "GTP" };
	const EMULATION_DRIVE *drive = &EmulationDrives[0];
	const uint8_t *disk = drive->pBuffer;
	int size = drive->nImageBytes;
	int sector, reserved, fats, entries, fat_sectors, root, i, found = 0;
	uint16_t checksum = 0;
	char name[16] = "";

	if (!drive->bDiskInserted || !disk || size < 512
	    || !(drive->ImageType == FLOPPY_IMAGE_TYPE_ST || drive->ImageType == FLOPPY_IMAGE_TYPE_MSA
	         || drive->ImageType == FLOPPY_IMAGE_TYPE_DIM))
		return;
	/* An executable boot sector's big-endian words add up to 0x1234. */
	for (i = 0; i < 512; i += 2)
		checksum += (uint16_t)(disk[i] << 8 | disk[i + 1]);
	if (checksum == 0x1234)
		return;

	sector = disk[11] | disk[12] << 8;
	reserved = disk[14] | disk[15] << 8;
	fats = disk[16];
	entries = disk[17] | disk[18] << 8;
	fat_sectors = disk[22] | disk[23] << 8;
	if (sector != 512 || reserved < 1 || fats < 1 || entries < 1)
		return;		/* not a TOS file system */
	root = (reserved + fats * fat_sectors) * sector;
	for (i = 0; i < entries && root + i * 32 + 32 <= size; i++)
	{
		const uint8_t *entry = disk + root + i * 32;
		char base[9], ext[4];
		int b, e, p;

		if (entry[0] == 0)
			break;
		if (entry[0] == 0xE5 || entry[11] & 0x18)	/* deleted, volume label or folder */
			continue;
		memcpy(base, entry, 8); base[8] = '\0';
		memcpy(ext, entry + 8, 3); ext[3] = '\0';
		for (b = 7; b >= 0 && base[b] == ' '; b--) base[b] = '\0';
		for (e = 2; e >= 0 && ext[e] == ' '; e--) ext[e] = '\0';
		for (p = 0; p < ARRAY_SIZE(programs); p++)
		{
			if (strcmp(ext, programs[p]) == 0)
			{
				if (++found == 1)
					snprintf(name, sizeof(name), "A:\\%s.%s", base, ext);
			}
		}
	}
	if (found == 1)
	{
		Log_Printf(LOG_INFO, "Drive A's disk does not boot; starting its program %s\n", name);
		INF_SetAutoStart(name, 0);
	}
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
	/* The host repeats a doubled screen's lines itself (MachineCore::line_repeat). */
	bScreenSingleLines = true;
	Screen_Init();

	STMemory_Init(ConfigureParams.Memory.STRamSize_KB * 1024);

	ACIA_Init(ACIA_Array, MachineClocks.ACIA_Freq, MachineClocks.ACIA_Freq);
	IKBD_Init();			/* After ACIA_Init */

	DSP_Init();
	Floppy_Init();
	/* A program's own game (KairoSt_Run's program argument) starts from drive C instead, and a
	 * program the host names (--auto) is started whatever the disk holds. */
	if (!ConfigureParams.HardDisk.bBootFromHardDisk && !autostart_given)
		autostart_floppy_program();
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

void KairoStHost_Vbl(void)
{
	host->vbl(host->context);
}

void KairoStHost_Fatal(const char *message)
{
	host->fatal(host->context, message);
}

void KairoStHost_Notice(const char *message)
{
	host->notice(host->context, message);
}

static bool KairoSt_Run(int argc, const char *const argv[], const char *home, const struct KairoStHost *machine)
{
	bool ok;

	if (host)
		abort();	/* a copy of the library runs one machine */
	host = machine;
	if (setjmp(fatal_exit))
	{
		/* Main_ErrorExit: Hatari gave up part way, as when it would have exited the process. The
		 * host discards this copy of the library with whatever Hatari left behind. */
		fatal_exit_set = false;
		return false;
	}
	fatal_exit_set = true;

	bQuitProgram = false;
	bEmulationActive = true;

	Hatari_srand(host->seed);
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
	for (int i = 1; i < argc; i++)
		if (strcmp(argv[i], "--auto") == 0)
			autostart_given = true;
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
	boot_program = ConfigureParams.HardDisk.bBootFromHardDisk;
	boot = boot_program || ConfigureParams.DiskImage.szDiskFileName[0][0] ? BOOT_WAITING : BOOT_OVER;
	/* Hatari autostarts only on TOS 1.04 and later (inffile.c); before, a program waits on the
	 * desktop, so there is no boot to run through. */
	if (boot_program && bUseTos && TosVersion < 0x0104)
		end_boot();
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

static void KairoSt_Quit(void)
{
	Main_SetQuitValue(0);
}

static const uint32_t *KairoSt_Screen(int *width, int *height, int *pitch)
{
	*width = sdlscrn->w;
	*height = sdlscrn->h;
	*pitch = sdlscrn->pitch;
	return (const uint32_t *)sdlscrn->pixels;
}

static double KairoSt_FrameSeconds(void)
{
	return ClocksTimings_GetVBLDuration_micro(ConfigureParams.System.nMachineType, nScreenRefreshRate)
		* (double)nVBLSlowdown / 1e6;
}

static bool KairoSt_Booting(void)
{
	return boot != BOOT_OVER;
}

static void KairoSt_Key(int scancode, bool down)
{
	if (down)
		end_boot();
	IKBD_PressSTKey((uint8_t)scancode, down);
}

static void KairoSt_Joystick(int control, bool down)
{
	if (down)
	{
		end_boot();
		Joy_KeyDown(joystick_keys[control], 0);
	}
	else
		Joy_KeyUp(joystick_keys[control], 0);
}

/* Hatari's mouse handling, without its window scaling: the host sends ST pixels. Motion right
 * after a reset is dropped, as Hatari drops it, or TOS may take it for key clicks. */
static void KairoSt_MouseMove(int dx, int dy)
{
	if (nVBLs < 10)
		return;
	KeyboardProcessor.Mouse.dx += dx;
	KeyboardProcessor.Mouse.dy += dy;
}

static void KairoSt_MouseButton(int button, bool down)
{
	int *state = button == 1 ? &Keyboard.bLButtonDown : &Keyboard.bRButtonDown;
	if (down)
	{
		end_boot();
		*state |= BUTTON_MOUSE;
	}
	else
		*state &= ~BUTTON_MOUSE;
}

static void KairoSt_Reset(bool cold)
{
	if (cold)
		Reset_Cold();
	else
		Reset_Warm();
}

static void KairoSt_RequestSave(const char *file)
{
	MemorySnapShot_Capture(file, false);
}

static void KairoSt_RequestRestore(const char *file)
{
	end_boot();
	MemorySnapShot_Restore(file, false);
}

static bool KairoSt_StateFailed(void)
{
	return MemorySnapShot_Failed();
}

static bool KairoSt_InsertFloppy(int drive, const char *path)
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

static const char *KairoSt_Floppy(int drive)
{
	return ConfigureParams.DiskImage.szDiskFileName[drive];
}

static int KairoSt_LineRepeat(void)
{
	return bScreenLinesHalved ? 2 : 1;
}

static bool KairoSt_GuestCrashed(void)
{
	return STMemory_ReadLong(0x380) == 0x12345678;
}

/* The process TOS runs is its desktop: the GEMDOS process TOS runs (the run variable, which the OS
 * header names at offset 0x28 from TOS 1.02 on, in EmuTOS too) has its code in the ROM, as the
 * AES and its desktop do; a game's program has its code in RAM, and a game a boot sector starts
 * never reaches the desktop. -1 for TOS 1.00, which does not name the variable. */
static int KairoSt_OnDesktop(void)
{
	uint32_t run, basepage, text;

	if (!bIsEmuTOS && TosVersion < 0x0102)
		return -1;
	run = STMemory_ReadLong(TosAddress + 0x28);
	basepage = run ? STMemory_ReadLong(run) : 0;
	text = basepage ? STMemory_ReadLong(basepage + 8) : 0;
	return text >= TosAddress && text < TosAddress + TosSize;
}

static void KairoSt_CpuState(uint32_t registers[16], uint32_t *pc, uint16_t *sr)
{
	for (int i = 0; i < 16; i++)
		registers[i] = Regs[i];
	*pc = M68000_GetPC();
	*sr = M68000_GetSR();
}

static void KairoSt_ReadMemory(uint32_t address, uint8_t *out, uint32_t length)
{
	for (uint32_t i = 0; i < length; i++)
		out[i] = STMemory_ReadByte(address + i);
}

const struct KairoStApi *KairoSt_Api(void)
{
	static const struct KairoStApi api = {
		.run = KairoSt_Run,
		.quit = KairoSt_Quit,
		.screen = KairoSt_Screen,
		.line_repeat = KairoSt_LineRepeat,
		.frame_seconds = KairoSt_FrameSeconds,
		.sample_rate = KairoSt_SampleRate,
		.read_audio = KairoSt_ReadAudio,
		.booting = KairoSt_Booting,
		.key = KairoSt_Key,
		.joystick = KairoSt_Joystick,
		.mouse_move = KairoSt_MouseMove,
		.mouse_button = KairoSt_MouseButton,
		.reset = KairoSt_Reset,
		.request_save = KairoSt_RequestSave,
		.request_restore = KairoSt_RequestRestore,
		.state_failed = KairoSt_StateFailed,
		.insert_floppy = KairoSt_InsertFloppy,
		.floppy = KairoSt_Floppy,
		.guest_crashed = KairoSt_GuestCrashed,
		.on_desktop = KairoSt_OnDesktop,
		.cpu_state = KairoSt_CpuState,
		.read_memory = KairoSt_ReadMemory,
	};
	return &api;
}
