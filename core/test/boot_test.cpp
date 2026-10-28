// Boots an ST on the host and runs it for a while without Android.
//
// Usage: boot_test <tos image> <work folder> <seconds> [machine [floppy image [runs [keys [country [memory KiB
//                  [startup program]]]]]]]
//
// <startup program> is a program in the top folder of drive A's disk for TOS to start
// (Config::floppy_program).
//
// Runs the machine for <seconds> of guest time, writing a frame every five seconds to
// <work folder>/frame-<run>-<seconds>s.bmp and the last to <work folder>/frame-<run>.bmp, and reports the frame size and how much sound it made. With
// <runs> above 1 it starts a new machine after each, in the same process, as the app does for the
// next game. <floppy image> may list several media separated by "|" (which no file name holds):
// run n starts the n-th, wrapping around, so one process can run a program and then a floppy.
// A program (.prg, .tos, .ttp, .app, .gtp) runs from its folder instead of a floppy. It reports when the boot ends (StMachine::booting) and writes that frame to
// <work folder>/frame-<run>-booted.bmp. <keys> is a list of "frame:scancode" presses (comma separated), each held for five
// frames; "frame:fire" presses the joystick's fire button instead. It reports when the guest
// crashes (a TOS bomb or EmuTOS panic, StMachine::guest_crashed), and the sound's peak over the
// whole
// run, before the first press ("early peak": a press makes TOS click) and over its last ten
// seconds ("late peak"), whether the machine ends on the GEM desktop (StMachine::on_desktop) and
// with Caps Lock on (StMachine::locks).
// For debugging, each run's last CPU state goes to <work folder>/cpu-<run>.txt and its ST RAM to
// <work folder>/ram-<run>.bin.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "st_machine.h"

namespace {

void write_bmp(const std::string &path, const uint8_t *rgba, uint32_t width, uint32_t height) {
    const uint32_t row = width * 3 + (4 - width * 3 % 4) % 4;
    const uint32_t size = 54 + row * height;
    std::vector<uint8_t> file(size, 0);
    auto put32 = [&](size_t at, uint32_t value) {
        for (int i = 0; i < 4; ++i) file[at + i] = uint8_t(value >> (8 * i));
    };
    file[0] = 'B';
    file[1] = 'M';
    put32(2, size);
    put32(10, 54);
    put32(14, 40);
    put32(18, width);
    put32(22, height);
    file[26] = 1;
    file[28] = 24;
    put32(34, row * height);
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t *in = rgba + size_t(height - 1 - y) * width * 4;
        uint8_t *out = file.data() + 54 + size_t(y) * row;
        for (uint32_t x = 0; x < width; ++x, in += 4) {
            *out++ = in[2];
            *out++ = in[1];
            *out++ = in[0];
        }
    }
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(file.data()), file.size());
}

// A key by its scan code, or the joystick's fire button (kFire).
constexpr int kFire = -1;

struct Press {
    long frame;
    int scancode;
};

std::vector<Press> parse_keys(const std::string &text) {
    std::vector<Press> presses;
    std::stringstream list(text);
    std::string item;
    while (std::getline(list, item, ',')) {
        auto colon = item.find(':');
        if (colon == std::string::npos) {
            std::fprintf(stderr, "bad key press %s\n", item.c_str());
            std::exit(2);
        }
        const std::string what = item.substr(colon + 1);
        presses.push_back({std::stol(item.substr(0, colon)), what == "fire" ? kFire : int(std::stol(what, nullptr, 0))});
    }
    return presses;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: boot_test <tos image> <work folder> <seconds> [machine [floppy [runs [keys [country [memory KiB [startup program]]]]]]]\n");
        return 2;
    }
    kairost::Config base;
    base.tos = argv[1];
    base.home = argv[2];
    const double seconds = std::atof(argv[3]);
    if (argc > 4) base.machine = argv[4];
    std::vector<kairost::Config> configs;
    if (argc > 5 && *argv[5]) {
        std::stringstream list(argv[5]);
        std::string media;
        while (std::getline(list, media, '|')) {
            kairost::Config config = base;
            std::string extension = media.substr(media.find_last_of('.') + 1);
            for (char &c : extension) c = char(std::tolower(static_cast<unsigned char>(c)));
            if (extension == "prg" || extension == "tos" || extension == "ttp" || extension == "app" || extension == "gtp")
                config.program = media;
            else
                config.floppy[kairost::kDriveA] = media;
            // Hatari ends the process without a word when it cannot open the disk (on Windows, a
            // path over MAX_PATH), so say which file it was.
            if (!std::ifstream(media, std::ios::binary)) {
                std::fprintf(stderr, "run 0: could not start: cannot open %s\n", media.c_str());
                return 1;
            }
            configs.push_back(config);
        }
    }
    if (configs.empty()) configs.push_back(base);
    const int runs = argc > 6 ? std::atoi(argv[6]) : 1;
    const std::vector<Press> presses = argc > 7 && *argv[7] ? parse_keys(argv[7]) : std::vector<Press>{};
    if (argc > 8 && *argv[8])
        for (auto &config : configs) config.country = argv[8];
    if (argc > 9 && *argv[9])
        for (auto &config : configs) config.memory_kib = std::atoi(argv[9]);
    if (argc > 10)
        for (auto &config : configs) config.floppy_program = argv[10];

    std::vector<int16_t> audio(48000 * 2);
    int status = 0;
    for (int run = 0; run < runs; ++run) {
        const kairost::Config &config = configs[size_t(run) % configs.size()];
        std::string error;
        auto machine = kairost::StMachine::create(config, error);
        if (!machine) {
            std::fprintf(stderr, "run %d: could not start: %s\n", run, error.c_str());
            return 1;
        }
        double elapsed = 0;
        long frames = 0;
        bool booting = machine->booting();
        uint64_t samples = 0;
        int64_t loudest = 0, early_loudest = 0, late_loudest = 0;
        bool crashed = false;
        const long first_press = presses.empty() ? -1 : std::min_element(presses.begin(), presses.end(),
            [](const Press &a, const Press &b) { return a.frame < b.frame; })->frame;
        while (elapsed < seconds) {
            for (const auto &press : presses) {
                for (bool down : {true, false}) {
                    if (frames != press.frame + (down ? 0 : 5)) continue;
                    if (press.scancode == kFire) machine->joystick(4, down);
                    else machine->key(press.scancode, down);
                }
            }
            if (!machine->run_frame(error)) {
                // The app starts the next game after a machine that stopped, in the same process.
                std::fprintf(stderr, "run %d: stopped after %ld frames: %s\n", run, frames, error.c_str());
                status = 1;
                break;
            }
            if (!crashed && machine->guest_crashed()) {
                crashed = true;
                std::printf("run %d: the guest crashed after %ld frames (%.1f s)\n", run, frames, elapsed);
            }
            if (booting && !machine->booting()) {
                booting = false;
                std::printf("run %d: boot over after %ld frames (%.1f s)\n", run, frames, elapsed);
                uint32_t width, height;
                const uint8_t *rgba = machine->picture(width, height);
                write_bmp(config.home + "/frame-" + std::to_string(run) + "-booted.bmp", rgba, width, height);
            }
            const double before = elapsed;
            elapsed += machine->frame_seconds();
            if (int(elapsed / 5) != int(before / 5)) {
                uint32_t width, height;
                const uint8_t *rgba = machine->picture(width, height);
                write_bmp(config.home + "/frame-" + std::to_string(run) + "-" + std::to_string(int(elapsed)) + "s.bmp",
                          rgba, width, height);
            }
            ++frames;
            uint32_t got = machine->read_audio(audio.data(), uint32_t(audio.size() / 2));
            samples += got;
            const bool late = elapsed > seconds - 10;
            for (uint32_t i = 0; i < got * 2; ++i) {
                const int64_t level = std::abs(int(audio[i]));
                loudest = std::max(loudest, level);
                if (late) late_loudest = std::max(late_loudest, level);
                if (first_press < 0 || frames <= first_press) early_loudest = std::max(early_loudest, level);
            }
        }
        if (elapsed < seconds) continue;
        uint32_t width, height;
        const uint8_t *rgba = machine->picture(width, height);
        std::string bmp = config.home + "/frame-" + std::to_string(run) + ".bmp";
        write_bmp(bmp, rgba, width, height);
        {
            uint32_t registers[16], pc;
            uint16_t sr;
            machine->cpu_state(registers, pc, sr);
            std::ofstream cpu(config.home + "/cpu-" + std::to_string(run) + ".txt");
            char line[128];
            std::snprintf(line, sizeof(line), "pc %08x sr %04x\n", pc, sr);
            cpu << line;
            for (int i = 0; i < 16; ++i) {
                std::snprintf(line, sizeof(line), "%c%d %08x\n", i < 8 ? 'd' : 'a', i % 8, registers[i]);
                cpu << line;
            }
            const auto ram = machine->read_memory(0, uint32_t(config.memory_kib) * 1024);
            std::ofstream(config.home + "/ram-" + std::to_string(run) + ".bin", std::ios::binary)
                .write(reinterpret_cast<const char *>(ram.data()), std::streamsize(ram.size()));
        }
        std::printf("run %d: %ld frames (%.4f s each), %ux%u, %llu sound frames at %d Hz, peak %lld, early peak %lld, late peak %lld, desktop %s, caps %s, %s%s\n", run,
                    frames, machine->frame_seconds(), width, height, (unsigned long long) samples,
                    machine->sample_rate(), (long long) loudest, (long long) early_loudest, (long long) late_loudest,
                    machine->on_desktop() < 0 ? "unknown" : machine->on_desktop() ? "yes" : "no", machine->locks() & 1 ? "on" : "off", bmp.c_str(), booting ? ", still booting" : "");
    }
    return status;
}
