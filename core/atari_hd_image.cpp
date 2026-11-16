// SPDX-License-Identifier: GPL-2.0-or-later
// A blank hard disk image as Hatari's tools/atari-hd-image.sh makes one (atari_hd_image.h).

#include "atari_hd_image.h"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

namespace {

// The script's disk geometry: 16 heads of 32 sectors of 512 bytes, the partition right after the
// master boot record.
constexpr unsigned kSectorSize = 512;
constexpr unsigned kHeads = 16;
constexpr unsigned kTrackSectors = 32;
// GEMDOS's cluster count limit, which mkdosfs -A meets by doubling the logical sector size.
constexpr uint64_t kMaxClusters = 32765;
constexpr unsigned kRootEntries = 512;

void put16(std::vector<uint8_t> &bytes, size_t at, unsigned value) {
    bytes[at] = value & 0xff;
    bytes[at + 1] = value >> 8 & 0xff;
}

void put32(std::vector<uint8_t> &bytes, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[at + i] = value >> (8 * i) & 0xff;
}

void put_chs(std::vector<uint8_t> &mbr, size_t at, uint32_t lba) {
    uint32_t c = lba / (kTrackSectors * kHeads);
    uint32_t h = lba / kTrackSectors % kHeads;
    uint32_t s = lba % kTrackSectors + 1;
    mbr[at] = h;
    mbr[at + 1] = (s & 0x3f) | (c >> 2 & 0xc0);
    mbr[at + 2] = c & 0xff;
}

// The sum of a sector's 256 big-endian words: 0x1234 makes TOS run it as boot code.
unsigned atari_checksum(const std::vector<uint8_t> &bytes) {
    unsigned sum = 0;
    for (size_t i = 0; i < 512; i += 2) sum += bytes[i] << 8 | bytes[i + 1];
    return sum & 0xffff;
}

// The master boot record the script's Python writes: a partition table whose first partition
// (type 4, FAT16 under 32 MB, or 6) starts at sector 1 and takes the disk, and the BPB-like
// geometry fields it also fills in.
std::vector<uint8_t> master_boot_record(uint32_t partsectors) {
    std::vector<uint8_t> mbr(kSectorSize, 0);
    uint32_t sectors = 1 + partsectors;
    uint8_t type;
    if (sectors < 65536) {
        put16(mbr, 0x13, sectors);
        put32(mbr, 0x20, sectors);
        type = 0x4;
    } else {
        put32(mbr, 0x20, sectors);
        type = 0x6;
    }
    mbr[0x0e] = 1;
    put16(mbr, 0x0b, kSectorSize);
    mbr[0x0d] = 2;
    put16(mbr, 0x18, kTrackSectors);
    put16(mbr, 0x1a, kHeads);
    mbr[0x15] = 0xf8;
    mbr[0x24] = 0x80;
    uint32_t size = partsectors - 1;
    size_t entry = 0x1be;
    mbr[entry] = 0x80;
    mbr[entry + 4] = type;
    put32(mbr, entry + 0x08, 1);
    put32(mbr, entry + 0x0c, size);
    put_chs(mbr, entry + 1, 1);
    put_chs(mbr, entry + 5, size);
    uint32_t next = size + 1;
    for (int i = 1; i < 4; ++i) {
        entry += 0x10;
        put32(mbr, entry + 0x08, next);
        put32(mbr, entry + 0x0c, 0);
        put_chs(mbr, entry + 1, next);
        put_chs(mbr, entry + 5, next);
    }
    mbr[0x1fe] = 0x55;
    mbr[0x1ff] = 0xaa;
    return mbr;
}

} // namespace

std::string kairo::write_atari_hd_image(const std::string &path, unsigned megabytes, const std::string &name) {
    if (megabytes < 5 || megabytes > 512)
        return "A hard disk is 5 to 512 MB, not " + std::to_string(megabytes);
    if (name.empty() || name.size() > 11)
        return "A partition name is 1 to 11 characters, not \"" + name + "\"";
    for (char c : name)
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' '))
            return "A partition name is A to Z, 0 to 9 and spaces, not \"" + name + "\"";

    // The partition: 4 * MB * heads * sectors per track sectors. The file system takes the most
    // of them that is a whole number of "tracks" of mkdosfs's (32 sectors, doubled with the
    // logical sector size while there would be more than GEMDOS's 32765 clusters of 2 sectors).
    const uint32_t partsectors = 4 * megabytes * kHeads * kTrackSectors;
    uint64_t clusters = partsectors / 2;
    uint32_t tracksize = 32;
    uint32_t logical = kSectorSize;
    while (clusters > kMaxClusters) {
        clusters /= 2;
        tracksize *= 2;
        logical *= 2;
    }
    const uint32_t fs_sectors = partsectors / tracksize * tracksize;

    // mkdosfs -A -F 16: logical sectors of `logical` bytes, 2 to a cluster, 1 reserved (the boot
    // sector), 2 FATs, 512 root directory entries, and the FAT length mkdosfs works out.
    const uint32_t count = fs_sectors / (logical / kSectorSize);
    const uint32_t root_sectors = (kRootEntries * 32 + logical - 1) / logical;
    const uint64_t fat_data = count - 1 - root_sectors;
    uint64_t clust = (fat_data * logical + 2 * 4) / (2 * logical + 2 * 2);
    const uint32_t fat_length = static_cast<uint32_t>(((clust + 2) * 2 + logical - 1) / logical);
    clust = (fat_data - 2ull * fat_length) / 2;
    const uint64_t fat_entries = static_cast<uint64_t>(fat_length) * logical / 2 - 2;
    if (clust > fat_entries) clust = fat_entries;
    if (clust < 4085 || clust > 65524)
        return "A " + std::to_string(megabytes) + " MB hard disk would have " + std::to_string(clust) +
               " clusters, not a FAT16 file system's 4085 to 65524";
    if (count > 65535) return "A " + std::to_string(megabytes) + " MB hard disk needs more logical sectors than FAT16's 65535";

    std::random_device random;
    const uint32_t volume_id = random();

    // The boot sector, as mkdosfs -A writes it: a 2-byte jump, "kdosf", a 3-byte Atari serial
    // number, the BIOS parameter block and the FAT16 extended block with the partition's name.
    std::vector<uint8_t> boot(logical, 0);
    boot[0] = 0xeb;
    boot[1] = 0x3c;
    boot[2] = 'm';
    const char *oem = "kdosf";
    for (int i = 0; i < 5; ++i) boot[3 + i] = oem[i];
    boot[8] = volume_id & 0xff;
    boot[9] = volume_id >> 8 & 0xff;
    boot[10] = volume_id >> 16 & 0xff;
    put16(boot, 0x0b, logical);
    boot[0x0d] = 2;
    put16(boot, 0x0e, 1);
    boot[0x10] = 2;
    put16(boot, 0x11, kRootEntries);
    put16(boot, 0x13, count);
    boot[0x15] = 0xf8;
    put16(boot, 0x16, fat_length);
    put16(boot, 0x18, kTrackSectors);
    put16(boot, 0x1a, kHeads);
    boot[0x24] = 0x80;
    boot[0x26] = 0x29;
    put32(boot, 0x27, volume_id);
    std::string label = name + std::string(11 - name.size(), ' ');
    for (int i = 0; i < 11; ++i) boot[0x2b + i] = label[i];
    const char *type = "FAT16   ";
    for (int i = 0; i < 8; ++i) boot[0x36 + i] = type[i];
    boot[0x1fe] = 0x55;
    boot[0x1ff] = 0xaa;
    // Never boot code (a sum of 0x1234): the serial number came out so, so a byte of the unused
    // boot code area takes the sum elsewhere.
    if (atari_checksum(boot) == 0x1234) boot[0x1fd] = 1;

    std::vector<uint8_t> fat(static_cast<size_t>(fat_length) * logical, 0);
    put16(fat, 0, 0xfff8);
    put16(fat, 2, 0xffff);

    // The root directory: the partition's name as its volume label entry, made now.
    std::vector<uint8_t> root(static_cast<size_t>(root_sectors) * logical, 0);
    for (int i = 0; i < 11; ++i) root[i] = label[i];
    root[11] = 0x08;
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    const unsigned time = local.tm_hour << 11 | local.tm_min << 5 | local.tm_sec / 2;
    const unsigned date = (local.tm_year - 80) << 9 | (local.tm_mon + 1) << 5 | local.tm_mday;
    put16(root, 0x16, time);
    put16(root, 0x18, date);

    const std::vector<uint8_t> mbr = master_boot_record(partsectors);
    if (atari_checksum(mbr) == 0x1234) return "The master boot record would be boot code";

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return "Cannot write a hard disk at " + path;
    out.write(reinterpret_cast<const char *>(mbr.data()), mbr.size());
    out.write(reinterpret_cast<const char *>(boot.data()), boot.size());
    out.write(reinterpret_cast<const char *>(fat.data()), fat.size());
    out.write(reinterpret_cast<const char *>(fat.data()), fat.size());
    out.write(reinterpret_cast<const char *>(root.data()), root.size());
    out.close();
    if (!out) return "Cannot write a hard disk at " + path;
    // The rest of the file system is zeros, as the script's dd leaves the image: the master boot
    // record and the file system's sectors.
    std::error_code error;
    std::filesystem::resize_file(path, (1ull + fs_sectors) * kSectorSize, error);
    if (error) return "Cannot size the hard disk at " + path + ": " + error.message();
    return "";
}
