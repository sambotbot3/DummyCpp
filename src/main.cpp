#include "dpp/transpiler.h"

#include "dpp/parser/preprocessor.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::string input_path;
  std::string output_path;
  std::vector<std::string> include_dirs;
};

void print_help(std::ostream &out) {
  out << "dpp - DummyCpp C++ to C transpiler\n"
      << "\n"
      << "Usage:\n"
      << "  dpp <input.cpp> [-I <dir>]... -o <output.c>\n"
      << "\n"
      << "Current bootstrap support:\n"
      << "  - simple struct declarations with fields\n"
      << "  - free functions\n"
      << "  - int main() -> int main(void)\n"
      << "  - aggregate initialization: Type name{...};\n"
      << "  - narrow std::cout chains with strings, int expressions, and std::endl\n";
}

Options parse_args(int argc, char **argv) {
  if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    print_help(std::cout);
    std::exit(0);
  }

  Options opts;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-o") {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing path after -o");
      }
      opts.output_path = argv[++i];
    } else if (arg == "-I") {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing path after -I");
      }
      opts.include_dirs.push_back(argv[++i]);
    } else if (arg.size() > 2 && arg[0] == '-' && arg[1] == 'I') {
      opts.include_dirs.push_back(arg.substr(2));
    } else if (opts.input_path.empty()) {
      opts.input_path = arg;
    } else {
      throw std::runtime_error("unexpected argument: " + arg);
    }
  }

  if (opts.input_path.empty() || opts.output_path.empty()) {
    throw std::runtime_error("expected input path and -o output path");
  }

  return opts;
}

std::string read_file(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("failed to open input: " + path);
  }

  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void write_file(const std::string &path, const std::string &content) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("failed to open output: " + path);
  }
  out << content;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const Options opts = parse_args(argc, argv);
    const std::string source = read_file(opts.input_path);
    const dpp::parser::PreprocessResult preprocessed =
        dpp::parser::preprocess_translation_unit_file(opts.input_path, source, opts.include_dirs);
    write_file(opts.output_path, dpp::transpile_bootstrap_subset(preprocessed.source));
    return 0;
  } catch (const std::exception &err) {
    std::cerr << "dpp: error: " << err.what() << "\n\n";
    print_help(std::cerr);
    return 1;
  }
}
