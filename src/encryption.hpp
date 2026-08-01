#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>

namespace encryption {

std::vector<uint8_t> gen_header_key(const std::string &name);
size_t gen_header_offset(const std::string &name);

std::vector<uint8_t> gen_entries_key(const std::string &name);
size_t gen_entries_offset(const std::string &name);

std::vector<uint8_t> gen_file_key(const std::string &name, const uint8_t key2[16]);

class Snow2Decoder {
public:
    Snow2Decoder(const std::vector<uint8_t> &key, std::istream &rd);
    bool read_exact(uint8_t *buf, size_t len);
    uint32_t read_u32_le();
    uint8_t read_u8();

private:
    void generate_keystream();

    uint32_t state_table[18];
    uint32_t keystream[16];
    size_t cur_index;
    std::istream &rd;

    uint8_t left_buffer[4];
    size_t left_buffer_len;
};

class Snow2Encoder {
public:
    Snow2Encoder(const std::vector<uint8_t> &key, std::ostream &wr);
    ~Snow2Encoder();

    bool write(const uint8_t *buf, size_t len);
    bool write_u32_le(uint32_t val);
    bool write_u8(uint8_t val);
    void flush();

private:
    void end_encoding();
    void generate_keystream();

    uint32_t state_table[18];
    uint32_t keystream[16];
    size_t cur_index;
    std::ostream &wr;

    uint8_t left_buffer[4];
    size_t left_buffer_len;
};

} // namespace encryption

#endif // ENCRYPTION_HPP
