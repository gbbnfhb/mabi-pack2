#include "list.hpp"
#include "common.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

void run_list(const std::string &fname, const std::optional<std::string> &output) {
    std::ifstream rd(fname, std::ios::binary);
    if (!rd) {
        throw std::runtime_error("cannot open file: " + fname);
    }

    std::string final_name = get_final_file_name(fname);
    FileHeader header = read_header(final_name, rd);
    validate_header(header);

    if (header.version != 2) {
        throw std::runtime_error("header version " + std::to_string(header.version) + " not supported");
    }

    std::vector<FileEntry> entries = read_entries(final_name, header, rd);
    validate_entries(entries);

    std::ofstream ofs;
    std::ostream *out_stream = &std::cout;

    if (output.has_value()) {
        ofs.open(output.value());
        if (!ofs) {
            throw std::runtime_error("cannot open list output file: " + output.value());
        }
        out_stream = &ofs;
    }

    for (const auto &ent : entries) {
        (*out_stream) << ent.name << "\n";
    }
}
