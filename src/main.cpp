#include "common.hpp"
#include "extract.hpp"
#include "list.hpp"
#include "pack.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <cstring>

static void print_help() {
    std::cout << "Mabinogi pack utilities 2 v1.3.1\n"
              << "regomne <fallingsunz@gmail.com>\n\n"
              << "USAGE:\n"
              << "    mabi-pack2 [SUBCOMMAND]\n\n"
              << "OPTIONS:\n"
              << "    -h, --help       Print help information\n"
              << "    -V, --version    Print version information\n\n"
              << "SUBCOMMANDS:\n"
              << "    extract    Extract a .it pack\n"
              << "    list       Output the file list of a .it pack\n"
              << "    pack       Create a .it pack\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    std::string command = argv[1];
    if (command == "-h" || command == "--help") {
        print_help();
        return 0;
    }
    if (command == "-V" || command == "--version") {
        std::cout << "v1.3.1\n";
        return 0;
    }

    try {
        if (command == "extract") {
            std::string input;
            std::string output;
            std::vector<std::string> filters;

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
                    input = argv[++i];
                } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                    output = argv[++i];
                } else if ((arg == "-f" || arg == "--filter") && i + 1 < argc) {
                    filters.push_back(argv[++i]);
                } else if (arg == "-c" || arg == "--check_additional") {
                    std::cout << "WARNING: --check_additional has been deprecated\n";
                }
            }

            if (input.empty() || output.empty()) {
                std::cerr << "Error: extract command requires -i/--input and -o/--output\n";
                return 1;
            }
            run_extract(input, output, filters);
        } else if (command == "list") {
            std::string input;
            std::optional<std::string> output;

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
                    input = argv[++i];
                } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                    output = argv[++i];
                } else if (arg == "-c" || arg == "--check_additional") {
                    std::cout << "WARNING: --check_additional has been deprecated\n";
                }
            }

            if (input.empty()) {
                std::cerr << "Error: list command requires -i/--input\n";
                return 1;
            }
            run_list(input, output);
        } else if (command == "pack") {
            std::string input;
            std::string output;
            std::vector<std::string> compress_ext;

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
                    input = argv[++i];
                } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                    output = argv[++i];
                } else if ((arg == "-f" || arg == "--compress-format") && i + 1 < argc) {
                    compress_ext.push_back(argv[++i]);
                } else if (arg == "-a" || arg == "--additional_data") {
                    std::cout << "WARNING: --additional_data has been deprecated\n";
                }
            }

            if (input.empty() || output.empty()) {
                std::cerr << "Error: pack command requires -i/--input and -o/--output\n";
                return 1;
            }
            run_pack(input, output, compress_ext);
        } else {
            std::cerr << "please select a subcommand (type --help to get details)\n";
            return 1;
        }
    } catch (const std::exception &e) {
        std::cerr << "Err: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
