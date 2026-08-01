#include "common.hpp"
#include <filesystem>
#include <stdexcept>
#include <numeric>

static std::string utf16_to_utf8(const std::vector<uint16_t> &u16) {
    std::string utf8;
    for (size_t i = 0; i < u16.size(); ++i) {
        uint32_t cp = u16[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < u16.size()) {
            uint32_t trail = u16[i + 1];
            if (trail >= 0xDC00 && trail <= 0xDFFF) {
                cp = 0x10000 + (((cp & 0x3FF) << 10) | (trail & 0x3FF));
                i++;
            }
        }
        if (cp < 0x80) {
            utf8.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            utf8.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            utf8.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            utf8.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return utf8;
}

FileHeader FileHeader::read_from(encryption::Snow2Decoder &decoder) {
    FileHeader hdr;
    hdr.checksum = decoder.read_u32_le();
    hdr.version = decoder.read_u8();
    hdr.file_cnt = decoder.read_u32_le();
    return hdr;
}

FileEntry FileEntry::read_from(encryption::Snow2Decoder &decoder) {
    FileEntry ent;
    uint32_t str_len = decoder.read_u32_le();
    std::vector<uint16_t> fname_u16(str_len);
    if (!decoder.read_exact(reinterpret_cast<uint8_t*>(fname_u16.data()), str_len * 2)) {
        throw std::runtime_error("FileEntry: failed to read filename string");
    }
    ent.name = utf16_to_utf8(fname_u16);
    ent.checksum = decoder.read_u32_le();
    ent.flags = decoder.read_u32_le();
    ent.offset = decoder.read_u32_le();
    ent.original_size = decoder.read_u32_le();
    ent.raw_size = decoder.read_u32_le();
    if (!decoder.read_exact(ent.key.data(), 16)) {
        throw std::runtime_error("FileEntry: failed to read key");
    }
    return ent;
}

std::string get_final_file_name(const std::string &fname) {
    std::filesystem::path p(fname);
    if (!p.has_filename()) {
        throw std::runtime_error("not a valid file path: " + fname);
    }
    return p.filename().string();
}

FileHeader read_header(const std::string &fname, std::istream &rd) {
    auto key = encryption::gen_header_key(fname);
    auto offset = encryption::gen_header_offset(fname);
    rd.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    encryption::Snow2Decoder dec(key, rd);
    return FileHeader::read_from(dec);
}

void validate_header(const FileHeader &hdr) {
    if (static_cast<uint32_t>(hdr.version) + hdr.file_cnt != hdr.checksum) {
        throw std::runtime_error("header checksum wrong");
    }
}

std::vector<FileEntry> read_entries(const std::string &fname, const FileHeader &header, std::istream &rd) {
    auto key = encryption::gen_entries_key(fname);
    auto offset_header = encryption::gen_header_offset(fname);
    auto offset_entry = encryption::gen_entries_offset(fname);
    rd.seekg(static_cast<std::streamoff>(offset_header + offset_entry), std::ios::beg);

    encryption::Snow2Decoder dec(key, rd);
    std::vector<FileEntry> entries;
    entries.reserve(header.file_cnt);
    for (uint32_t i = 0; i < header.file_cnt; ++i) {
        entries.push_back(FileEntry::read_from(dec));
    }
    return entries;
}

void validate_entries(const std::vector<FileEntry> &entries) {
    for (const auto &ent : entries) {
        uint32_t key_sum = 0;
        for (uint8_t k : ent.key) {
            key_sum += k;
        }
        if (ent.flags + ent.offset + ent.original_size + ent.raw_size + key_sum != ent.checksum) {
            throw std::runtime_error("entry checksum wrong, file name: " + ent.name);
        }
    }
}
