// An Atari ST on Hatari (third_party/hatari), run one video frame at a time by its host instead of
// Hatari's own real-time loop. Hatari runs on a thread of its own and hands control back at the
// end of each frame (core/kairo/kairo_st.h); between frames the host takes the picture and sound
// and sends input.
//
// Hatari keeps process-wide state, so each machine runs on a fresh copy of the Hatari library
// (hatari_library.h), which starts from Hatari's initial state as a new process would.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace kairost {

// The floppy drives.
enum Drive { kDriveA, kDriveB, kDrives };

// The joystick's controls: up, down, left, right, fire.
constexpr int kJoystickControls = 5;

// What a machine starts with.
struct Config {
    // A private folder for Hatari's own files (its log and NVRAM).
    std::string home;
    // The TOS image the machine boots.
    std::string tos;
    // The machine, by Hatari's name for it: st, megast, ste, megaste, tt or falcon.
    std::string machine = "st";
    // The CPU's clock in MHz, 8, 16 or 32 (Hatari's --cpuclock); 0 for the model's own.
    int cpu_clock = 0;
    // ST RAM in KiB: 512, 1024, 2048 or 4096 on an ST (Hatari's --memsize takes KiB above 14).
    int memory_kib = 1024;
    // The monitor, by Hatari's name for it: mono, rgb, vga or tv.
    std::string monitor = "rgb";
    // EmuTOS's country, by Hatari's code for it: its language, keyboard layout and, on an ST or
    // STE, its refresh rate (us is 60 Hz NTSC, every other country 50 Hz PAL). A TT or Falcon
    // takes it as its NVRAM language and layout. A player's own TOS keeps its own.
    std::string country = "uk";
    // The image in each drive; "" for an empty one.
    std::string floppy[kDrives];
    // A hard disk image on the machine's own bus (ACSI on an ST or STE, SCSI on a TT, IDE on a Falcon),
    // "" for none. The machine boots from it with drive A empty.
    std::string hard_disk;
    // A program to start, "" for none: its folder becomes drive C (Hatari's GEMDOS hard disk),
    // the machine boots from it, and TOS starts the program (Hatari's autostart).
    std::string program;
    // A program in the top folder of drive A's disk for TOS to start after it boots (Hatari's
    // autostart), such as "GAME.PRG", for a disk that boots to the desktop. "" for the automatic
    // choice: a disk that does not boot and holds exactly one program in its top folder starts it.
    std::string floppy_program;
    // The audio sample rate.
    int sample_rate = 48000;
    // Hatari's TOS patches for a faster boot: no memory test, and on TOS 1.00 to 2.05 no wait
    // for hard disks on the DMA bus (there are none). Games see the same machine.
    bool skip_memory_check = true;
    // The seed of Hatari's random numbers; 0 for the time. A measurement fixes it so runs repeat.
    unsigned int seed = 0;
    // Called on Hatari's thread before it starts, where the emulation runs: the app names the
    // thread and gives it the priority of emulation work (shared thread_priority.h), which belongs
    // to a thread, not to the threads that start it.
    std::function<void()> on_thread_start;
};

class StMachine {
public:
    // A machine running config's media; null with error set when it cannot start.
    static std::unique_ptr<StMachine> create(const Config &config, std::string &error);
    ~StMachine();

    // Whether the guest is booting (KairoSt_Booting), which the host runs at full speed.
    bool booting() const;
    // Whether the guest boots or loads from a floppy (KairoSt_Loading), which the host runs flat out.
    bool loading() const;
    // Runs one guest video frame; false with error set when the machine stops.
    bool run_frame(std::string &error);
    // The guest time the last frame lasted.
    double frame_seconds() const;
    // The latest picture: RGBA rows of width pixels.
    const uint8_t *frame(uint32_t &width, uint32_t &height);
    // How many picture rows each row of frame() stands for: 2 for an ST low or medium resolution
    // screen (each ST line is one row; the picture repeats it), 1 otherwise.
    uint32_t line_repeat() const;
    // frame() with each row repeated line_repeat() times: the picture as it is seen, for tests.
    const uint8_t *picture(uint32_t &width, uint32_t &height);
    int sample_rate() const;
    // Takes up to max_frames interleaved stereo frames produced so far.
    uint32_t read_audio(int16_t *stereo, uint32_t max_frames);

    // A key by its ST keyboard scan code.
    void key(int scancode, bool down);
    // A control of the joystick in port 1: kJoystickControls in their order.
    void joystick(int control, bool down);
    // The mouse in port 0: motion in ST pixels, and button 1 (left) or 2 (right).
    void mouse_move(int dx, int dy);
    void mouse_button(int button, bool down);

    // A warm reset, as the ST's reset button does.
    void reset();
    // Saves or loads the whole machine; false with error set when Hatari could not. Hatari does
    // either at the end of the current CPU instruction, so the call runs one more frame.
    bool save_state(const std::string &file, std::string &error);
    bool load_state(const std::string &file, std::string &error);

    // Inserts an image in a drive, or ejects it for ""; false with error set when Hatari refused.
    bool insert(Drive drive, const std::string &path, std::string &error);
    // The image in a drive, as it was inserted; "" when it is empty.
    std::string media_path(Drive drive);
    // Puts a hard disk image on the machine's bus, or takes it off for "", restarting the machine.
    bool insert_hard_disk(const std::string &path, std::string &error);
    // The hard disk image on the machine's bus, "" for none.
    std::string hard_disk_path();
    // Whether the guest crashed (a TOS bomb or EmuTOS panic, kairo_st.h).
    bool guest_crashed() const;
    // 1 when TOS runs its GEM desktop, 0 when not, -1 when the TOS cannot tell (kairo_st.h).
    int on_desktop() const;
    // The lock keys on: bit 0 Caps Lock, as TOS keeps it.
    int locks() const;
    // Direct touch: moves TOS's pointer to point (x, y) of the picture over the next frames, by a push
    // past the top-left corner and the counts from there (kairo::RelativeWarp).
    void mouse_warp(int x, int y);
    bool mouse_warp_done() const;
    // The keys the guest took from TOS's console and its AES calls (KairoSt_GuestInput).
    void guest_input(unsigned long &console_reads, unsigned long &aes_calls) const;
    // For host diagnostics: the CPU's D0-D7 and A0-A7, PC and SR, and the guest's memory as the
    // CPU sees it.
    void cpu_state(uint32_t registers[16], uint32_t &pc, uint16_t &sr) const;
    std::vector<uint8_t> read_memory(uint32_t address, uint32_t length) const;

private:
    struct Impl;
    explicit StMachine(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace kairost
