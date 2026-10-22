#include "st_machine.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

#include "kairo_st.h"

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <cstdio>
#endif

namespace kairost {

namespace {

// Hatari runs on its own thread (Hatari's) and the host on the caller's. Exactly one of them
// runs at a time: the host lets Hatari run until its next VBL, and Hatari waits there until the
// host lets it run again. The mutex hand-off orders every access to Hatari's state.
struct Session {
    enum class Turn { kHatari, kHost };

    std::mutex mutex;
    std::condition_variable changed;
    Turn turn = Turn::kHatari;
    // Hatari's thread has returned from KairoSt_Run.
    bool ended = false;
    // KairoSt_Run's result, once ended.
    bool quit_cleanly = false;
    // The error Hatari reported (KairoStHost_Fatal), on Hatari's thread before its turn ends.
    std::string fatal;
    std::thread thread;

    std::vector<uint8_t> rgba;
    std::string floppy[kDrives];

    // Lets Hatari run until its next VBL or until it ends; false when it ended.
    bool run_hatari() {
        std::unique_lock lock(mutex);
        turn = Turn::kHatari;
        changed.notify_all();
        changed.wait(lock, [&] { return turn == Turn::kHost || ended; });
        return !ended;
    }

    // On Hatari's thread, at its VBL.
    void at_vbl() {
        std::unique_lock lock(mutex);
        turn = Turn::kHost;
        changed.notify_all();
        changed.wait(lock, [&] { return turn == Turn::kHatari; });
    }

    std::string error() const {
        return fatal.empty() ? std::string("The machine stopped") : fatal;
    }

    // Ends Hatari's thread: asks it to quit and lets it run to the end.
    void stop() {
        if (!thread.joinable()) return;
        if (!ended) {
            KairoSt_Quit();
            run_hatari();
        }
        thread.join();
    }
};

// The running machine, for the KairoStHost_* calls from Hatari's thread. A process runs one.
Session *running = nullptr;

} // namespace

struct StMachine::Impl : Session {};

StMachine::StMachine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

StMachine::~StMachine() {
    impl_->stop();
    running = nullptr;
}

std::unique_ptr<StMachine> StMachine::create(const Config &config, std::string &error) {
    if (running) {
        error = "Another machine is still running";
        return nullptr;
    }
    // Hatari's command line: the whole configuration, with no configuration file. The display
    // is Hatari's own (with borders, for overscan), unscaled; the joystick in port 1 is Hatari's
    // keyboard joystick, which KairoSt_Joystick drives; nothing asks or waits for a person.
    std::vector<std::string> args = {
        "hatari",
        "--machine", config.machine,
        "--tos", config.tos,
        "--memsize", std::to_string(config.memory_kib),
        "--monitor", config.monitor,
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
        "--confirm-quit", "off",
        "--mousewarp", "off",
        "--natfeats", "off",
        "--log-file", config.home + "/hatari.log",
        "--log-level", "warn",
        "--alert-level", "error",
    };
    if (!config.floppy[kDriveA].empty()) args.insert(args.end(), {"--disk-a", config.floppy[kDriveA]});
    if (!config.floppy[kDriveB].empty()) args.insert(args.end(), {"--disk-b", config.floppy[kDriveB]});

    auto impl = std::make_unique<Impl>();
    for (int drive = 0; drive < kDrives; ++drive) impl->floppy[drive] = config.floppy[drive];
    Session *state = impl.get();
    running = state;
    state->thread = std::thread([state, args = std::move(args), home = config.home] {
        std::vector<const char *> argv;
        for (const auto &arg : args) argv.push_back(arg.c_str());
        bool clean = KairoSt_Run(int(argv.size()), argv.data(), home.c_str());
        std::lock_guard lock(state->mutex);
        state->quit_cleanly = clean;
        state->ended = true;
        state->changed.notify_all();
    });
    // Hatari's first VBL: the machine is up.
    {
        std::unique_lock lock(state->mutex);
        state->changed.wait(lock, [&] { return state->turn == Session::Turn::kHost || state->ended; });
    }
    if (state->ended) {
        error = state->error();
        state->thread.join();
        running = nullptr;
        return nullptr;
    }
    return std::unique_ptr<StMachine>(new StMachine(std::move(impl)));
}

bool StMachine::run_frame(std::string &error) {
    if (impl_->ended || !impl_->run_hatari()) {
        error = impl_->error();
        return false;
    }
    return true;
}

bool StMachine::booting() const { return KairoSt_Booting(); }

double StMachine::frame_seconds() const { return KairoSt_FrameSeconds(); }

const uint8_t *StMachine::frame(uint32_t &width, uint32_t &height) {
    int w, h, pitch;
    const uint32_t *pixels = KairoSt_Screen(&w, &h, &pitch);
    width = uint32_t(w);
    height = uint32_t(h);
    impl_->rgba.resize(size_t(w) * size_t(h) * 4);
    uint8_t *out = impl_->rgba.data();
    // SDL's dummy window surface is XRGB8888.
    for (int y = 0; y < h; ++y) {
        const uint32_t *row = reinterpret_cast<const uint32_t *>(reinterpret_cast<const uint8_t *>(pixels) + size_t(y) * pitch);
        for (int x = 0; x < w; ++x) {
            uint32_t pixel = row[x];
            *out++ = uint8_t(pixel >> 16);
            *out++ = uint8_t(pixel >> 8);
            *out++ = uint8_t(pixel);
            *out++ = 0xff;
        }
    }
    return impl_->rgba.data();
}

int StMachine::sample_rate() const { return KairoSt_SampleRate(); }

uint32_t StMachine::read_audio(int16_t *stereo, uint32_t max_frames) { return KairoSt_ReadAudio(stereo, max_frames); }

void StMachine::key(int scancode, bool down) { KairoSt_Key(scancode, down); }

void StMachine::joystick(int control, bool down) {
    static_assert(kJoystickControls == KAIRO_ST_JOYSTICK_CONTROLS);
    if (control >= 0 && control < kJoystickControls) KairoSt_Joystick(control, down);
}

void StMachine::mouse_move(int dx, int dy) { KairoSt_MouseMove(dx, dy); }

void StMachine::mouse_button(int button, bool down) {
    if (button == 1 || button == 2) KairoSt_MouseButton(button, down);
}

void StMachine::reset() { KairoSt_Reset(false); }

bool StMachine::save_state(const std::string &file, std::string &error) {
    KairoSt_RequestSave(file.c_str());
    if (!run_frame(error)) return false;
    if (KairoSt_StateFailed()) {
        error = "Hatari could not save the state to " + file;
        return false;
    }
    return true;
}

bool StMachine::load_state(const std::string &file, std::string &error) {
    KairoSt_RequestRestore(file.c_str());
    if (!run_frame(error)) return false;
    if (KairoSt_StateFailed()) {
        error = "Hatari could not restore the state from " + file;
        return false;
    }
    for (int drive = 0; drive < kDrives; ++drive) impl_->floppy[drive] = KairoSt_Floppy(drive);
    return true;
}

bool StMachine::insert(Drive drive, const std::string &path, std::string &error) {
    if (!KairoSt_InsertFloppy(drive, path.c_str())) {
        error = "Hatari refused the disk image " + path;
        return false;
    }
    impl_->floppy[drive] = path;
    return true;
}

std::string StMachine::media_path(Drive drive) { return impl_->floppy[drive]; }

} // namespace kairost

// Hatari's thread.

extern "C" void KairoStHost_Vbl(void) { kairost::running->at_vbl(); }

extern "C" void KairoStHost_Fatal(const char *message) {
    // The first error is the cause; later ones follow from it.
    if (kairost::running->fatal.empty()) kairost::running->fatal = message;
}

// Hatari's own log has it too (core/kairo/alert_hooks.c); this puts it where the platform's
// logs are read. Error level, as some devices keep no other.
extern "C" void KairoStHost_Notice(const char *message) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "KairoST", "Hatari: %s", message);
#else
    std::fprintf(stderr, "Hatari: %s\n", message);
#endif
}
