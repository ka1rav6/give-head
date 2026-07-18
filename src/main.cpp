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

namespace fs = std::filesystem;

static std::string derive_header_path(const std::string& filePath) {
    auto lastSlash = filePath.find_last_of('/');
    std::string dir = (lastSlash != std::string::npos)
        ? filePath.substr(0, lastSlash + 1)
        : "";
    std::string baseName = (lastSlash != std::string::npos)
        ? filePath.substr(lastSlash + 1)
        : filePath;
    auto dotPos = baseName.find_last_of('.');
    if (dotPos != std::string::npos)
        baseName = baseName.substr(0, dotPos);
    return dir + baseName + ".h";
}

static bool is_c_file(const fs::path& path) {
    return path.extension() == ".c" || path.extension() == ".C";
}

static int process_file(const std::string& fileName) {
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

    std::string headerPath = derive_header_path(fileName);
    Generator::generate_header(headerPath, decls);
    return 0;
}

static int process_directory(const fs::path& dir, bool recursive) {
    int exit_code = 0;
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
            if (process_file(entry.path().string()) != 0)
                exit_code = 1;
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
            if (process_file(entry.path().string()) != 0)
                exit_code = 1;
        }
    }
    return exit_code;
}

static std::map<fs::path, fs::file_time_type> scan_files(const fs::path& dir, bool recursive) {
    std::map<fs::path, fs::file_time_type> timestamps;
    auto scan = [&](auto it) {
        for (const auto& entry : it) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
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

static void watch_directory(const fs::path& dir, bool recursive) {
    std::cerr << "Watching " << dir.string() << " for changes (Ctrl+C to stop)..." << std::endl;

    auto prev = scan_files(dir, recursive);
    for (const auto& [path, _] : prev) {
        process_file(path.string());
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto curr = scan_files(dir, recursive);

        for (const auto& [path, mtime] : curr) {
            auto it = prev.find(path);
            if (it == prev.end() || it->second != mtime) {
                std::cerr << "Changed: " << path.string() << std::endl;
                process_file(path.string());
            }
        }

        prev = std::move(curr);
    }
}

static void print_usage() {
    std::cerr << "Usage: copa [-r] [--watch] <file.c | directory>" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    bool recursive = false;
    bool watch = false;
    int arg_start = 1;

    while (arg_start < argc) {
        std::string arg = argv[arg_start];
        if (arg == "-r") {
            recursive = true;
            arg_start++;
        } else if (arg == "--watch") {
            watch = true;
            arg_start++;
        } else {
            break;
        }
    }

    if (arg_start >= argc) {
        print_usage();
        return 1;
    }

    std::string target = argv[arg_start];
    fs::path targetPath(target);

    if (!fs::exists(targetPath)) {
        std::cerr << "Error: " << target << " does not exist" << std::endl;
        return 1;
    }

    if (fs::is_directory(targetPath)) {
        if (watch) {
            watch_directory(targetPath, recursive);
            return 0;
        }
        return process_directory(targetPath, recursive);
    }

    if (watch) {
        std::cerr << "Error: --watch requires a directory, not a file" << std::endl;
        return 1;
    }

    return process_file(target);
}
