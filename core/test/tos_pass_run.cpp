// The TOS pass's runner (tools/tos_pass.py): boots a floppy on the host for a while, pressing keys,
// and reports what tools/tos_pass.py judges the disk by. Not a boot test: those are the shared PC
// boot test's, on core/test/boot-tests.json (KairoST#87).
//
// Usage: tos_pass_run <tos image> <work folder> <seconds> <machine> <floppy image> <keys>
//
// <keys> is a list of "frame:scancode" presses (comma separated), each held for five frames;
// "frame:fire" presses the joystick's fire button instead. Runs the machine for <seconds> of guest
// time from drive A and writes its last picture to <work folder>/frame.bmp. Prints one JSON line:
// {"earlyPeak", "latePeak", "crashed", "desktop", "error"}: the sound's peak before the first press
// (a press makes TOS click) and over the last ten seconds, whether the guest crashed (a TOS bomb or
// an EmuTOS panic, StMachine::guest_crashed), whether it ends on the GEM desktop
// (StMachine::on_desktop), and why the machine could not start or stopped, or null.
// The runner itself fails (exit 2) only on bad arguments.

#include <algorithm>
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
        const auto colon = item.find(':');
        if (colon == std::string::npos) {
            std::fprintf(stderr, "bad key press %s\n", item.c_str());
            std::exit(2);
        }
        const std::string what = item.substr(colon + 1);
        presses.push_back({std::stol(item.substr(0, colon)), what == "fire" ? kFire : int(std::stol(what, nullptr, 0))});
    }
    return presses;
}

std::string json_text(const std::string &text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            out += escaped;
            continue;
        }
        out += c;
    }
    return out + "\"";
}

void report(int64_t early, int64_t late, bool crashed, bool desktop, const std::string &error) {
    std::printf("{\"earlyPeak\": %lld, \"latePeak\": %lld, \"crashed\": %s, \"desktop\": %s, \"error\": %s}\n",
                (long long) early, (long long) late, crashed ? "true" : "false", desktop ? "true" : "false",
                error.empty() ? "null" : json_text(error).c_str());
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 7) {
        std::fprintf(stderr, "usage: tos_pass_run <tos image> <work folder> <seconds> <machine> <floppy image> <keys>\n");
        return 2;
    }
    kairost::Config config;
    config.tos = argv[1];
    config.home = argv[2];
    const double seconds = std::atof(argv[3]);
    config.machine = argv[4];
    config.floppy[kairost::kDriveA] = argv[5];
    const std::vector<Press> presses = *argv[6] ? parse_keys(argv[6]) : std::vector<Press>{};
    // Hatari ends the process without a word when it cannot open the disk (on Windows, a path over
    // MAX_PATH), so say which file it was.
    if (!std::ifstream(argv[5], std::ios::binary)) {
        report(0, 0, false, false, std::string("cannot open ") + argv[5]);
        return 0;
    }
    std::string error;
    auto machine = kairost::StMachine::create(config, error);
    if (!machine) {
        report(0, 0, false, false, error.empty() ? "the machine could not start" : error);
        return 0;
    }
    const long first_press = presses.empty() ? -1 : std::min_element(presses.begin(), presses.end(),
        [](const Press &a, const Press &b) { return a.frame < b.frame; })->frame;
    std::vector<int16_t> audio(48000 * 2);
    double elapsed = 0;
    long frames = 0;
    int64_t early = 0, late = 0;
    bool crashed = false;
    while (elapsed < seconds) {
        for (const auto &press : presses) {
            for (const bool down : {true, false}) {
                if (frames != press.frame + (down ? 0 : 5)) continue;
                if (press.scancode == kFire) machine->joystick(4, down);
                else machine->key(press.scancode, down);
            }
        }
        if (!machine->run_frame(error)) {
            report(early, late, crashed, false, error.empty() ? "the machine stopped" : error);
            return 0;
        }
        crashed = crashed || machine->guest_crashed();
        elapsed += machine->frame_seconds();
        ++frames;
        const uint32_t got = machine->read_audio(audio.data(), uint32_t(audio.size() / 2));
        const bool at_end = elapsed > seconds - 10;
        for (uint32_t i = 0; i < got * 2; ++i) {
            const int64_t level = std::abs(int(audio[i]));
            if (at_end) late = std::max(late, level);
            if (first_press < 0 || frames <= first_press) early = std::max(early, level);
        }
    }
    uint32_t width, height;
    const uint8_t *rgba = machine->picture(width, height);
    write_bmp(config.home + "/frame.bmp", rgba, width, height);
    report(early, late, crashed, machine->on_desktop() == 1, "");
    return 0;
}
