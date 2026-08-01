#include "encryption.hpp"
#include <cstring>
#include <algorithm>
#include <stdexcept>

extern "C" {
    void c_snow2_loadkey(uint32_t *state_table, const uint8_t *key);
    void c_snow2_generate_keystream(uint32_t *state_table, uint32_t *stream);
}

namespace encryption {

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

static std::string to_lower_ascii(std::string s) {
    for (char &c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }
    }
    return s;
}

std::vector<uint8_t> gen_header_key(const std::string &name) {
    std::string s = to_lower_ascii(name) + "@6QeTuOaDgJlZcBm#9";
    auto input = to_utf16(s);
    std::vector<uint8_t> key(128);
    for (size_t i = 0; i < 128; ++i) {
        uint16_t val = input[i % input.size()] + static_cast<uint16_t>(i);
        key[i] = static_cast<uint8_t>(val);
    }
    return key;
}

size_t gen_header_offset(const std::string &name) {
    auto input = to_utf16(to_lower_ascii(name));
    size_t sum = 0;
    for (auto c : input) {
        sum += c;
    }
    return sum % 312 + 30;
}

std::vector<uint8_t> gen_entries_key(const std::string &name) {
    std::string s = to_lower_ascii(name) + "@6QeTuOaDgJlZcBm#9";
    auto input = to_utf16(s);
    size_t len = input.size();
    std::vector<uint8_t> key(128);
    for (size_t i = 0; i < 128; ++i) {
        size_t term = (i % 3 + 2) * static_cast<size_t>(input[len - 1 - i % len]);
        size_t val = i + term;
        key[i] = static_cast<uint8_t>(val);
    }
    return key;
}

size_t gen_entries_offset(const std::string &name) {
    auto input = to_utf16(to_lower_ascii(name));
    size_t r = 0;
    for (auto c : input) {
        r += static_cast<size_t>(c) * 3;
    }
    return r % 212 + 42;
}

std::vector<uint8_t> gen_file_key(const std::string &name, const uint8_t key2[16]) {
    auto input = to_utf16(name);
    std::vector<uint8_t> key(128);
    for (size_t i = 0; i < 128; ++i) {
        uint8_t k2 = key2[i % 16];
        uint8_t sub = static_cast<uint8_t>(i / 5 * 5);
        uint8_t step1 = static_cast<uint8_t>(k2 - sub);
        uint8_t step2 = static_cast<uint8_t>(step1 + 2);
        uint8_t factor8 = static_cast<uint8_t>(step2 + static_cast<uint8_t>(i));
        uint16_t factor16 = static_cast<uint16_t>(factor8);
        uint16_t mul = static_cast<uint16_t>(input[i % input.size()] * factor16);
        uint16_t add = static_cast<uint16_t>(mul + static_cast<uint16_t>(i));
        key[i] = static_cast<uint8_t>(add);
    }
    return key;
}

Snow2Decoder::Snow2Decoder(const std::vector<uint8_t> &key, std::istream &rd)
    : cur_index(0), rd(rd), left_buffer_len(0) {
    std::memset(state_table, 0, sizeof(state_table));
    std::memset(keystream, 0, sizeof(keystream));
    std::memset(left_buffer, 0, sizeof(left_buffer));
    c_snow2_loadkey(state_table, key.data());
    generate_keystream();
}

void Snow2Decoder::generate_keystream() {
    c_snow2_generate_keystream(state_table, keystream);
}

bool Snow2Decoder::read_exact(uint8_t *buf, size_t len) {
    if (len == 0) return true;

    size_t copy_left = std::min(len, left_buffer_len);
    if (copy_left > 0) {
        std::memcpy(buf, left_buffer, copy_left);
        left_buffer_len -= copy_left;
        if (left_buffer_len > 0) {
            std::memmove(left_buffer, left_buffer + copy_left, left_buffer_len);
        }
        buf += copy_left;
        len -= copy_left;
    }

    if (len == 0) return true;

    size_t dec_block_len = (len + 3) & ~static_cast<size_t>(3);
    std::vector<uint8_t> ori_buf(dec_block_len);
    rd.read(reinterpret_cast<char*>(ori_buf.data()), dec_block_len);
    if (rd.gcount() < static_cast<std::streamsize>(dec_block_len)) {
        return false;
    }

    std::vector<uint8_t> dec_block(dec_block_len);
    for (size_t i = 0; i < dec_block_len / 4; ++i) {
        uint32_t raw_u32;
        std::memcpy(&raw_u32, ori_buf.data() + i * 4, 4);
        uint32_t dec_u32 = raw_u32 - keystream[cur_index];
        std::memcpy(dec_block.data() + i * 4, &dec_u32, 4);

        cur_index++;
        if (cur_index >= 16) {
            generate_keystream();
            cur_index = 0;
        }
    }

    std::memcpy(buf, dec_block.data(), len);
    left_buffer_len = dec_block_len - len;
    if (left_buffer_len > 0) {
        std::memcpy(left_buffer, dec_block.data() + len, left_buffer_len);
    }
    return true;
}

uint32_t Snow2Decoder::read_u32_le() {
    uint32_t val = 0;
    if (!read_exact(reinterpret_cast<uint8_t*>(&val), 4)) {
        throw std::runtime_error("Snow2Decoder: failed to read u32");
    }
    return val;
}

uint8_t Snow2Decoder::read_u8() {
    uint8_t val = 0;
    if (!read_exact(&val, 1)) {
        throw std::runtime_error("Snow2Decoder: failed to read u8");
    }
    return val;
}

Snow2Encoder::Snow2Encoder(const std::vector<uint8_t> &key, std::ostream &wr)
    : cur_index(0), wr(wr), left_buffer_len(0) {
    std::memset(state_table, 0, sizeof(state_table));
    std::memset(keystream, 0, sizeof(keystream));
    std::memset(left_buffer, 0, sizeof(left_buffer));
    c_snow2_loadkey(state_table, key.data());
    generate_keystream();
}

Snow2Encoder::~Snow2Encoder() {
    try {
        end_encoding();
        flush();
    } catch (...) {}
}

void Snow2Encoder::generate_keystream() {
    c_snow2_generate_keystream(state_table, keystream);
}

void Snow2Encoder::end_encoding() {
    if (left_buffer_len != 0) {
        size_t len = left_buffer_len;
        left_buffer_len = 0;
        uint8_t temp[4] = {0};
        std::memcpy(temp, left_buffer, len);
        write(temp, 4);
    }
}

bool Snow2Encoder::write(const uint8_t *buf, size_t len) {
    size_t need_writing_len = len + left_buffer_len;
    size_t enc_block_len = need_writing_len & ~static_cast<size_t>(3);

    std::vector<uint8_t> temp_buf(need_writing_len);
    if (left_buffer_len > 0) {
        std::memcpy(temp_buf.data(), left_buffer, left_buffer_len);
    }
    if (len > 0) {
        std::memcpy(temp_buf.data() + left_buffer_len, buf, len);
    }

    if (enc_block_len > 0) {
        std::vector<uint8_t> enc_block(enc_block_len);
        for (size_t i = 0; i < enc_block_len / 4; ++i) {
            uint32_t raw_u32;
            std::memcpy(&raw_u32, temp_buf.data() + i * 4, 4);
            uint32_t enc_u32 = raw_u32 + keystream[cur_index];
            std::memcpy(enc_block.data() + i * 4, &enc_u32, 4);

            cur_index++;
            if (cur_index >= 16) {
                generate_keystream();
                cur_index = 0;
            }
        }
        wr.write(reinterpret_cast<const char*>(enc_block.data()), enc_block_len);
    }

    left_buffer_len = need_writing_len - enc_block_len;
    if (left_buffer_len > 0) {
        std::memcpy(left_buffer, temp_buf.data() + enc_block_len, left_buffer_len);
    }
    return !wr.fail();
}

bool Snow2Encoder::write_u32_le(uint32_t val) {
    return write(reinterpret_cast<const uint8_t*>(&val), 4);
}

bool Snow2Encoder::write_u8(uint8_t val) {
    return write(&val, 1);
}

void Snow2Encoder::flush() {
    wr.flush();
}

} // namespace encryption
