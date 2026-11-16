// SPDX-License-Identifier: GPL-2.0-or-later
// A blank hard disk image, partitioned and formatted as Hatari's tools/atari-hd-image.sh makes one
// (KairoST#88): a DOS master boot record whose one partition holds an Atari-compatible FAT16 file
// system, which EmuTOS and TOS with a hard disk driver read and write. EmuTOS cannot partition or
// format a hard disk, so a player's game that installs to one needs a disk made this way.
#pragma once

#include <string>

namespace kairo {

// Writes a blank hard disk of `megabytes` MB (5 to 512, the script's range) at `path`, its one
// partition named `name` (at most 11 characters: A to Z, 0 to 9 and space). "" on success,
// else what went wrong.
std::string write_atari_hd_image(const std::string &path, unsigned megabytes, const std::string &name);

} // namespace kairo
