#include "lexer.h"
#include "parser.h"
#include "generator.h"
#include <iostream>
#include <fstream>
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
    std::string sourceDir = (lastSlash != std::string::npos)
        ? filePath.substr(0, lastSlash + 1)
        : "";
    baseName = (lastSlash != std::string::npos)
        ? filePath.substr(lastSlash + 1)
        : filePath;
    auto dotPos = baseName.find_last_of('.');
    if (dotPos != std::string::npos)
        baseName = baseName.substr(0, dotPos);
    // Default to the source file's own directory (matches the documented
    // --target default of "same as source"); only override it when the
    // caller explicitly passed an output directory.
    std::string dir = outputDir.empty() ? sourceDir : outputDir;
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

static bool is_removable(const Lexer::Declaration& decl) {
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Lexer::IncludeStatement>)  return true;
        else if constexpr (std::is_same_v<T, Lexer::Macro>)        return true;
        else if constexpr (std::is_same_v<T, Lexer::Typedef>)      return true;
        else if constexpr (std::is_same_v<T, Lexer::Struct>)       return true;
        else if constexpr (std::is_same_v<T, Lexer::Union>)        return true;
        else if constexpr (std::is_same_v<T, Lexer::Enum>)         return true;
        else if constexpr (std::is_same_v<T, Lexer::ExternVariable>)   return true;
        else if constexpr (std::is_same_v<T, Lexer::ForwardDeclaration>) return true;
        else if constexpr (std::is_same_v<T, Lexer::FunctionPointer>)  return true;
        else if constexpr (std::is_same_v<T, Lexer::VariableDefinition>) return true;
        else if constexpr (std::is_same_v<T, Lexer::Function>) {
            return !arg.has_body;
        }
        return false;
    }, decl);
}

static void move_definitions_to_header(
    const std::string& fileName,
    const std::string& headerPath,
    const std::vector<Lexer::DeclarationRange>& ranges)
{
    std::ifstream infile(fileName);
    if (!infile.is_open()) {
        std::cerr << "Error: cannot open " << fileName << " for reading" << std::endl;
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(infile, line))
        lines.push_back(line);
    infile.close();

    // Collect 1-indexed line numbers to remove
    std::vector<size_t> lines_to_remove;
    for (const auto& range : ranges) {
        if (!is_removable(range.decl)) continue;
        for (size_t ln = range.start_line; ln <= range.end_line; ln++)
            lines_to_remove.push_back(ln);
    }

    // Deduplicate
    std::sort(lines_to_remove.begin(), lines_to_remove.end());
    lines_to_remove.erase(
        std::unique(lines_to_remove.begin(), lines_to_remove.end()),
        lines_to_remove.end());

    // Find the include insertion point: after the last existing #include
    size_t insert_after = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string trimmed = lines[i];
        // Skip leading whitespace
        size_t first_non_space = trimmed.find_first_not_of(" \t");
        if (first_non_space != std::string::npos)
            trimmed = trimmed.substr(first_non_space);
        if (trimmed.rfind("#include", 0) == 0)
            insert_after = i + 1;
    }

    // Build new file content
    std::vector<std::string> new_lines;
    std::string include_line = "#include \"" + fs::path(headerPath).filename().string() + "\"";
    bool include_added = false;

    for (size_t i = 0; i < lines.size(); i++) {
        size_t line_num = i + 1;
        if (std::binary_search(lines_to_remove.begin(), lines_to_remove.end(), line_num))
            continue;

        new_lines.push_back(lines[i]);

        if (!include_added && i + 1 == insert_after) {
            new_lines.push_back(include_line);
            include_added = true;
        }
    }

    // If we never found an #include, insert at the top
    if (!include_added) {
        new_lines.insert(new_lines.begin(), include_line);
    }

    // Collapse consecutive blank lines into at most one
    std::vector<std::string> cleaned;
    bool prev_blank = false;
    for (const auto& l : new_lines) {
        bool blank = l.empty() ||
            (l.find_first_not_of(" \t") == std::string::npos);
        if (blank && prev_blank) continue;
        cleaned.push_back(l);
        prev_blank = blank;
    }

    // Write back
    std::ofstream outfile(fileName);
    if (!outfile.is_open()) {
        std::cerr << "Error: cannot open " << fileName << " for writing" << std::endl;
        return;
    }
    for (size_t i = 0; i < cleaned.size(); i++) {
        outfile << cleaned[i];
        if (i + 1 < cleaned.size()) outfile << "\n";
    }
    outfile.close();
    std::cout << "Moved definitions from " << fileName << " to " << headerPath << std::endl;
}

static int process_file(const std::string& fileName, const Generator::FormatOptions& opts, const std::string& outputDir = "", const std::vector<std::string>& excludes = {}, bool move = false) {
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

    std::string headerPath = derive_header_path(fileName, outputDir);

    if (move) {
        auto ranges = Parser::parse_with_ranges(tokens);
        std::vector<Lexer::Declaration> decls;
        for (const auto& r : ranges) decls.push_back(r.decl);
        Generator::generate_header(headerPath, decls, opts);
        move_definitions_to_header(fileName, headerPath, ranges);
    } else {
        std::vector<Lexer::Declaration> decls = Parser::parse(tokens);
        Generator::generate_header(headerPath, decls, opts);
    }
    return 0;
}

static int process_directory(const fs::path& dir, bool recursive, const Generator::FormatOptions& opts, const std::string& outputDir = "", const std::vector<std::string>& excludes = {}, bool move = false) {
    int exit_code = 0;
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
            if (is_excluded(entry.path(), excludes)) continue;
            if (process_file(entry.path().string(), opts, outputDir, excludes, move) != 0)
                exit_code = 1;
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (!is_c_file(entry.path())) continue;
            if (is_excluded(entry.path(), excludes)) continue;
            if (process_file(entry.path().string(), opts, outputDir, excludes, move) != 0)
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

static void watch_directory(const fs::path& dir, bool recursive, const Generator::FormatOptions& opts, const std::string& outputDir = "", const std::vector<std::string>& excludes = {}, bool move = false) {
    std::cerr << "Watching " << dir.string() << " for changes (Ctrl+C to stop)..." << std::endl;

    auto prev = scan_files(dir, recursive, excludes);
    for (const auto& [path, _] : prev) {
        process_file(path.string(), opts, outputDir, excludes, move);
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto curr = scan_files(dir, recursive, excludes);

        for (const auto& [path, mtime] : curr) {
            auto it = prev.find(path);
            if (it == prev.end() || it->second != mtime) {
                std::cerr << "Changed: " << path.string() << std::endl;
                process_file(path.string(), opts, outputDir, excludes, move);
            }
        }

        prev = std::move(curr);
    }
}

static void print_usage() {
    std::cerr << "Usage: copa [-r] [--watch] [--move] [--pragma-once] [--indent <2|4|8>] [--target <dir>] [--exclude <pattern>] <file.c | directory>" << std::endl;
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
    std::cout << "  --move                  Move definitions to header, remove from source" << std::endl;
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
    std::cout << "  copa --move src/main.c              Move definitions to header, update source" << std::endl;
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
    bool move = false;
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
        } else if (arg == "--move") {
            move = true;
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
            watch_directory(sourcePathObj, recursive, opts, outputDir, excludes, move);
            return 0;
        }
        return process_directory(sourcePathObj, recursive, opts, outputDir, excludes, move);
    }

    if (watch) {
        std::cerr << "Error: --watch requires a directory, not a file" << std::endl;
        return 1;
    }

    return process_file(sourcePath, opts, outputDir, excludes, move);
}
