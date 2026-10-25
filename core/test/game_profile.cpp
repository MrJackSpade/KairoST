// Measures a game run (KairoST#31): boots a disk or program, then runs a fixed number of frames as
// fast as the host can with scripted input, and reports host time per guest frame. The same run on
// every build, on the host or on the device (KAIROST_TOOLS=ON with the NDK), makes builds
// comparable; --hashes shows that two builds run the guest identically.
//
//   game_profile <tos image> <work dir> <media> <machine> <memory KiB> <frames> <script> [options]
//
// <media> is a floppy image or a program (.prg, .tos, .ttp, .app, .gtp), as boot_test takes it.
// <script> is a comma-separated list of <frame>:<+|-><key code|fire> events, by frame after the boot
// ends: "+" presses, "-" lets go; key codes are ST scan codes (0x39 is Space). "-" alone is no input.
//
// Options:
//   --boot-frames <n> run exactly n frames as the boot instead of until the boot ends
//                     (StMachine::booting)
//   --warmup <n>      run n frames after the boot before measuring (script frames count from the boot)
//   --seed <n>        Hatari's random seed (default 1, so runs repeat; Config::seed)
//   --program <name>  the program on drive A that TOS starts (Config::floppy_program)
//   --shots <n>       write the picture every n measured frames as <work dir>/frame-<frame>.bmp
//   --hashes <file>   write a hash of each frame's picture and sound, boot frames included
//   --nice <n>        run Hatari's thread at this priority, as the app does (-16; root on Android)
//   --present         take each frame's picture and copy it, as the app's frame host does
//                     (StMachine::frame, then FrameMachine::publish's copy), and time it apart
//
// Prints one summary line starting "host ": frames per second the host reached, the guest's
// rate, and the frame time distribution; and one starting "boot ": the boot's frames and rate.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "st_machine.h"

#ifndef _WIN32
#include <sys/resource.h>
#endif

using kairost::Config;
using kairost::StMachine;

namespace {

int fail(const std::string &message) {
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    return 1;
}

struct Event {
    long frame;
    bool down;
    int code;  // a scan code, or kFire
};

constexpr int kFire = -1;

bool parse_script(const std::string &script, std::vector<Event> &events, std::string &error) {
    if (script == "-") return true;
    size_t start = 0;
    while (start <= script.size()) {
        const size_t end = std::min(script.find(',', start), script.size());
        const std::string item = script.substr(start, end - start);
        const size_t colon = item.find(':');
        if (colon == std::string::npos || colon + 2 >= item.size() || (item[colon + 1] != '+' && item[colon + 1] != '-')) {
            error = "bad script event \"" + item + "\"";
            return false;
        }
        const std::string what = item.substr(colon + 2);
        events.push_back({std::stol(item.substr(0, colon)), item[colon + 1] == '+',
                          what == "fire" ? kFire : int(std::stol(what, nullptr, 0))});
        start = end + 1;
    }
    return true;
}

// FNV-1a, over a frame's picture or sound.
uint64_t hash(const uint8_t *data, size_t size) {
    uint64_t value = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i) value = (value ^ data[i]) * 1099511628211ull;
    return value;
}

void write_bmp(const std::string &path, const uint8_t *rgba, uint32_t width, uint32_t height) {
    const uint32_t row = width * 3 + (4 - width * 3 % 4) % 4;
    std::vector<uint8_t> file(54 + size_t(row) * height, 0);
    auto put32 = [&](size_t at, uint32_t value) {
        for (int i = 0; i < 4; ++i) file[at + i] = uint8_t(value >> (8 * i));
    };
    file[0] = 'B';
    file[1] = 'M';
    put32(2, uint32_t(file.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, width);
    put32(22, height);
    file[26] = 1;
    file[28] = 24;
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t *in = rgba + size_t(height - 1 - y) * width * 4;
        uint8_t *out = file.data() + 54 + size_t(y) * row;
        for (uint32_t x = 0; x < width; ++x, in += 4) {
            *out++ = in[2];
            *out++ = in[1];
            *out++ = in[0];
        }
    }
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(file.data()), std::streamsize(file.size()));
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 8)
        return fail("usage: game_profile <tos image> <work dir> <media> <machine> <memory KiB> <frames> <script> [options]");
    Config config;
    config.tos = argv[1];
    config.home = argv[2];
    const std::string media = argv[3];
    config.machine = argv[4];
    config.memory_kib = std::atoi(argv[5]);
    const long frames = std::atol(argv[6]);
    std::vector<Event> events;
    std::string error;
    if (!parse_script(argv[7], events, error)) return fail(error);
    std::string extension = media.substr(media.find_last_of('.') + 1);
    for (char &c : extension) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (extension == "prg" || extension == "tos" || extension == "ttp" || extension == "app" || extension == "gtp")
        config.program = media;
    else
        config.floppy[kairost::kDriveA] = media;
    if (!std::ifstream(media, std::ios::binary)) return fail("cannot open " + media);
    config.seed = 1;
    long boot_frames = -1, warmup = 0, shots = 0;
    std::string hashes_path;
    bool present = false;
    for (int i = 8; i < argc; ++i) {
        const std::string option = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "FAIL: %s needs a value\n", option.c_str());
                std::exit(1);
            }
            return argv[++i];
        };
        if (option == "--boot-frames") boot_frames = std::atol(value().c_str());
        else if (option == "--warmup") warmup = std::atol(value().c_str());
        else if (option == "--seed") config.seed = unsigned(std::strtoul(value().c_str(), nullptr, 0));
        else if (option == "--program") config.floppy_program = value();
        else if (option == "--shots") shots = std::atol(value().c_str());
        else if (option == "--hashes") hashes_path = value();
        else if (option == "--present") present = true;
        else if (option == "--nice") {
#ifdef _WIN32
            return fail("--nice is for Linux and Android");
#else
            const int nice = std::atoi(value().c_str());
            config.on_thread_start = [nice] {
                if (setpriority(PRIO_PROCESS, 0, nice) != 0) {
                    std::fprintf(stderr, "FAIL: could not set priority %d\n", nice);
                    std::exit(1);
                }
            };
#endif
        }
        else return fail("unknown option " + option);
    }
    if (config.seed == 0) return fail("--seed 0 means the time; give a fixed seed");

    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    auto machine = StMachine::create(config, error);
    if (!machine) return fail("could not start: " + error);
    const double start_seconds = std::chrono::duration<double>(Clock::now() - started).count();
    std::FILE *hashes = hashes_path.empty() ? nullptr : std::fopen(hashes_path.c_str(), "w");
    if (!hashes_path.empty() && !hashes) return fail("cannot write " + hashes_path);
    std::vector<int16_t> audio(48000 * 2);
    std::vector<uint8_t> published;
    double present_ms = 0;

    // One frame: runs it, takes its sound and hashes picture and sound when asked.
    long frame = 0;
    auto run = [&]() -> bool {
        if (!machine->run_frame(error)) return false;
        uint32_t got, total = 0;
        uint64_t sound = 1469598103934665603ull;
        while ((got = machine->read_audio(audio.data(), uint32_t(audio.size() / 2))) > 0) {
            total += got;
            if (hashes) sound ^= hash(reinterpret_cast<const uint8_t *>(audio.data()), size_t(got) * 4);
        }
        if (hashes) {
            uint32_t width, height;
            const uint8_t *rgba = machine->picture(width, height);
            std::fprintf(hashes, "%ld %016llx %016llx %u\n", frame,
                         (unsigned long long) hash(rgba, size_t(width) * height * 4), (unsigned long long) sound, total);
        }
        ++frame;
        return true;
    };

    const auto boot_start = Clock::now();
    long booted = 0;
    while (boot_frames >= 0 ? booted < boot_frames : machine->booting()) {
        if (!run()) return fail("stopped while booting after " + std::to_string(booted) + " frames: " + error);
        ++booted;
    }
    const double boot_seconds = std::chrono::duration<double>(Clock::now() - boot_start).count();
    std::printf("boot %ld frames in %.2f s, %.2f frames/s (start %.3f s)\n", booted, boot_seconds,
                booted / std::max(boot_seconds, 1e-9), start_seconds);

    std::vector<double> times;
    times.reserve(size_t(frames));
    for (long after = 0; after < warmup + frames; ++after) {
        for (const Event &event : events) {
            if (event.frame != after) continue;
            if (event.code == kFire) machine->joystick(4, event.down);
            else machine->key(event.code, event.down);
        }
        const auto before = Clock::now();
        if (!run()) return fail("stopped after " + std::to_string(after) + " frames: " + error);
        if (present) {
            const auto taking = Clock::now();
            uint32_t width, height;
            const uint8_t *rgba = machine->frame(width, height);
            published.assign(rgba, rgba + size_t(width) * height * 4);
            if (after >= warmup) present_ms += std::chrono::duration<double, std::milli>(Clock::now() - taking).count();
        }
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - before).count();
        if (after >= warmup) {
            times.push_back(ms);
            const long measured = after - warmup;
            if (shots > 0 && measured % shots == 0) {
                uint32_t width, height;
                const uint8_t *rgba = machine->picture(width, height);
                write_bmp(config.home + "/frame-" + std::to_string(measured) + ".bmp", rgba, width, height);
            }
        }
    }
    if (hashes) std::fclose(hashes);
    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    double total = 0;
    for (double t : times) total += t;
    const double mean = total / double(times.size());
    std::printf("host %.2f frames/s (guest %.2f), frame ms mean %.3f median %.3f p95 %.3f max %.3f over %zu frames\n",
                1000.0 / mean, 1.0 / machine->frame_seconds(), mean, sorted[sorted.size() / 2],
                sorted[std::min(sorted.size() - 1, sorted.size() * 95 / 100)], sorted.back(), times.size());
    if (present) std::printf("present ms mean %.3f\n", present_ms / double(times.size()));
    return 0;
}
