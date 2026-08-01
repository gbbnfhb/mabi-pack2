#ifndef COMMON_HPP
#define COMMON_HPP

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <iostream>
#include "encryption.hpp"

struct FileHeader {
    uint32_t checksum;
    uint8_t version;
    uint32_t file_cnt;

    static FileHeader read_from(encryption::Snow2Decoder &decoder);
};

struct FileEntry {
    std::string name;
    uint32_t checksum;
    uint32_t flags;
    uint32_t offset;
    uint32_t original_size;
    uint32_t raw_size;
    std::array<uint8_t, 16> key;

    static FileEntry read_from(encryption::Snow2Decoder &decoder);
};

constexpr uint32_t FLAG_COMPRESSED = 1;
constexpr uint32_t FLAG_ALL_ENCRYPTED = 2;
constexpr uint32_t FLAG_HEAD_ENCRYPTED = 4;

std::string get_final_file_name(const std::string &fname);
FileHeader read_header(const std::string &fname, std::istream &rd);
void validate_header(const FileHeader &hdr);
std::vector<FileEntry> read_entries(const std::string &fname, const FileHeader &header, std::istream &rd);
void validate_entries(const std::vector<FileEntry> &entries);

#endif // COMMON_HPP
