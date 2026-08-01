#include "pack.hpp"
#include "common.hpp"
#include "encryption.hpp"
#include "miniz.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <algorithm>

static std::vector<uint16_t> to_utf16(const std::string &utf8) {
    std::vector<uint16_t> utf16;
    size_t i = 0;
    while (i < utf8.size()) {
        uint32_t cp = 0;
        uint8_t c = static_cast<uint8_t>(utf8[i]);
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < utf8.size()) {
                cp = ((c & 0x1F) << 6) | (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < utf8.size()) {
                cp = ((c & 0x0F) << 12) | ((static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 6) | (static_cast<uint8_t>(utf8[i + 2]) & 0x3F);
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < utf8.size()) {
                cp = ((c & 0x07) << 18) | ((static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 12) | ((static_cast<uint8_t>(utf8[i + 2]) & 0x3F) << 6) | (static_cast<uint8_t>(utf8[i + 3]) & 0x3F);
            }
            i += 4;
        } else {
            i += 1;
        }
        if (cp < 0x10000) {
            utf16.push_back(static_cast<uint16_t>(cp));
        } else {
            cp -= 0x10000;
            utf16.push_back(static_cast<uint16_t>(0xD800 | (cp >> 10)));
            utf16.push_back(static_cast<uint16_t>(0xDC00 | (cp & 0x3FF)));
        }
    }
    return utf16;
}

static bool ends_with(const std::string &str, const std::string &suffix) {
    if (str.length() >= suffix.length()) {
        return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
    }
    return false;
}

static bool need_compress(const std::string &fname, const std::vector<std::string> &extra_ext_list) {
    static const std::vector<std::string> default_exts = {".txt", ".xml", ".dds", ".pmg", ".set", ".raw"};
    for (const auto &ext : default_exts) {
        if (ends_with(fname, ext)) return true;
    }
    for (const auto &ext : extra_ext_list) {
        if (ends_with(fname, ext)) return true;
    }
    return false;
}

static std::pair<FileEntry, std::vector<uint8_t>> pack_file(const std::string &root_dir, const std::string &rel_path, bool compress) {
    std::filesystem::path full_p = std::filesystem::path(root_dir) / rel_path;
    std::ifstream fp(full_p, std::ios::binary | std::ios::ate);
    if (!fp) {
        throw std::runtime_error("cannot open file for packing: " + full_p.string());
    }
    std::streamsize size = fp.tellg();
    fp.seekg(0, std::ios::beg);

    std::vector<uint8_t> stm(size);
    if (size > 0) {
        fp.read(reinterpret_cast<char*>(stm.data()), size);
    }

    size_t original_size = stm.size();
    std::vector<uint8_t> raw_stm;
    uint32_t flags = 0;

    if (compress) {
        mz_ulong max_bound = mz_compressBound(original_size);
        raw_stm.resize(max_bound);
        mz_ulong dest_len = max_bound;
        int status = mz_compress2(raw_stm.data(), &dest_len, stm.data(), original_size, 5);
        if (status != MZ_OK) {
            throw std::runtime_error("compression failed for " + rel_path);
        }
        raw_stm.resize(dest_len);
        flags = FLAG_COMPRESSED;
    } else {
        raw_stm = std::move(stm);
        flags = 0;
    }

    FileEntry ent;
    ent.name = rel_path;
    ent.checksum = 0;
    ent.flags = flags;
    ent.offset = 0;
    ent.original_size = static_cast<uint32_t>(original_size);
    ent.raw_size = static_cast<uint32_t>(raw_stm.size());
    ent.key.fill(0);

    return {ent, raw_stm};
}

static void write_header(uint32_t file_cnt, const std::vector<uint8_t> &key, std::ostream &wr) {
    constexpr uint8_t IT_VERSION = 2;
    uint32_t checksum = file_cnt + static_cast<uint32_t>(IT_VERSION);
    encryption::Snow2Encoder enc_stm(key, wr);
    enc_stm.write_u32_le(checksum);
    enc_stm.write_u8(IT_VERSION);
    enc_stm.write_u32_le(file_cnt);
    enc_stm.flush();
}

static void write_entries(const std::vector<FileEntry> &entries, const std::vector<uint8_t> &key, std::ostream &wr) {
    encryption::Snow2Encoder enc_stm(key, wr);
    for (const auto &ent : entries) {
        auto u16_str = to_utf16(ent.name);
        enc_stm.write_u32_le(static_cast<uint32_t>(u16_str.size()));
        enc_stm.write(reinterpret_cast<const uint8_t*>(u16_str.data()), u16_str.size() * 2);
        enc_stm.write_u32_le(ent.checksum);
        enc_stm.write_u32_le(ent.flags);
        enc_stm.write_u32_le(ent.offset);
        enc_stm.write_u32_le(ent.original_size);
        enc_stm.write_u32_le(ent.raw_size);
        enc_stm.write(ent.key.data(), 16);
    }
    enc_stm.flush();
}

static uint64_t ceil_1024(uint64_t v) {
    return (v + 1023) & ~static_cast<uint64_t>(1023);
}

void run_pack(const std::string &input_folder, const std::string &output_fname, const std::vector<std::string> &compress_ext) {
    std::vector<std::string> file_names;
    std::filesystem::path root(input_folder);

    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_directory()) {
            auto rel = std::filesystem::relative(entry.path(), root);
            std::string rel_str = rel.generic_string(); // Uses forward slashes
            file_names.push_back(rel_str);
        }
    }

    size_t entries_size = 0;
    for (const auto &s : file_names) {
        entries_size += to_utf16(s).size() * 2 + 40;
    }

    std::string final_file_name = get_final_file_name(output_fname);
    size_t header_off = encryption::gen_header_offset(final_file_name);
    size_t entries_off = encryption::gen_entries_offset(final_file_name);
    auto header_key = encryption::gen_header_key(final_file_name);
    auto entries_key = encryption::gen_entries_key(final_file_name);

    std::ofstream stm(output_fname, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!stm) {
        throw std::runtime_error("cannot open output file: " + output_fname);
    }

    uint64_t start_content_off = ceil_1024(header_off + entries_off + entries_size);
    uint64_t content_off = start_content_off;
    std::vector<FileEntry> entries;
    entries.reserve(file_names.size());

    for (const auto &name : file_names) {
        auto [ent, content] = pack_file(input_folder, name, need_compress(name, compress_ext));
        stm.seekp(static_cast<std::streamoff>(content_off), std::ios::beg);
        stm.write(reinterpret_cast<const char*>(content.data()), content.size());
        ent.offset = static_cast<uint32_t>((content_off - start_content_off) / 1024);
        ent.checksum = ent.offset + ent.raw_size + ent.original_size + ent.flags;
        content_off = ceil_1024(content_off + ent.raw_size);
        entries.push_back(ent);
    }

    stm.seekp(static_cast<std::streamoff>(header_off + entries_off), std::ios::beg);
    write_entries(entries, entries_key, stm);

    stm.seekp(static_cast<std::streamoff>(header_off), std::ios::beg);
    write_header(static_cast<uint32_t>(entries.size()), header_key, stm);
}
