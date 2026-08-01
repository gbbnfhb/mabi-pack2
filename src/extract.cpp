#include "extract.hpp"
#include "common.hpp"
#include "encryption.hpp"
#include "miniz.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <stdexcept>
#include <algorithm>

static void write_file(const std::string &root_dir, const std::string &rel_path, const std::vector<uint8_t> &content) {
    std::filesystem::path rel_p(rel_path);
    std::filesystem::path full_p = std::filesystem::path(root_dir) / rel_p;
    if (full_p.has_parent_path()) {
        std::filesystem::create_directories(full_p.parent_path());
    }
    std::ofstream ofs(full_p, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("cannot open output file: " + full_p.string());
    }
    ofs.write(reinterpret_cast<const char*>(content.data()), content.size());
}

static void extract_file(std::istream &stm, uint64_t start_off, const FileEntry &ent, const std::string &root_dir) {
    stm.seekg(static_cast<std::streamoff>(start_off + static_cast<uint64_t>(ent.offset) * 1024), std::ios::beg);
    std::vector<uint8_t> content(ent.raw_size);
    auto fkey = encryption::gen_file_key(ent.name, ent.key.data());

    if ((ent.flags & FLAG_ALL_ENCRYPTED) != 0) {
        encryption::Snow2Decoder dec(fkey, stm);
        if (!dec.read_exact(content.data(), content.size())) {
            throw std::runtime_error("failed to read encrypted content for " + ent.name);
        }
    } else {
        stm.read(reinterpret_cast<char*>(content.data()), content.size());
        if (stm.gcount() < static_cast<std::streamsize>(content.size())) {
            throw std::runtime_error("failed to read raw content for " + ent.name);
        }
    }

    if ((ent.flags & FLAG_HEAD_ENCRYPTED) != 0) {
        std::string raw_str(reinterpret_cast<const char*>(content.data()), content.size());
        std::istringstream iss(raw_str, std::ios::binary);
        encryption::Snow2Decoder dec(fkey, iss);
        size_t dec_len = std::min(content.size(), static_cast<size_t>(1024));
        if (!dec.read_exact(content.data(), dec_len)) {
            throw std::runtime_error("failed to decrypt head for " + ent.name);
        }
    }

    std::vector<uint8_t> final_content;
    if ((ent.flags & FLAG_COMPRESSED) != 0) {
        final_content.resize(ent.original_size);
        mz_ulong dest_len = ent.original_size;
        int status = mz_uncompress(final_content.data(), &dest_len, content.data(), content.size());
        if (status != MZ_OK || dest_len != ent.original_size) {
            throw std::runtime_error("decompress failed for " + ent.name);
        }
    } else {
        final_content = std::move(content);
    }

    write_file(root_dir, ent.name, final_content);
}

void run_extract(const std::string &fname, const std::string &output_folder, const std::vector<std::string> &filters) {
    std::ifstream rd(fname, std::ios::binary);
    if (!rd) {
        throw std::runtime_error("cannot open pack file: " + fname);
    }
    std::string final_name = get_final_file_name(fname);
    FileHeader header = read_header(final_name, rd);
    validate_header(header);

    if (header.version != 2) {
        throw std::runtime_error("header version " + std::to_string(header.version) + " not supported");
    }

    std::vector<FileEntry> entries = read_entries(final_name, header, rd);
    validate_entries(entries);

    uint64_t cur_pos = static_cast<uint64_t>(rd.tellg());
    uint64_t content_start_off = (cur_pos + 1023) & ~static_cast<uint64_t>(1023);

    std::vector<std::regex> regexes;
    for (const auto &f : filters) {
        regexes.emplace_back(f);
    }

    for (const auto &ent : entries) {
        bool match = regexes.empty();
        if (!match) {
            for (const auto &re : regexes) {
                if (std::regex_search(ent.name, re)) {
                    match = true;
                    break;
                }
            }
        }
        if (match) {
            extract_file(rd, content_start_off, ent, output_folder);
        }
    }
}
