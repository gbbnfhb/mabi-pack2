#include "encryption.hpp"
#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>

void test_header_offset() {
    size_t off = encryption::gen_header_offset("data_00000.it");
    assert(off == 0x6a);
    std::cout << "test header_offset ... ok\n";
}

void test_header_key() {
    auto key = encryption::gen_header_key("data_00000.it");
    uint8_t expected[16] = {
        0x64, 0x62, 0x76, 0x64, 0x63, 0x35, 0x36, 0x37, 0x38, 0x39, 0x38, 0x74, 0x80, 0x4d, 0x44, 0x60
    };
    for (size_t i = 0; i < 16; ++i) {
        assert(key[i] == expected[i]);
    }
    std::cout << "test header_key ... ok\n";
}

void test_entries_offset() {
    size_t off = encryption::gen_entries_offset("data_00000.it");
    assert(off == 0x6e);
    std::cout << "test entries_offset ... ok\n";
}

void test_entries_key() {
    auto key = encryption::gen_entries_key("data_00000.it");
    uint8_t expected[16] = {
        0x72, 0x6a, 0xb6, 0x87, 0x2d, 0x6d, 0xde, 0xe5, 0xa4, 0x91, 0x2d, 0x47, 0xf6, 0x9, 0xa2, 0xb1
    };
    for (size_t i = 0; i < 16; ++i) {
        assert(key[i] == expected[i]);
    }
    std::cout << "test entries_key ... ok\n";
}

void test_decoder() {
    auto key = encryption::gen_header_key("data_00000.it");
    uint8_t ciphered_text[12] = {
        0x37, 0x62, 0x6D, 0x63, 0x82, 0x03, 0x09, 0xD0, 0x24, 0x73, 0xBE, 0xA9
    };
    std::string str(reinterpret_cast<char*>(ciphered_text), 12);
    std::istringstream iss(str, std::ios::binary);
    encryption::Snow2Decoder rd(key, iss);

    assert(rd.read_u32_le() == 0x4b5);
    assert(rd.read_u8() == 2);
    assert(rd.read_u32_le() == 0x4b3);
    std::cout << "test decoder ... ok\n";
}

void test_decoder2() {
    auto key = encryption::gen_entries_key("data_00000.it");
    uint8_t ciphered_text[32] = {
        0x8B, 0xD6, 0xBF, 0xE6, 0xAD, 0x7E, 0xE9, 0xE7, 0x64, 0x95, 0xF0, 0xBB, 0x08, 0x0E,
        0x89, 0x2D, 0xEE, 0x7A, 0x1E, 0x93, 0x16, 0x2B, 0x92, 0xCC, 0x20, 0x43, 0x2D, 0xE3,
        0x69, 0x1A, 0x65, 0xB3
    };
    std::string str(reinterpret_cast<char*>(ciphered_text), 32);
    std::istringstream iss(str, std::ios::binary);
    encryption::Snow2Decoder rd(key, iss);

    assert(rd.read_u32_le() == 31);
    uint8_t buf[28] = {0};
    assert(rd.read_exact(buf, 28));

    uint8_t expected[28] = {
        0x64, 0x0, 0x61, 0x0, 0x74, 0x0, 0x61, 0x0, 0x2f, 0x0, 0x63, 0x0, 0x6f, 0x0, 0x6c,
        0x0, 0x6f, 0x0, 0x72, 0x0, 0x2f, 0x0, 0x62, 0x0, 0x65, 0x0, 0x73, 0x0
    };
    for (size_t i = 0; i < 28; ++i) {
        assert(buf[i] == expected[i]);
    }
    std::cout << "test decoder2 ... ok\n";
}

int main() {
    test_header_offset();
    test_header_key();
    test_entries_offset();
    test_entries_key();
    test_decoder();
    test_decoder2();
    std::cout << "All unit tests passed successfully!\n";
    return 0;
}
