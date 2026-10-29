#include "st_machine.h"

#include <cstring>
#include <ctime>
#include <utility>

#include "hatari_library.h"
#include "kairo/emulator_turns.h"
#include "kairo/relative_warp.h"
#include "kairo/state_paths.h"

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <cstdio>
#endif

namespace kairost {

namespace {

// Hatari runs on its own thread (Hatari's) and the host on the caller's, taking turns
// (kairo/emulator_turns.h): the host lets Hatari run until its next VBL, and Hatari waits there
// until the host lets it run again.
struct Session {
    // This machine's own copy of Hatari, unloaded after its thread ends.
    std::unique_ptr<HatariLibrary> library;
    const KairoStApi *hatari = nullptr;
    KairoStHost host{};

    kairo::EmulatorTurns turns;
    // The error Hatari reported (host_fatal), on Hatari's thread before its turn ends.
    std::string fatal;

    std::vector<uint8_t> rgba;
    // picture(): frame() with its rows repeated.
    std::vector<uint8_t> picture;
    std::string floppy[kDrives];
    // The hard disk the machine started with, until its first frame shows Hatari took it.
    std::string hard_disk;
    // Direct touch (StMachine::mouse_warp).
    kairo::RelativeWarp warp;

    std::string error() const {
        return fatal.empty() ? std::string("The machine stopped") : fatal;
    }

    // Ends Hatari's thread: asks it to quit and lets it run to the end.
    void stop() {
        if (!turns.ended()) hatari->quit();
        turns.finish();
    }
};

// Hatari's thread's calls (KairoStHost), with the session as their context.
void host_vbl(void *context) { static_cast<Session *>(context)->turns.frame_end(); }

void host_fatal(void *context, const char *message) {
    auto *session = static_cast<Session *>(context);
    // The first error is the cause; later ones follow from it.
    if (session->fatal.empty()) session->fatal = message;
}

// Hatari's own log has it too (core/kairo/alert_hooks.c); this puts it where the platform's
// logs are read. Error level, as some devices keep no other.
// The frontend knows where each of the game's files is now while it loads a state (kairo/state_paths.h).
const char *host_state_path(void *, const char *recorded) { return kairo_state_path(recorded); }

void host_notice(void *, const char *message) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "KairoST", "Hatari: %s", message);
#else
    std::fprintf(stderr, "Hatari: %s\n", message);
#endif
}

} // namespace

struct StMachine::Impl : Session {};

StMachine::StMachine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

StMachine::~StMachine() { impl_->stop(); }

std::unique_ptr<StMachine> StMachine::create(const Config &config, std::string &error) {
    auto library = HatariLibrary::load(error);
    if (!library) return nullptr;
    // Hatari's command line: the whole configuration, with no configuration file. The display
    // is Hatari's own (with borders, for overscan), unscaled; the joystick in port 1 is Hatari's
    // keyboard joystick, which KairoStApi::joystick drives; nothing asks or waits for a person.
    std::vector<std::string> args = {
        "hatari",
        "--machine", config.machine,
        "--tos", config.tos,
        "--memsize", std::to_string(config.memory_kib),
        "--monitor", config.monitor,
        "--country", config.country,
        "--language", config.country,
        "--layout", config.country,
        "--sound", std::to_string(config.sample_rate),
        "--sound-sync", "off",
        "--joy0", "none",
        "--joy1", "keys",
        "--borders", "on",
        "--zoom", "1",
        "--statusbar", "off",
        "--drive-led", "off",
        "--frameskips", "0",
        "--fast-forward", "off",
        "--fast-boot", config.skip_memory_check ? "on" : "off",
        "--confirm-quit", "off",
        "--mousewarp", "off",
        "--natfeats", "off",
        "--log-file", config.home + "/hatari.log",
        "--log-level", "warn",
        "--alert-level", "error",
    };
    // Hatari runs each model at its own clock unless told another; it must come after --machine.
    if (config.cpu_clock) args.insert(args.end(), {"--cpuclock", std::to_string(config.cpu_clock)});
    if (!config.floppy[kDriveA].empty()) args.insert(args.end(), {"--disk-a", config.floppy[kDriveA]});
    if (!config.floppy[kDriveB].empty()) args.insert(args.end(), {"--disk-b", config.floppy[kDriveB]});
    // The hard disk on the model's own bus, as its unit 0.
    if (!config.hard_disk.empty()) {
        if (config.machine == "tt") args.insert(args.end(), {"--scsi", "0=" + config.hard_disk});
        else if (config.machine == "falcon") args.insert(args.end(), {"--ide-master", config.hard_disk});
        else args.insert(args.end(), {"--acsi", "0=" + config.hard_disk});
    }
    if (!config.floppy_program.empty()) {
        if (config.floppy[kDriveA].empty()) {
            error = "The startup program " + config.floppy_program + " needs a disk in drive A";
            return nullptr;
        }
        args.insert(args.end(), {"--auto", "A:\\" + config.floppy_program});
    }
    // Hatari's last argument: a program, whose folder it mounts as drive C and boots from, and
    // which it starts.
    if (!config.program.empty()) args.push_back(config.program);

    auto impl = std::make_unique<Impl>();
    for (int drive = 0; drive < kDrives; ++drive) impl->floppy[drive] = config.floppy[drive];
    impl->hard_disk = config.hard_disk;
    Session *state = impl.get();
    state->hatari = &library->api();
    state->library = std::move(library);
    state->host = {state, host_vbl, host_fatal, host_notice, host_state_path,
                   config.seed ? config.seed : static_cast<unsigned int>(std::time(nullptr))};
    // Hatari's first VBL: the machine is up.
    const bool up = state->turns.start([state, args = std::move(args), home = config.home, start = config.on_thread_start] {
        if (start) start();
        std::vector<const char *> argv;
        for (const auto &arg : args) argv.push_back(arg.c_str());
        state->hatari->run(int(argv.size()), argv.data(), home.c_str(), &state->host);
    });
    if (!up) {
        error = state->error();
        state->turns.finish();
        return nullptr;
    }
    return std::unique_ptr<StMachine>(new StMachine(std::move(impl)));
}

bool StMachine::run_frame(std::string &error) {
    if (impl_->warp.running()) {
        int dx, dy;
        impl_->warp.step(impl_->hatari->mouse_idle(), dx, dy);
        if (dx || dy) impl_->hatari->mouse_move(dx, dy);
    }
    if (!impl_->turns.run_frame()) {
        error = impl_->error();
        return false;
    }
    // Hatari stops using a hard disk it cannot open, and boots without it.
    if (!impl_->hard_disk.empty()) {
        if (impl_->hatari->hard_disk_path() != impl_->hard_disk) {
            error = "Hatari could not open the hard disk image " + impl_->hard_disk;
            return false;
        }
        impl_->hard_disk.clear();
    }
    return true;
}

bool StMachine::booting() const { return impl_->hatari->booting(); }
bool StMachine::loading() const { return impl_->hatari->loading(); }

int StMachine::locks() const { return impl_->hatari->caps_lock() ? 1 : 0; }

void StMachine::guest_input(unsigned long &console_reads, unsigned long &aes_calls) const {
    impl_->hatari->guest_input(&console_reads, &aes_calls);
}

double StMachine::frame_seconds() const { return impl_->hatari->frame_seconds(); }

const uint8_t *StMachine::frame(uint32_t &width, uint32_t &height) {
    int w, h, pitch;
    const uint8_t *pixels = reinterpret_cast<const uint8_t *>(impl_->hatari->screen(&w, &h, &pitch));
    width = uint32_t(w);
    height = uint32_t(h);
    // SDL's dummy window surface already holds R, G, B, A bytes (patch sdl-dummy-rgba): rows without
    // padding are the picture as it is; padded rows are packed.
    const size_t row = size_t(w) * 4;
    if (size_t(pitch) == row) return pixels;
    impl_->rgba.resize(row * size_t(h));
    for (int y = 0; y < h; ++y) std::memcpy(impl_->rgba.data() + row * size_t(y), pixels + size_t(pitch) * size_t(y), row);
    return impl_->rgba.data();
}

uint32_t StMachine::line_repeat() const { return uint32_t(impl_->hatari->line_repeat()); }

const uint8_t *StMachine::picture(uint32_t &width, uint32_t &height) {
    const uint8_t *rows = frame(width, height);
    const uint32_t repeat = line_repeat();
    if (repeat == 1) return rows;
    const size_t row = size_t(width) * 4;
    impl_->picture.resize(row * height * repeat);
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t r = 0; r < repeat; ++r)
            std::memcpy(impl_->picture.data() + row * (size_t(y) * repeat + r), rows + row * y, row);
    height *= repeat;
    return impl_->picture.data();
}

int StMachine::sample_rate() const { return impl_->hatari->sample_rate(); }

uint32_t StMachine::read_audio(int16_t *stereo, uint32_t max_frames) { return impl_->hatari->read_audio(stereo, max_frames); }

void StMachine::key(int scancode, bool down) { impl_->hatari->key(scancode, down); }

void StMachine::joystick(int control, bool down) {
    static_assert(kJoystickControls == KAIRO_ST_JOYSTICK_CONTROLS);
    if (control >= 0 && control < kJoystickControls) impl_->hatari->joystick(control, down);
}

void StMachine::mouse_move(int dx, int dy) {
    if (!impl_->warp.hold(dx, dy)) impl_->hatari->mouse_move(dx, dy);
}

void StMachine::mouse_warp(int x, int y) {
    int guest_x, guest_y, width, height;
    impl_->hatari->guest_point(x, y, &guest_x, &guest_y, &width, &height);
    impl_->warp.start(guest_x, guest_y, width, height);
}

bool StMachine::mouse_warp_done() const { return impl_->warp.done(); }

void StMachine::mouse_button(int button, bool down) {
    if (button == 1 || button == 2) impl_->hatari->mouse_button(button, down);
}

void StMachine::reset() { impl_->hatari->reset(false); }

bool StMachine::save_state(const std::string &file, std::string &error) {
    impl_->hatari->request_save(file.c_str());
    if (!run_frame(error)) return false;
    if (impl_->hatari->state_failed()) {
        error = "Hatari could not save the state to " + file;
        return false;
    }
    return true;
}

bool StMachine::load_state(const std::string &file, std::string &error) {
    impl_->hatari->request_restore(file.c_str());
    if (!run_frame(error)) return false;
    if (impl_->hatari->state_failed()) {
        error = "Hatari could not restore the state from " + file;
        return false;
    }
    for (int drive = 0; drive < kDrives; ++drive) impl_->floppy[drive] = impl_->hatari->floppy(drive);
    return true;
}

bool StMachine::insert(Drive drive, const std::string &path, std::string &error) {
    if (!impl_->hatari->insert_floppy(drive, path.c_str())) {
        error = "Hatari refused the disk image " + path;
        return false;
    }
    impl_->floppy[drive] = path;
    return true;
}

std::string StMachine::media_path(Drive drive) { return impl_->floppy[drive]; }

bool StMachine::insert_hard_disk(const std::string &path, std::string &error) {
    if (!impl_->hatari->hard_disk(path.c_str())) {
        error = "Hatari could not open the hard disk image " + path;
        return false;
    }
    return true;
}

std::string StMachine::hard_disk_path() { return impl_->hatari->hard_disk_path(); }

bool StMachine::guest_crashed() const { return impl_->hatari->guest_crashed(); }

int StMachine::on_desktop() const { return impl_->hatari->on_desktop(); }

void StMachine::cpu_state(uint32_t registers[16], uint32_t &pc, uint16_t &sr) const {
    impl_->hatari->cpu_state(registers, &pc, &sr);
}

std::vector<uint8_t> StMachine::read_memory(uint32_t address, uint32_t length) const {
    std::vector<uint8_t> bytes(length);
    impl_->hatari->read_memory(address, bytes.data(), length);
    return bytes;
}

} // namespace kairost
