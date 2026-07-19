#include "lexer.h"
#include "parser.h"
#include "generator.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

static std::string derive_header_path(const std::string& filePath, const std::string& outputDir = "") {
    std::string baseName;
    auto lastSlash = filePath.find_last_of('/');
    baseName = (lastSlash != std::string::npos)
        ? filePath.substr(lastSlash + 1)
        : filePath;
    auto dotPos = baseName.find_last_of('.');
    if (dotPos != std::string::npos)
        baseName = baseName.substr(0, dotPos);
    std::string dir = outputDir;
    if (!dir.empty() && dir.back() != '/')
        dir += '/';
    return dir + baseName + ".h";
}

static bool is_c_file(const fs::path& path) {
    return path.extension() == ".c" || path.extension() == ".C";
}

static bool matches_pattern(const std::string& filename, const std::string& pattern) {
    if (pattern.empty()) return false;
    if (pattern == "*") return true;
    if (pattern.find('*') == std::string::npos) return filename == pattern;
    auto star_pos = pattern.find('*');
    std::string prefix = pattern.substr(0, star_pos);
    std::string suffix = pattern.substr(star_pos + 1);
    if (!prefix.empty() && filename.substr(0, prefix.size()) != prefix) return false;
    if (!suffix.empty() && filename.substr(filename.size() - suffix.size()) != suffix) return false;
    return true;
}

static bool is_excluded(const fs::path& path, const std::vector<std::string>& excludes) {
    std::string filename = path.filename().string();
    for (const auto& pattern : excludes) {
        if (matches_pattern(filename, pattern)) return true;
    }
    return false;
}

static int process_file(const std::string& fileName, const Generator::FormatOptions& opts, const std::string& outputDir = "", const std::vector<std::string>& excludes = {}) {
    if (is_excluded(fs::path(fileName), excludes)) return 0;

    std::vector<std::string> lines = Lexer::read_file(fileName);
    if (lines.empty()) {
        std::cerr << "Error: no lines read from " << fileName << std::endl;
        return 1;
    }

    std::vector<std::vector<Lexer::Token>> line_tokens;
    for (size_t i = 0; i < lines.size(); i++) {
        line_tokens.push_back(Lexer::tokenise_line(lines[i], i + 1));
    }

    std::vector<Lexer::Token> tokens = Parser::flatten_tokens(line_tokens);
    std::vector<Lexer::Declaration> decls = Parser::parse(tokens);

    std::string headerPath = derive_header_path(fileName, outputDir);
    Generator::generate_header(headerPath, decls, opts);
    return 0;
}

static int process_directory(const fs::path& dir, bool recursive, const Generator::FormatOptions& opts, const std::string& outputDir = "", const std::vector<std::string>& excludes = {}) {
    int exit_code = 0;
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
            if (is_excluded(entry.path(), excludes)) continue;
            if (process_file(entry.path().string(), opts, outputDir, excludes) != 0)
                exit_code = 1;
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
            if (is_excluded(entry.path(), excludes)) continue;
            if (process_file(entry.path().string(), opts, outputDir, excludes) != 0)
                exit_code = 1;
        }
    }
    return exit_code;
}

static std::map<fs::path, fs::file_time_type> scan_files(const fs::path& dir, bool recursive, const std::vector<std::string>& excludes = {}) {
    std::map<fs::path, fs::file_time_type> timestamps;
    auto scan = [&](auto it) {
        for (const auto& entry : it) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
            if (is_excluded(entry.path(), excludes)) continue;
            timestamps[entry.path()] = entry.last_write_time();
        }
    };
    if (recursive) {
        scan(fs::recursive_directory_iterator(dir));
    } else {
        scan(fs::directory_iterator(dir));
    }
    return timestamps;
}

static void watch_directory(const fs::path& dir, bool recursive, const Generator::FormatOptions& opts, const std::string& outputDir = "", const std::vector<std::string>& excludes = {}) {
    std::cerr << "Watching " << dir.string() << " for changes (Ctrl+C to stop)..." << std::endl;

    auto prev = scan_files(dir, recursive, excludes);
    for (const auto& [path, _] : prev) {
        process_file(path.string(), opts, outputDir, excludes);
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto curr = scan_files(dir, recursive, excludes);

        for (const auto& [path, mtime] : curr) {
            auto it = prev.find(path);
            if (it == prev.end() || it->second != mtime) {
                std::cerr << "Changed: " << path.string() << std::endl;
                process_file(path.string(), opts, outputDir, excludes);
            }
        }

        prev = std::move(curr);
    }
}

static void print_usage() {
    std::cerr << "Usage: copa [-r] [--watch] [--pragma-once] [--indent <2|4|8>] [--target <dir>] [--exclude <pattern>] <file.c | directory>" << std::endl;
}

static void print_help() {
    std::cout << "copa - lightweight C header file generator" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: copa [options] <file.c | directory>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help              Show this help message" << std::endl;
    std::cout << "  -r                      Recursively process directories" << std::endl;
    std::cout << "  --watch                 Watch directory for changes (requires directory)" << std::endl;
    std::cout << "  --pragma-once           Use #pragma once instead of include guards" << std::endl;
    std::cout << "  --indent <2|4|8>        Set indentation width (default: 4)" << std::endl;
    std::cout << "  --target <dir>          Output directory for generated headers" << std::endl;
    std::cout << "  --exclude <pattern>     Exclude files matching pattern (can be repeated)" << std::endl;
    std::cout << std::endl;
    std::cout << "Exclude patterns:" << std::endl;
    std::cout << "  Match against filenames. Supports '*' as a wildcard." << std::endl;
    std::cout << "  Examples:" << std::endl;
    std::cout << "    --exclude test.c           Exclude exactly 'test.c'" << std::endl;
    std::cout << "    --exclude 'test*'          Exclude files starting with 'test'" << std::endl;
    std::cout << "    --exclude '*_test.c'       Exclude files ending with '_test.c'" << std::endl;
    std::cout << "    --exclude '*.c' --exclude '*.C'   Exclude all C files (useless but possible)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  copa src/main.c                     Generate header for a single file" << std::endl;
    std::cout << "  copa -r src/                        Generate headers for all .c files in src/" << std::endl;
    std::cout << "  copa -r --exclude test_* src/       Exclude test files from generation" << std::endl;
    std::cout << "  copa -r --target include/ src/      Output headers to include/ directory" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    bool recursive = false;
    bool watch = false;
    std::string outputDir;
    std::vector<std::string> excludes;
    Generator::FormatOptions opts;
    int arg_start = 1;

    while (arg_start < argc) {
        std::string arg = argv[arg_start];
        if (arg == "-h" || arg == "--help") {
            print_help();
            return 0;
        } else if (arg == "-r") {
            recursive = true;
            arg_start++;
        } else if (arg == "--watch") {
            watch = true;
            arg_start++;
        } else if (arg == "--pragma-once") {
            opts.use_pragma_once = true;
            arg_start++;
        } else if (arg == "--indent") {
            if (arg_start + 1 >= argc) {
                std::cerr << "Error: --indent requires a value (2, 4, or 8)" << std::endl;
                return 1;
            }
            arg_start++;
            std::string val = argv[arg_start];
            if (val == "2") opts.indent_width = 2;
            else if (val == "4") opts.indent_width = 4;
            else if (val == "8") opts.indent_width = 8;
            else {
                std::cerr << "Error: --indent must be 2, 4, or 8" << std::endl;
                return 1;
            }
            arg_start++;
        } else if (arg == "--target") {
            if (arg_start + 1 >= argc) {
                std::cerr << "Error: --target requires a directory path" << std::endl;
                return 1;
            }
            arg_start++;
            outputDir = argv[arg_start];
            arg_start++;
        } else if (arg == "--exclude") {
            if (arg_start + 1 >= argc) {
                std::cerr << "Error: --exclude requires a pattern" << std::endl;
                return 1;
            }
            arg_start++;
            excludes.push_back(argv[arg_start]);
            arg_start++;
        } else {
            break;
        }
    }

    if (arg_start >= argc) {
        print_usage();
        return 1;
    }

    if (!outputDir.empty()) {
        std::error_code ec;
        fs::create_directories(outputDir, ec);
        if (ec) {
            std::cerr << "Error: cannot create output directory " << outputDir << ": " << ec.message() << std::endl;
            return 1;
        }
    }

    std::string sourcePath = argv[arg_start];
    fs::path sourcePathObj(sourcePath);

    if (!fs::exists(sourcePathObj)) {
        std::cerr << "Error: " << sourcePath << " does not exist" << std::endl;
        return 1;
    }

    if (fs::is_directory(sourcePathObj)) {
        if (watch) {
            watch_directory(sourcePathObj, recursive, opts, outputDir, excludes);
            return 0;
        }
        return process_directory(sourcePathObj, recursive, opts, outputDir, excludes);
    }

    if (watch) {
        std::cerr << "Error: --watch requires a directory, not a file" << std::endl;
        return 1;
    }

    return process_file(sourcePath, opts, outputDir, excludes);
}
