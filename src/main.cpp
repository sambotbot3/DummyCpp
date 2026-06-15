#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::string input_path;
  std::string output_path;
};

void print_help(std::ostream &out) {
  out << "dpp - DummyCpp C++ to C transpiler\n"
      << "\n"
      << "Usage:\n"
      << "  dpp <input.cpp> -o <output.c>\n"
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

std::string trim(const std::string &value) {
  const std::string whitespace = " \t\r\n";
  const std::size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos) {
    return "";
  }
  const std::size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

bool starts_with(const std::string &value, const std::string &prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool is_string_literal(const std::string &value) {
  return value.size() >= 2 && value.front() == '"' && value.back() == '"';
}

std::string leading_indent(const std::string &line) {
  const std::size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return line;
  }
  return line.substr(0, first);
}

std::vector<std::string> split_cout_chain(const std::string &chain) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < chain.size()) {
    const std::size_t pos = chain.find("<<", start);
    if (pos == std::string::npos) {
      parts.push_back(trim(chain.substr(start)));
      break;
    }
    parts.push_back(trim(chain.substr(start, pos - start)));
    start = pos + 2;
  }
  return parts;
}

std::string lower_cout_line(const std::string &line, bool &used_stdio) {
  const std::string stripped = trim(line);
  const std::string indent = leading_indent(line);
  std::string chain;

  if (starts_with(stripped, "std::cout")) {
    chain = trim(stripped.substr(std::string("std::cout").size()));
  } else if (starts_with(stripped, "cout")) {
    chain = trim(stripped.substr(std::string("cout").size()));
  } else {
    return line;
  }

  if (!starts_with(chain, "<<") || chain.back() != ';') {
    return line;
  }

  used_stdio = true;
  chain = trim(chain.substr(2, chain.size() - 3));
  std::ostringstream out;
  const std::vector<std::string> parts = split_cout_chain(chain);
  for (const std::string &part : parts) {
    if (part.empty()) {
      continue;
    }
    if (part == "std::endl" || part == "endl") {
      out << indent << "printf(\"\\n\");\n";
    } else if (is_string_literal(part)) {
      out << indent << "printf(\"%s\", " << part << ");\n";
    } else {
      out << indent << "printf(\"%d\", " << part << ");\n";
    }
  }

  std::string lowered = out.str();
  if (!lowered.empty() && lowered.back() == '\n') {
    lowered.pop_back();
  }
  return lowered;
}

std::string lower_iostreams(const std::string &source, bool &used_stdio) {
  std::istringstream in(source);
  std::ostringstream out;
  std::string line;
  while (std::getline(in, line)) {
    const std::string stripped = trim(line);
    if (stripped == "#include <iostream>" || stripped == "using namespace std;") {
      used_stdio = true;
      continue;
    }
    out << lower_cout_line(line, used_stdio) << '\n';
  }
  return out.str();
}

std::string lower_structs(const std::string &source) {
  const std::regex struct_re(R"(struct\s+([A-Za-z_]\w*)\s*\{([^}]*)\}\s*;)");
  return std::regex_replace(source, struct_re, "typedef struct $1 {$2} $1;");
}

std::string lower_main_signature(const std::string &source) {
  const std::regex main_re(R"(\bint\s+main\s*\(\s*\))");
  return std::regex_replace(source, main_re, "int main(void)");
}

std::string lower_aggregate_initializers(const std::string &source) {
  const std::regex aggregate_re(
      R"(\b([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*\{([^{};]*)\}\s*;)");
  return std::regex_replace(source, aggregate_re, "$1 $2 = ($1){$3};");
}

std::string transpile_bootstrap_subset(const std::string &source) {
  std::string out = source;
  bool used_stdio = false;
  out = lower_iostreams(out, used_stdio);
  out = lower_structs(out);
  out = lower_main_signature(out);
  out = lower_aggregate_initializers(out);

  const std::string includes = used_stdio ? "#include <stdio.h>\n\n" : "";

  return "/* Generated by DummyCpp/dpp bootstrap transpiler. */\n"
         "/* Supported input is intentionally tiny; see docs/plan.md. */\n\n" +
         includes + out;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const Options opts = parse_args(argc, argv);
    const std::string source = read_file(opts.input_path);
    write_file(opts.output_path, transpile_bootstrap_subset(source));
    return 0;
  } catch (const std::exception &err) {
    std::cerr << "dpp: error: " << err.what() << "\n\n";
    print_help(std::cerr);
    return 1;
  }
}
