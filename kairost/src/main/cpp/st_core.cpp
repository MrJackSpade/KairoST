// The Atari ST machine (core/st_machine.h) as the MachineCore the shared frame host runs
// (shared/frontend/src/main/cpp/kairo/machine_core.h).

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "kairo/machine_core.h"
#include "kairo/thread_priority.h"
#include "st_machine.h"

namespace {

using kairost::StMachine;

// Media slots, as MainActivity numbers them: the core's drives.
static_assert(kairost::kDriveA == 0 && kairost::kDriveB == 1);
// MainActivity.machine.stateFile.
constexpr const char *kStateFile = "/state.sts";
// MainActivity.stateError reads these.
// StKeys numbers a key by its ST scan code, 1 to 0x72.
constexpr int kKeyCodes = 0x73;

class StCore : public kairo::MachineCore {
public:
    explicit StCore(std::unique_ptr<StMachine> machine) : machine_(std::move(machine)) {}

    bool run_frame() override { return machine_->run_frame(error_); }
    bool loading() override { return machine_->loading(); }
    // Bit 0 Caps, as NativeMachine lists it (MainActivity).
    int locks() override { return machine_->locks(); }
    std::string error() override { return error_; }
    // The emulator's reason, which save_state and load_state leave in error_.
    std::string state_error() override { return error_; }
    const uint8_t *frame(uint32_t &width, uint32_t &height) override { return machine_->frame(width, height); }
    // An ST low or medium resolution screen comes one row per ST line; the presenter repeats it.
    uint32_t line_repeat() override { return machine_->line_repeat(); }
    double frame_seconds() override { return machine_->frame_seconds(); }
    int32_t audio_rate() override { return machine_->sample_rate(); }
    uint32_t read_audio(int16_t *stereo, uint32_t max_frames) override { return machine_->read_audio(stereo, max_frames); }

    void release_inputs() override {
        for (int code = 1; code < kKeyCodes; ++code)
            if (keys_[code]) key(code, false);
        for (int control = 0; control < kairost::kJoystickControls; ++control) machine_->joystick(control, false);
        machine_->mouse_button(1, false);
        machine_->mouse_button(2, false);
    }

    void key(int code, bool down) override {
        if (code <= 0 || code >= kKeyCodes || keys_[code] == down) return;
        keys_[code] = down;
        machine_->key(code, down);
    }

    // Controls in StKeys.joystick order, whose first five are the core's.
    void joystick(int control, bool down) override { machine_->joystick(control, down); }
    // The mouse in port 0.
    void mouse_move(int dx, int dy) override { machine_->mouse_move(dx, dy); }
    void mouse_button(int button, bool down) override { machine_->mouse_button(button, down); }

    void reset() override { machine_->reset(); }

    int save_state(const std::string &directory) override {
        return machine_->save_state(directory + kStateFile, error_) ? 0 : kFailed;
    }

    int load_state(const std::string &directory) override {
        return machine_->load_state(directory + kStateFile, error_) ? 0 : kFailed;
    }

    int media(int slot, const std::string &path) override {
        if (slot < 0 || slot >= kairost::kDrives) return kUnsupported;
        return machine_->insert(kairost::Drive(slot), path, error_) ? 0 : kFailed;
    }

    std::vector<std::string> media_paths() override {
        std::vector<std::string> paths;
        for (int drive = 0; drive < kairost::kDrives; ++drive) paths.push_back(machine_->media_path(kairost::Drive(drive)));
        return paths;
    }

private:
    std::unique_ptr<StMachine> machine_;
    std::string error_;
    std::array<bool, kKeyCodes> keys_{};
};

} // namespace

// The launch's home holds "emutos" with the EmuTOS image and "hatari", Hatari's own files, as
// MainActivity prepares them; its firmware folder holds the player's tos.img.
std::unique_ptr<kairo::MachineCore> kairo::create_core(const kairo::Launch &launch, std::string &error) {
    kairost::Config config;
    config.home = launch.home + "/hatari";
    for (int drive = 0; drive < kairost::kDrives; ++drive)
        if (int(launch.media.size()) > drive) config.floppy[drive] = launch.media[drive];
    // After the drives: a program, which runs from its folder (MainActivity.startGame).
    if (int(launch.media.size()) > kairost::kDrives) config.program = launch.media[kairost::kDrives];
    // The settings StCatalog's options name; every launch passes them.
    // "emutos", or the file of the player's TOS for the game's model (MainActivity.startGame).
    const std::string &tos = launch.settings.at("tos");
    config.tos = tos == "emutos" ? launch.home + "/emutos/etos1024k.img" : launch.firmware + "/" + tos;
    config.machine = launch.settings.at("machine");
    const std::string &clock = launch.settings.at("clock");
    config.cpu_clock = clock == "auto" ? 0 : std::stoi(clock);
    config.memory_kib = std::stoi(launch.settings.at("memory"));
    config.monitor = launch.settings.at("monitor");
    config.country = launch.settings.at("region");
    const std::string &skip = launch.settings.at("skipMemoryCheck");
    if (skip != "on" && skip != "off") {
        error = "Skip memory check is \"" + skip + "\", not on or off";
        return nullptr;
    }
    config.skip_memory_check = skip == "on";
    // The program on drive A that TOS starts, "" for the automatic choice (MainActivity: the game's
    // Startup program setting).
    config.floppy_program = launch.settings.at("program");
    // Hatari runs the emulation on a thread of its own, which takes emulation work's priority.
    config.on_thread_start = [] {
        kairo::name_thread("kairost-hatari");
        kairo::raise_emulation_priority("hatari");
    };
    auto machine = StMachine::create(config, error);
    if (!machine) return nullptr;
    return std::make_unique<StCore>(std::move(machine));
}
