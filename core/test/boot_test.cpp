// Boots an ST on the host and runs it for a while without Android.
//
// Usage: boot_test <tos image> <work folder> <seconds> [machine [floppy image [runs [keys]]]]
//
// Runs the machine for <seconds> of guest time, writing a frame every five seconds to
// <work folder>/frame-<run>-<seconds>s.bmp and the last to <work folder>/frame-<run>.bmp, and reports the frame size and how much sound it made. With
// <runs> above 1 it starts a new machine after each, in the same process, as the app does for the
// next game. <keys> is a list of "frame:scancode" presses (comma separated), each held for five
// frames.

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
        presses.push_back({std::stol(item.substr(0, colon)), int(std::stol(item.substr(colon + 1), nullptr, 0))});
    }
    return presses;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: boot_test <tos image> <work folder> <seconds> [machine [floppy [runs [keys]]]]\n");
        return 2;
    }
    kairost::Config config;
    config.tos = argv[1];
    config.home = argv[2];
    const double seconds = std::atof(argv[3]);
    if (argc > 4) config.machine = argv[4];
    if (argc > 5) config.floppy[kairost::kDriveA] = argv[5];
    const int runs = argc > 6 ? std::atoi(argv[6]) : 1;
    const std::vector<Press> presses = argc > 7 ? parse_keys(argv[7]) : std::vector<Press>{};

    std::vector<int16_t> audio(48000 * 2);
    for (int run = 0; run < runs; ++run) {
        std::string error;
        auto machine = kairost::StMachine::create(config, error);
        if (!machine) {
            std::fprintf(stderr, "run %d: could not start: %s\n", run, error.c_str());
            return 1;
        }
        double elapsed = 0;
        long frames = 0;
        uint64_t samples = 0;
        int64_t loudest = 0;
        while (elapsed < seconds) {
            for (const auto &press : presses) {
                if (frames == press.frame) machine->key(press.scancode, true);
                if (frames == press.frame + 5) machine->key(press.scancode, false);
            }
            if (!machine->run_frame(error)) {
                std::fprintf(stderr, "run %d: stopped after %ld frames: %s\n", run, frames, error.c_str());
                return 1;
            }
            const double before = elapsed;
            elapsed += machine->frame_seconds();
            if (int(elapsed / 5) != int(before / 5)) {
                uint32_t width, height;
                const uint8_t *rgba = machine->frame(width, height);
                write_bmp(config.home + "/frame-" + std::to_string(run) + "-" + std::to_string(int(elapsed)) + "s.bmp",
                          rgba, width, height);
            }
            ++frames;
            uint32_t got = machine->read_audio(audio.data(), uint32_t(audio.size() / 2));
            samples += got;
            for (uint32_t i = 0; i < got * 2; ++i) loudest = std::max<int64_t>(loudest, std::abs(int(audio[i])));
        }
        uint32_t width, height;
        const uint8_t *rgba = machine->frame(width, height);
        std::string bmp = config.home + "/frame-" + std::to_string(run) + ".bmp";
        write_bmp(bmp, rgba, width, height);
        std::printf("run %d: %ld frames (%.4f s each), %ux%u, %llu sound frames at %d Hz, peak %lld, %s\n", run,
                    frames, machine->frame_seconds(), width, height, (unsigned long long) samples,
                    machine->sample_rate(), (long long) loudest, bmp.c_str());
    }
    return 0;
}
