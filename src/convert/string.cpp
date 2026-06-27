#include "dpp/convert/string.h"
#include "dpp/convert/cleanup.h"
#include "dpp/parser/ir.h"
#include "dpp/string_utils.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace dpp::convert {
namespace {

// ── Character helpers ──────────────────────────────────────────────────────

static bool is_ident(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static bool is_word_before(const std::string &s, std::size_t pos) {
  return pos == 0 || !is_ident(s[pos - 1]);
}

static bool is_word_after(const std::string &s, std::size_t pos) {
  return pos >= s.size() || !is_ident(s[pos]);
}

// Skip horizontal whitespace starting at pos.
static std::size_t skip_ws(const std::string &s, std::size_t pos) {
  while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
  return pos;
}

// ── String-type prefix matching ────────────────────────────────────────────
// Returns 0 if no string type at pos, else the number of chars consumed
// (for "std::string" returns 11, for "string" returns 6).
static std::size_t match_str_type(const std::string &s, std::size_t pos) {
  if (pos >= s.size()) return 0;
  const auto cmp = [&](std::size_t p, const char *lit) -> bool {
    const std::size_t n = std::strlen(lit);
    return s.compare(p, n, lit) == 0;
  };
  if (cmp(pos, "std::string") && (pos + 11 >= s.size() || !is_ident(s[pos + 11])))
    return 11;
  if (cmp(pos, "string") && (pos + 6 >= s.size() || !is_ident(s[pos + 6])))
    return 6;
  return 0;
}

// ── Identifier scanning ────────────────────────────────────────────────────
// Returns the identifier starting at pos, or "" if none.
static std::string read_ident(const std::string &s, std::size_t pos) {
  if (pos >= s.size() || !is_ident(s[pos]) || std::isdigit(static_cast<unsigned char>(s[pos])))
    return "";
  std::size_t end = pos;
  while (end < s.size() && is_ident(s[end])) ++end;
  return s.substr(pos, end - pos);
}

// Returns true if position `pos` in `s` is inside a double-quoted string literal.
static bool in_string_literal(const std::string &s, std::size_t pos) {
  bool in_str = false;
  for (std::size_t i = 0; i < pos && i < s.size(); ++i) {
    if (s[i] == '\\' && in_str) { ++i; continue; }
    if (s[i] == '"') in_str = !in_str;
  }
  return in_str;
}

// Find the first occurrence of word `w` with word boundaries in `s` starting at `from`.
// Returns std::string::npos if not found.
static std::size_t find_word(const std::string &s, const std::string &w, std::size_t from = 0) {
  std::size_t p = from;
  while ((p = s.find(w, p)) != std::string::npos) {
    if (is_word_before(s, p) && is_word_after(s, p + w.size()))
      return p;
    ++p;
  }
  return std::string::npos;
}

// Like find_word but skips occurrences inside string literals.
static std::size_t find_word_code(const std::string &s, const std::string &w,
                                  std::size_t from = 0) {
  std::size_t p = from;
  while ((p = s.find(w, p)) != std::string::npos) {
    if (is_word_before(s, p) && is_word_after(s, p + w.size()) && !in_string_literal(s, p))
      return p;
    ++p;
  }
  return std::string::npos;
}

// Replace the first occurrence of `old_word` (with word-boundary check) in `s` with `rep`.
static std::string replace_word_first(const std::string &s, const std::string &old_word,
                                      const std::string &rep) {
  const std::size_t p = find_word(s, old_word);
  if (p == std::string::npos) return s;
  return s.substr(0, p) + rep + s.substr(p + old_word.size());
}

// Find the closing paren/bracket matching `open` at `pos` in string `s`.
// open_char/close_char: '(' ')' or '[' ']'
static std::size_t find_close(const std::string &s, std::size_t pos,
                              char open_char, char close_char) {
  int depth = 1;
  ++pos;
  for (; pos < s.size(); ++pos) {
    if (s[pos] == open_char) ++depth;
    else if (s[pos] == close_char) { --depth; if (!depth) return pos; }
  }
  return std::string::npos;
}

// ── Concat splitting ───────────────────────────────────────────────────────
static std::vector<std::string> split_string_concat(const std::string &expr) {
  std::vector<std::string> parts;
  std::string cur;
  int depth = 0;
  bool in_str = false;
  char delim = 0;
  for (std::size_t i = 0; i < expr.size(); ++i) {
    const char c = expr[i];
    if (in_str) {
      cur += c;
      if (c == '\\' && i + 1 < expr.size()) cur += expr[++i];
      else if (c == delim) in_str = false;
    } else if (c == '"' || c == '\'') {
      in_str = true; delim = c; cur += c;
    } else if (c == '(' || c == '[') { ++depth; cur += c; }
    else if (c == ')' || c == ']') { --depth; cur += c; }
    else if (c == '+' && depth == 0) { parts.push_back(trim(cur)); cur.clear(); }
    else cur += c;
  }
  parts.push_back(trim(cur));
  return parts;
}

// ── Concat part classification ─────────────────────────────────────────────
static std::pair<std::string, bool>
classify_concat_part(const std::string &raw,
                     const std::map<std::string, bool> &live,
                     const std::set<std::string> &string_fns) {
  const std::string p = trim(raw);
  if (!p.empty() && p.front() == '"') return {p, false};

  // std::to_string / to_string → dpp_to_string (value-return)
  {
    const bool has_std_to = p.rfind("std::to_string(", 0) == 0 ||
        p.find("std::to_string(") != std::string::npos;
    const bool bare_to = p.rfind("to_string(", 0) == 0;
    if (has_std_to || bare_to) {
      // Replace all occurrences
      std::string out = p;
      // Replace std::to_string( first
      for (;;) {
        const std::size_t pos = out.find("std::to_string(");
        if (pos == std::string::npos) break;
        out.replace(pos, 15, "dpp_to_string(");
      }
      // Then bare to_string(
      for (;;) {
        const std::size_t pos = out.find("to_string(");
        if (pos == std::string::npos) break;
        // Make sure it's not already prefixed
        if (pos >= 4 && out.compare(pos - 4, 4, "dpp_") == 0) break;
        out.replace(pos, 10, "dpp_to_string(");
      }
      return {out, true};
    }
  }

  if (p.find("dpp_to_string(") != std::string::npos ||
      p.find("dpp_string_from_") != std::string::npos ||
      p.find("dpp_string_substr(") != std::string::npos)
    return {p, true};

  // String-returning function call: name(...)
  {
    const std::size_t paren = p.find('(');
    if (paren != std::string::npos && paren > 0) {
      const std::string fn = trim(p.substr(0, paren));
      if (!fn.empty() && string_fns.count(fn)) return {p, true};
    }
  }

  if (live.count(p)) return {"dpp_string_c_str(&" + p + ")", false};
  return {p, false};
}

// ── Live-strings helpers ───────────────────────────────────────────────────
struct ScopedString {
  std::string name;
  std::size_t depth = 0;
};

static std::map<std::string, bool> live_strings(const std::vector<ScopedString> &ss) {
  std::map<std::string, bool> m;
  for (const auto &s : ss) m[s.name] = true;
  return m;
}

static std::vector<std::string> destroy_lines(const std::string &indent,
                                               const std::vector<ScopedString> &ss) {
  std::vector<std::string> out;
  for (auto it = ss.rbegin(); it != ss.rend(); ++it)
    out.push_back(indent + "dpp_string_destroy(&" + it->name + ");");
  return out;
}

static std::vector<std::string> close_scope_lines(const std::string &indent,
                                                   std::vector<ScopedString> &ss,
                                                   std::size_t remaining_depth) {
  std::vector<ScopedString> closing;
  for (const auto &s : ss)
    if (s.depth > remaining_depth) closing.push_back(s);
  ss.erase(std::remove_if(ss.begin(), ss.end(),
    [remaining_depth](const ScopedString &s) { return s.depth > remaining_depth; }), ss.end());
  return destroy_lines(indent, closing);
}

static std::vector<std::string> peek_destroy_lines(const std::string &indent,
                                                    const std::vector<ScopedString> &ss,
                                                    std::size_t min_depth) {
  std::vector<ScopedString> closing;
  for (const auto &s : ss)
    if (s.depth > min_depth) closing.push_back(s);
  return destroy_lines(indent, closing);
}

// ── cstr expression lowering ───────────────────────────────────────────────
// Lower a string-typed expression to a const char * expression.
static std::string lower_cstr_expr(const std::string &expr,
                                   const std::map<std::string, bool> &live) {
  const std::string stripped = trim(expr);
  if (!stripped.empty() && stripped.front() == '"') return stripped;
  if (live.count(stripped)) return "dpp_string_c_str(&" + stripped + ")";
  return stripped;
}

// ── Per-variable expression lowering ──────────────────────────────────────
// Lower all occurrences of `name` method calls, comparisons, subscripts, etc.

static std::string lower_string_var(std::string line, const std::string &name,
                                    const std::string &other_name) {
  // name == other_name / name != other_name
  // We'll handle these via find in lower_string_exprs; here handle same-var ordering
  // (caller handles all pairs).
  return line;
}

// Find pattern `<name>.method(args)` in line starting at `from`.
// Returns position of name start, or npos.
struct MethodMatch {
  std::size_t name_start;
  std::size_t args_start;  // after '('
  std::size_t close_paren; // position of ')'
  std::string args;
};

static bool find_method_call(const std::string &line, const std::string &name,
                              const std::string &method, std::size_t from,
                              MethodMatch &m) {
  std::size_t p = from;
  while (p < line.size()) {
    const std::size_t pos = find_word_code(line, name, p);
    if (pos == std::string::npos) return false;
    // After name: optional whitespace then '.' then method then '('
    std::size_t k = pos + name.size();
    k = skip_ws(line, k);
    if (k >= line.size() || line[k] != '.') { p = pos + 1; continue; }
    ++k;
    k = skip_ws(line, k);
    if (line.compare(k, method.size(), method) != 0 ||
        (k + method.size() < line.size() && is_ident(line[k + method.size()]))) {
      p = pos + 1; continue;
    }
    k += method.size();
    k = skip_ws(line, k);
    if (k >= line.size() || line[k] != '(') { p = pos + 1; continue; }
    const std::size_t cp = find_close(line, k, '(', ')');
    if (cp == std::string::npos) { p = pos + 1; continue; }
    m.name_start = pos;
    m.args_start = k + 1;
    m.close_paren = cp;
    m.args = trim(line.substr(k + 1, cp - k - 1));
    return true;
  }
  return false;
}

// Replace a single method call `name.method(args)` at known position.
static std::string replace_method_at(const std::string &line, const MethodMatch &mm,
                                     const std::string &replacement) {
  return line.substr(0, mm.name_start) + replacement + line.substr(mm.close_paren + 1);
}

// Lower all `name.method()` patterns and comparisons in one pass over `line`.
// This is called for EACH live string variable.
static std::string lower_one_var(std::string line, const std::string &name,
                                  const std::map<std::string, bool> &live,
                                  const std::set<std::string> &string_fns) {
  // ── size() / length() ──────────────────────────────────────────────────
  for (const std::string &meth : {"size", "length"}) {
    for (;;) {
      MethodMatch mm;
      if (!find_method_call(line, name, meth, 0, mm)) break;
      if (!mm.args.empty()) break; // unexpected args
      line = replace_method_at(line, mm, "dpp_string_size(&" + name + ")");
    }
  }

  // ── c_str() / data() ──────────────────────────────────────────────────
  for (const std::string &meth : {"c_str", "data"}) {
    for (;;) {
      MethodMatch mm;
      if (!find_method_call(line, name, meth, 0, mm)) break;
      line = replace_method_at(line, mm, "dpp_string_c_str(&" + name + ")");
    }
  }

  // ── empty() ─────────────────────────────────────────────────────────
  for (;;) {
    MethodMatch mm;
    if (!find_method_call(line, name, "empty", 0, mm)) break;
    line = replace_method_at(line, mm, "(dpp_string_size(&" + name + ") == 0)");
  }

  // ── push_back(c) ──────────────────────────────────────────────────────
  for (;;) {
    MethodMatch mm;
    if (!find_method_call(line, name, "push_back", 0, mm)) break;
    if (mm.args.empty()) break;
    line = replace_method_at(line, mm, "dpp_string_push_back(&" + name + ", " + mm.args + ")");
  }

  // ── clear() ─────────────────────────────────────────────────────────
  for (;;) {
    MethodMatch mm;
    if (!find_method_call(line, name, "clear", 0, mm)) break;
    line = replace_method_at(line, mm, "dpp_string_assign_cstr(&" + name + ", \"\")");
  }

  // ── append(expr) ──────────────────────────────────────────────────────
  for (;;) {
    MethodMatch mm;
    if (!find_method_call(line, name, "append", 0, mm)) break;
    if (mm.args.empty()) break;
    line = replace_method_at(line, mm, "dpp_string_append_cstr(&" + name + ", " + mm.args + ")");
  }

  // ── find(expr, pos) and find(expr) ────────────────────────────────────
  // Do two-arg form first.
  for (;;) {
    // Find `name.find(`
    const std::size_t np = find_word(line, name);
    if (np == std::string::npos) break;
    std::size_t k = np + name.size();
    k = skip_ws(line, k);
    if (k >= line.size() || line[k] != '.') break;
    ++k; k = skip_ws(line, k);
    if (line.compare(k, 4, "find") != 0 || (k + 4 < line.size() && is_ident(line[k+4]))) break;
    k += 4; k = skip_ws(line, k);
    if (k >= line.size() || line[k] != '(') break;
    const std::size_t cp = find_close(line, k, '(', ')');
    if (cp == std::string::npos) break;
    const std::string args_text = line.substr(k + 1, cp - k - 1);
    // Split on top-level comma to detect 2-arg form.
    std::size_t comma_pos = std::string::npos;
    {
      int d = 0;
      for (std::size_t i = 0; i < args_text.size(); ++i) {
        if (args_text[i] == '(' || args_text[i] == '[') ++d;
        else if (args_text[i] == ')' || args_text[i] == ']') --d;
        else if (args_text[i] == ',' && d == 0) { comma_pos = i; break; }
      }
    }
    std::string repl;
    if (comma_pos != std::string::npos) {
      const std::string arg1 = trim(args_text.substr(0, comma_pos));
      const std::string arg2 = trim(args_text.substr(comma_pos + 1));
      repl = "dpp_string_find_cstr_from(&" + name + ", " + arg1 + ", " + arg2 + ")";
    } else {
      repl = "dpp_string_find_cstr(&" + name + ", " + args_text + ")";
    }
    line = line.substr(0, np) + repl + line.substr(cp + 1);
    break; // restart from top
  }

  // ── substr(pos, len) ──────────────────────────────────────────────────
  for (;;) {
    MethodMatch mm;
    if (!find_method_call(line, name, "substr", 0, mm)) break;
    line = replace_method_at(line, mm, "dpp_string_substr(&" + name + ", " + mm.args + ")");
  }

  // ── at(idx) ───────────────────────────────────────────────────────────
  for (;;) {
    MethodMatch mm;
    if (!find_method_call(line, name, "at", 0, mm)) break;
    line = replace_method_at(line, mm, "dpp_string_c_str(&" + name + ")[" + mm.args + "]");
  }

  // ── name[idx] → dpp_string_c_str(&name)[idx] ─────────────────────────
  for (;;) {
    const std::size_t np = find_word(line, name);
    if (np == std::string::npos) break;
    std::size_t k = np + name.size();
    k = skip_ws(line, k);
    if (k >= line.size() || line[k] != '[') break;
    const std::size_t cb = find_close(line, k, '[', ']');
    if (cb == std::string::npos) break;
    const std::string idx = line.substr(k + 1, cb - k - 1);
    line = line.substr(0, np) + "dpp_string_c_str(&" + name + ")[" + idx + "]" +
           line.substr(cb + 1);
    break;
  }

  // ── name == other / name != other (string vars) ──────────────────────
  for (const auto &entry : live) {
    const std::string &other = entry.first;
    // name == other
    for (std::size_t from = 0;;) {
      const std::size_t np = find_word(line, name, from);
      if (np == std::string::npos) break;
      std::size_t k = np + name.size(); k = skip_ws(line, k);
      if (k + 2 > line.size() || line.substr(k, 2) != "==") { from = np + 1; continue; }
      k += 2; k = skip_ws(line, k);
      if (line.compare(k, other.size(), other) != 0 || !is_word_after(line, k + other.size()))
        { from = np + 1; continue; }
      line = line.substr(0, np) +
             "dpp_string_equal(&" + name + ", &" + other + ")" +
             line.substr(k + other.size());
      break;
    }
    // name != other
    for (std::size_t from = 0;;) {
      const std::size_t np = find_word(line, name, from);
      if (np == std::string::npos) break;
      std::size_t k = np + name.size(); k = skip_ws(line, k);
      if (k + 2 > line.size() || line.substr(k, 2) != "!=") { from = np + 1; continue; }
      k += 2; k = skip_ws(line, k);
      if (line.compare(k, other.size(), other) != 0 || !is_word_after(line, k + other.size()))
        { from = np + 1; continue; }
      line = line.substr(0, np) +
             "(!dpp_string_equal(&" + name + ", &" + other + "))" +
             line.substr(k + other.size());
      break;
    }
  }

  // ── name == "lit" / name != "lit" ────────────────────────────────────
  {
    auto cmp_lit = [&](const std::string &op, const std::string &fn_prefix,
                       const std::string &fn_suffix) {
      for (;;) {
        const std::size_t np = find_word(line, name);
        if (np == std::string::npos) break;
        std::size_t k = np + name.size(); k = skip_ws(line, k);
        if (line.compare(k, op.size(), op) != 0) break;
        k += op.size(); k = skip_ws(line, k);
        if (k >= line.size() || line[k] != '"') break;
        // Find end of string literal
        std::size_t lend = k + 1;
        while (lend < line.size()) {
          if (line[lend] == '\\') { lend += 2; continue; }
          if (line[lend] == '"') { ++lend; break; }
          ++lend;
        }
        const std::string lit = line.substr(k, lend - k);
        line = line.substr(0, np) + fn_prefix + name + ", " + lit + fn_suffix +
               line.substr(lend);
        break;
      }
    };
    cmp_lit("==", "dpp_string_equal_cstr(&", ")");
    cmp_lit("!=", "(!dpp_string_equal_cstr(&", "))");
  }

  // ── "lit" == name / "lit" != name ─────────────────────────────────────
  {
    auto lit_cmp = [&](const std::string &op, const std::string &fn_prefix,
                       const std::string &fn_suffix) {
      for (;;) {
        // Find string literal followed by op followed by name
        std::size_t p = 0;
        while (p < line.size()) {
          if (line[p] != '"') { ++p; continue; }
          std::size_t lend = p + 1;
          while (lend < line.size()) {
            if (line[lend] == '\\') { lend += 2; continue; }
            if (line[lend] == '"') { ++lend; break; }
            ++lend;
          }
          const std::string lit = line.substr(p, lend - p);
          std::size_t k = skip_ws(line, lend);
          if (line.compare(k, op.size(), op) != 0) { p = lend; continue; }
          k += op.size(); k = skip_ws(line, k);
          if (line.compare(k, name.size(), name) != 0 ||
              !is_word_after(line, k + name.size())) { p = lend; continue; }
          line = line.substr(0, p) + fn_prefix + name + ", " + lit + fn_suffix +
                 line.substr(k + name.size());
          break;
        }
        break; // Only one pass needed
      }
    };
    lit_cmp("==", "dpp_string_equal_cstr(&", ")");
    lit_cmp("!=", "(!dpp_string_equal_cstr(&", "))");
  }

  // ── Ordering comparisons: name op "lit", name op other ───────────────
  {
    const std::vector<std::pair<std::string,std::string>> ordering_ops = {
      {"<=", "<="}, {">=", ">="}, {"<", "<"}, {">", ">"}
    };
    for (const auto &op_pair : ordering_ops) {
      const std::string &op = op_pair.first;
      const std::string &op_out = op_pair.second;
      // name op "lit"
      for (std::size_t from = 0;;) {
        const std::size_t np = find_word(line, name, from);
        if (np == std::string::npos) break;
        std::size_t k = np + name.size(); k = skip_ws(line, k);
        if (line.compare(k, op.size(), op) != 0) { from = np + 1; continue; }
        k += op.size(); k = skip_ws(line, k);
        if (k >= line.size() || line[k] != '"') { from = np + 1; continue; }
        std::size_t lend = k + 1;
        while (lend < line.size()) {
          if (line[lend] == '\\') { lend += 2; continue; }
          if (line[lend] == '"') { ++lend; break; }
          ++lend;
        }
        const std::string lit = line.substr(k, lend - k);
        line = line.substr(0, np) +
               "(dpp_string_compare_cstr(&" + name + ", " + lit + ") " + op_out + " 0)" +
               line.substr(lend);
        break;
      }
      // name op other (other is another live var)
      for (const auto &entry : live) {
        const std::string &other = entry.first;
        for (std::size_t from = 0;;) {
          const std::size_t np = find_word(line, name, from);
          if (np == std::string::npos) break;
          std::size_t k = np + name.size(); k = skip_ws(line, k);
          if (line.compare(k, op.size(), op) != 0) { from = np + 1; continue; }
          k += op.size(); k = skip_ws(line, k);
          if (line.compare(k, other.size(), other) != 0 ||
              !is_word_after(line, k + other.size())) { from = np + 1; continue; }
          line = line.substr(0, np) +
                 "(dpp_string_compare(&" + name + ", &" + other + ") " + op_out + " 0)" +
                 line.substr(k + other.size());
          break;
        }
      }
    }
    // "lit" op name (reversed ordering)
    const std::vector<std::pair<std::string,std::string>> rev_ops = {
      {"<=", ">="}, {">=", "<="}, {"<", ">"}, {">", "<"}
    };
    for (const auto &op_pair : rev_ops) {
      const std::string &op = op_pair.first;
      const std::string &op_out = op_pair.second;
      for (;;) {
        std::size_t p = 0;
        bool found = false;
        while (p < line.size()) {
          if (line[p] != '"') { ++p; continue; }
          std::size_t lend = p + 1;
          while (lend < line.size()) {
            if (line[lend] == '\\') { lend += 2; continue; }
            if (line[lend] == '"') { ++lend; break; }
            ++lend;
          }
          const std::string lit = line.substr(p, lend - p);
          std::size_t k = skip_ws(line, lend);
          if (line.compare(k, op.size(), op) != 0) { p = lend; continue; }
          k += op.size(); k = skip_ws(line, k);
          if (line.compare(k, name.size(), name) != 0 ||
              !is_word_after(line, k + name.size())) { p = lend; continue; }
          line = line.substr(0, p) +
                 "(dpp_string_compare_cstr(&" + name + ", " + lit + ") " + op_out + " 0)" +
                 line.substr(k + name.size());
          found = true;
          break;
        }
        if (!found) break;
      }
    }
  }

  // ── << name (streaming, not followed by [ ) ──────────────────────────
  for (std::size_t search = 0;;) {
    const std::size_t op_pos = line.find("<<", search);
    if (op_pos == std::string::npos) break;
    std::size_t k = op_pos + 2; k = skip_ws(line, k);
    if (line.compare(k, name.size(), name) != 0 || !is_word_after(line, k + name.size()))
      { search = op_pos + 1; continue; }
    // Not followed by '[' (subscript handled above)
    std::size_t after = skip_ws(line, k + name.size());
    if (after < line.size() && line[after] == '[') { search = op_pos + 1; continue; }
    line = line.substr(0, op_pos + 2) + " dpp_string_c_str(&" + name + ")" +
           line.substr(k + name.size());
    break;
  }

  return line;
}

// Lower string comparisons and method calls for all live string vars.
static std::string lower_string_exprs(std::string line,
                                       const std::map<std::string, bool> &live,
                                       const std::set<std::string> &string_fns) {
  for (const auto &entry : live)
    line = lower_one_var(line, entry.first, live, string_fns);
  return line;
}

// Wrap live dpp_string vars as c_str when used as function arguments.
static std::string wrap_cstr_args(std::string line, const std::map<std::string, bool> &live) {
  for (const auto &entry : live) {
    const std::string &name = entry.first;
    // Find `name` preceded by '(' or ',' and followed by ')' or ',' (not '.', '[', '&')
    std::size_t p = 0;
    while (p < line.size()) {
      const std::size_t pos = find_word(line, name, p);
      if (pos == std::string::npos) break;
      // Check what precedes
      std::size_t pre = pos;
      while (pre > 0 && (line[pre-1] == ' ' || line[pre-1] == '\t')) --pre;
      bool ok_before = (pre > 0 && (line[pre-1] == '(' || line[pre-1] == ','));
      // Check what follows
      std::size_t after = skip_ws(line, pos + name.size());
      bool ok_after = (after >= line.size() || line[after] == ')' || line[after] == ',' ||
                       line[after] == ';');
      // Must not be followed by '.' '[' '&' '('
      bool bad_after = (after < line.size() &&
                        (line[after] == '.' || line[after] == '[' ||
                         line[after] == '&' || line[after] == '('));
      if (ok_before && !bad_after) {
        const std::string rep = "dpp_string_c_str(&" + name + ")";
        line = line.substr(0, pos) + rep + line.substr(pos + name.size());
        p = pos + rep.size();
      } else {
        p = pos + 1;
      }
    }
  }
  return line;
}

// ── String field expression lowering ──────────────────────────────────────
// Lower obj.fname.c_str() etc. for known string field names.
static std::string lower_string_field_exprs(std::string line,
                                             const std::set<std::string> &field_names) {
  for (const std::string &fname : field_names) {
    // Pattern: path.fname.method()
    // path = identifier or identifier.identifier (etc.)
    // Look for `.fname.method(` in the line
    for (;;) {
      const std::string dot_fname = "." + fname + ".";
      const std::size_t dfp = line.find(dot_fname);
      if (dfp == std::string::npos) break;
      // Scan backward to find start of path
      std::size_t path_start = dfp;
      while (path_start > 0 && (is_ident(line[path_start-1]) || line[path_start-1] == '.'))
        --path_start;
      // Ensure path is not empty
      if (path_start == dfp) { break; } // no path before the dot
      const std::string path_and_fname = line.substr(path_start, dfp - path_start) + "." + fname;

      // Find method after `.fname.`
      std::size_t k = dfp + dot_fname.size();
      k = skip_ws(line, k);
      // Match one of: c_str, data, size, length, empty
      struct M { const char *name; const char *fn; };
      const M methods[] = {
        {"c_str", "dpp_string_c_str(&"},
        {"data",  "dpp_string_c_str(&"},
        {"size",  "dpp_string_size(&"},
        {"length","dpp_string_size(&"},
        {"empty", nullptr},
      };
      bool matched = false;
      for (const auto &m : methods) {
        const std::size_t ml = std::strlen(m.name);
        if (line.compare(k, ml, m.name) != 0 ||
            (k + ml < line.size() && is_ident(line[k + ml]))) continue;
        std::size_t k2 = k + ml;
        k2 = skip_ws(line, k2);
        if (k2 >= line.size() || line[k2] != '(') continue;
        const std::size_t cp = find_close(line, k2, '(', ')');
        if (cp == std::string::npos) continue;
        std::string repl;
        if (m.fn) {
          repl = std::string(m.fn) + path_and_fname + ")";
        } else {
          // empty
          repl = "(dpp_string_size(&" + path_and_fname + ") == 0)";
        }
        line = line.substr(0, path_start) + repl + line.substr(cp + 1);
        matched = true;
        break;
      }
      if (matched) continue;

      // Check for `<< path.fname` without method
      if (path_start > 2) {
        std::size_t pp = path_start;
        while (pp > 0 && (line[pp-1] == ' ' || line[pp-1] == '\t')) --pp;
        if (pp >= 2 && line.substr(pp - 2, 2) == "<<") {
          // << path.fname → << dpp_string_c_str(&path.fname)
          // find end of fname
          std::size_t end = dfp + dot_fname.size() - 1; // position after fname
          // check that nothing follows
          std::size_t after = skip_ws(line, end);
          if (after >= line.size() || !is_ident(line[after])) {
            line = line.substr(0, path_start) + "dpp_string_c_str(&" + path_and_fname + ")" +
                   line.substr(end);
            continue;
          }
        }
      }

      // Check for == "lit" and != "lit" after path.fname
      {
        std::size_t end = dfp + dot_fname.size() - 1; // after fname
        std::size_t k2 = skip_ws(line, end);
        if (k2 + 2 <= line.size()) {
          std::string op;
          std::string fn_prefix;
          if (line.substr(k2, 2) == "==" ) { op = "=="; fn_prefix = "dpp_string_equal_cstr(&"; }
          else if (line.substr(k2, 2) == "!=") { op = "!="; fn_prefix = "(!dpp_string_equal_cstr(&"; }
          if (!op.empty()) {
            std::size_t k3 = skip_ws(line, k2 + 2);
            if (k3 < line.size() && line[k3] == '"') {
              std::size_t lend = k3 + 1;
              while (lend < line.size()) {
                if (line[lend] == '\\') { lend += 2; continue; }
                if (line[lend] == '"') { ++lend; break; }
                ++lend;
              }
              const std::string lit = line.substr(k3, lend - k3);
              const std::string close_paren = (op == "!=") ? ")" : "";
              line = line.substr(0, path_start) + fn_prefix + path_and_fname + ", " + lit + ")" +
                     close_paren + line.substr(lend);
              continue;
            }
          }
        }
      }
      break; // No more matches for this fname in this iteration
    }
  }
  return line;
}

// ── Pre-scan: use tokens from ParsedSource ─────────────────────────────────

using Tokens = std::vector<parser::Token>;
using TK = parser::TokenKind;
using KW = parser::KeywordKind;

static std::size_t tok_next_nc(const Tokens &T, std::size_t i) {
  std::size_t j = i + 1;
  while (j < T.size() && T[j].kind == TK::comment) ++j;
  return j;
}

static std::size_t tok_find_close_paren(const Tokens &T, std::size_t open_idx) {
  int d = 1;
  for (std::size_t k = open_idx + 1; k < T.size(); ++k) {
    if (T[k].kind == TK::punctuation) {
      if (T[k].text == "(") ++d;
      else if (T[k].text == ")") { --d; if (!d) return k; }
    }
  }
  return T.size();
}

// Scan token params between open_paren+1..close_paren for identifiers with "string" in type.
static std::vector<std::string> tok_params_with_string(const Tokens &T,
                                                        std::size_t open_paren,
                                                        std::size_t close_paren) {
  std::vector<std::string> result;
  bool has_string = false;
  std::string last_ident;
  int pd = 0;

  for (std::size_t pi = open_paren + 1; pi <= close_paren && pi < T.size(); ++pi) {
    if (T[pi].kind == TK::comment) continue;
    bool at_sep = (pi == close_paren);
    if (!at_sep && T[pi].kind == TK::punctuation) {
      if (T[pi].text == "(" || T[pi].text == "[") ++pd;
      else if (T[pi].text == ")" || T[pi].text == "]") { if (pd > 0) --pd; }
      else if (T[pi].text == "," && pd == 0) at_sep = true;
    }
    if (at_sep) {
      if (has_string && !last_ident.empty()) result.push_back(last_ident);
      has_string = false; last_ident.clear();
    } else {
      if (T[pi].kind == TK::identifier && T[pi].text == "string") has_string = true;
      else if (T[pi].kind == TK::identifier && pd == 0) last_ident = T[pi].text;
    }
  }
  return result;
}

} // anonymous namespace

// ── Main pass ─────────────────────────────────────────────────────────────────

StringResult lower_strings(const std::string &source,
                           const parser::ParsedSource &parsed) {
  StringResult result;

  // ── Pre-scan 1: string field names and string-returning functions via ir ────
  const ir::ScopeAnalysis scope_info = ir::analyze_scope(parsed.tokens);

  std::set<std::string> string_field_names;
  std::set<std::string> string_fns;

  for (const ir::ScopeVar &sv : scope_info.vars) {
    const bool has_string = sv.type.find("string") != std::string::npos;
    if (sv.is_function) {
      if (has_string) string_fns.insert(sv.name);
    } else if (sv.is_field) {
      if (has_string) string_field_names.insert(sv.name);
    }
  }

  // ── Pre-scan 2: string param functions via token scan ─────────────────────
  std::map<std::string, std::vector<std::string>> str_param_fns;
  {
    const Tokens &T = parsed.tokens;
    for (std::size_t i = 0; i < T.size(); ++i) {
      if (T[i].kind == TK::end_of_file) break;
      if (T[i].kind != TK::identifier) continue;
      const std::string fn_name = T[i].text;
      const std::size_t j = tok_next_nc(T, i);
      if (j >= T.size() || T[j].kind != TK::punctuation || T[j].text != "(") continue;
      const std::size_t cp = tok_find_close_paren(T, j);
      if (cp >= T.size()) continue;
      // Check if followed by [const] {
      std::size_t k = tok_next_nc(T, cp);
      if (k < T.size() && T[k].kind == TK::keyword && T[k].keyword == KW::const_kw)
        k = tok_next_nc(T, k);
      if (k >= T.size() || T[k].kind != TK::punctuation || T[k].text != "{") continue;
      // Confirmed function definition. Check for string params.
      const std::vector<std::string> sp = tok_params_with_string(T, j, cp);
      if (!sp.empty()) str_param_fns[fn_name] = sp;
    }
  }

  // ── Main pass (line-by-line) ──────────────────────────────────────────────
  bool inside_string_ret_fn = false;
  std::set<std::string> cstr_params;
  std::vector<ScopedString> strings;
  std::vector<std::string> out;
  std::size_t brace_depth = 0;
  bool inside_record = false;
  std::vector<std::size_t> loop_depths;

  auto update_scope = [&](const std::string &line) {
    brace_depth += count_char(line, '{');
    const std::size_t closes = count_char(line, '}');
    brace_depth = closes > brace_depth ? 0 : brace_depth - closes;
    while (!loop_depths.empty() && brace_depth <= loop_depths.back())
      loop_depths.pop_back();
  };

  for (const std::string &line : split_lines(source)) {
    const std::size_t before_depth = brace_depth;
    const std::string stripped = trim(line);

    // ── Loop detection ────────────────────────────────────────────────────
    if (is_loop_kw(stripped, "for") || is_loop_kw(stripped, "while") ||
        is_loop_kw(stripped, "do")) {
      loop_depths.push_back(before_depth);
    }

    // ── Record tracking ───────────────────────────────────────────────────
    // starts_record: line that opens a struct/class body
    {
      const bool has_struct = stripped.rfind("struct ", 0) == 0 ||
                              stripped.rfind("class ", 0) == 0 ||
                              stripped.rfind("typedef struct", 0) == 0 ||
                              stripped.rfind("typedef class", 0) == 0;
      if (has_struct && stripped.find('{') != std::string::npos)
        inside_record = true;
    }

    // ── Remove #include <string> ──────────────────────────────────────────
    if (stripped == "#include <string>") {
      result.used_string = true;
      update_scope(line);
      continue;
    }

    // ── Entering a top-level function definition ──────────────────────────
    if (brace_depth == 0 && stripped.find('(') != std::string::npos &&
        !stripped.empty() && stripped.back() == '{') {
      cstr_params.clear();
      inside_string_ret_fn = false;
      // Find function name: last identifier before '('
      const std::size_t paren = stripped.find('(');
      if (paren > 0) {
        std::size_t end = paren;
        while (end > 0 && stripped[end-1] == ' ') --end;
        std::size_t start = end;
        while (start > 0 && is_ident(stripped[start-1])) --start;
        if (start < end) {
          const std::string fn_name = stripped.substr(start, end - start);
          inside_string_ret_fn = string_fns.count(fn_name) > 0;
          const auto it = str_param_fns.find(fn_name);
          if (it != str_param_fns.end())
            for (const std::string &pname : it->second) cstr_params.insert(pname);
        }
      }
    }
    if (brace_depth == 1 && stripped == "}") {
      cstr_params.clear();
      inside_string_ret_fn = false;
    }

    // ── Declaration: [std::]string name; ─────────────────────────────────
    {
      const std::string s = stripped;
      std::size_t pos = 0;
      const std::size_t n = match_str_type(s, pos);
      if (n > 0) {
        std::size_t k = skip_ws(s, pos + n);
        const std::string nm = read_ident(s, k);
        if (!nm.empty()) {
          std::size_t k2 = skip_ws(s, k + nm.size());
          const std::string indent_str = leading_indent(line);
          // Default decl: string name;
          if (k2 < s.size() && s[k2] == ';' && k2 + 1 == s.size()) {
            result.used_string = true;
            if (inside_record) {
              out.push_back(indent_str + "dpp_string " + nm + ";");
            } else {
              strings.push_back({nm, before_depth});
              out.push_back(indent_str + "dpp_string " + nm + ";");
              out.push_back(indent_str + "dpp_string_init(&" + nm + ");");
            }
            update_scope(line);
            continue;
          }
          // Value init: string name = EXPR;
          if (k2 < s.size() && s[k2] == '=') {
            // Check for 'const' before the type
            const std::string s_trim = stripped;
            const bool has_const = s_trim.rfind("const ", 0) == 0;
            if (has_const) {
              // const [std::]string name = EXPR; → const char *name = EXPR;
              const std::size_t semi = s_trim.rfind(';');
              if (semi != std::string::npos) {
                const std::string rhs = trim(s_trim.substr(k2 + 1, semi - k2 - 1));
                result.used_string = true;
                out.push_back(indent_str + "const char *" + nm + " = " + rhs + ";");
                update_scope(line);
                continue;
              }
            }
            // Find trailing semicolon
            const std::size_t semi = s_trim.rfind(';');
            if (semi != std::string::npos) {
              result.used_string = true;
              strings.push_back({nm, before_depth});
              const std::map<std::string, bool> live = live_strings(strings);
              const std::string raw_rhs = trim(s_trim.substr(k2 + 1, semi - k2 - 1));

              const std::vector<std::string> concat_parts = split_string_concat(raw_rhs);
              if (concat_parts.size() > 1) {
                auto [first_expr, first_is_val] =
                    classify_concat_part(concat_parts[0], live, string_fns);
                if (first_is_val) {
                  out.push_back(indent_str + "dpp_string " + nm + " = " + first_expr + ";");
                } else {
                  out.push_back(indent_str + "dpp_string " + nm + ";");
                  out.push_back(indent_str + "dpp_string_init_cstr(&" + nm + ", " + first_expr + ");");
                }
                for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
                  auto [expr, is_val] = classify_concat_part(concat_parts[pi], live, string_fns);
                  if (is_val)
                    out.push_back(indent_str + "dpp_string_append_dpp(&" + nm + ", " + expr + ");");
                  else
                    out.push_back(indent_str + "dpp_string_append_cstr(&" + nm + ", " + expr + ");");
                }
                update_scope(line);
                continue;
              }

              // Lower method calls in the rhs before classifying (e.g. s.substr → dpp_string_substr)
              // Also wrap live string vars passed as function arguments.
              const std::string lowered_rhs =
                  wrap_cstr_args(lower_string_exprs(raw_rhs, live, string_fns), live);
              // Single RHS
              auto [first_expr, first_is_val] =
                  classify_concat_part(lowered_rhs, live, string_fns);
              if (first_is_val) {
                out.push_back(indent_str + "dpp_string " + nm + " = " + first_expr + ";");
              } else {
                out.push_back(indent_str + "dpp_string " + nm + ";");
                out.push_back(indent_str + "dpp_string_init_cstr(&" + nm + ", " +
                              lower_cstr_expr(lowered_rhs, live) + ");");
              }
              update_scope(line);
              continue;
            }
          }
          // Fill ctor: string name(count, 'C');
          if (k2 < s.size() && s[k2] == '(') {
            const std::size_t semi = s.rfind(';');
            if (semi != std::string::npos && s[semi - 1] == ')') {
              // Find the comma inside the parens
              const std::size_t close_p = s.rfind(')', semi);
              const std::string args_text = s.substr(k2 + 1, close_p - k2 - 1);
              // Check for single char literal: find comma + char literal
              const std::size_t comma = args_text.rfind(',');
              if (comma != std::string::npos) {
                const std::string second_arg = trim(args_text.substr(comma + 1));
                if (second_arg.size() >= 3 && second_arg.front() == '\'' &&
                    second_arg.back() == '\'') {
                  // Fill ctor
                  result.used_string = true;
                  strings.push_back({nm, before_depth});
                  const std::string count_text = trim(args_text.substr(0, comma));
                  out.push_back(indent_str + "dpp_string " + nm + ";");
                  out.push_back(indent_str + "dpp_string_init_fill(&" + nm + ", " +
                                count_text + ", " + second_arg + ");");
                  update_scope(line);
                  continue;
                }
              }
              // Single-arg ctor: string name(expr);
              result.used_string = true;
              strings.push_back({nm, before_depth});
              const std::map<std::string, bool> live = live_strings(strings);
              out.push_back(indent_str + "dpp_string " + nm + ";");
              out.push_back(indent_str + "dpp_string_init_cstr(&" + nm + ", " +
                            lower_cstr_expr(trim(args_text), live) + ");");
              update_scope(line);
              continue;
            }
          }
        }
      }
    }

    // ── const [std::]string name = EXPR; (const decl, caught before above) ─
    {
      const std::string &s = stripped;
      if (s.rfind("const ", 0) == 0) {
        std::size_t k = skip_ws(s, 6);
        const std::size_t n = match_str_type(s, k);
        if (n > 0) {
          k = skip_ws(s, k + n);
          const std::string nm = read_ident(s, k);
          if (!nm.empty()) {
            std::size_t k2 = skip_ws(s, k + nm.size());
            if (k2 < s.size() && s[k2] == '=') {
              const std::size_t semi = s.rfind(';');
              if (semi != std::string::npos) {
                const std::string rhs = trim(s.substr(k2 + 1, semi - k2 - 1));
                result.used_string = true;
                out.push_back(leading_indent(line) + "const char *" + nm + " = " + rhs + ";");
                update_scope(line);
                continue;
              }
            }
          }
        }
      }
    }

    // ── Range-for over dpp_string: for (char c : str) { ──────────────────
    {
      // Pattern: stripped starts with "for" then has "(char CVAR : SNAME) {"
      if (is_loop_kw(stripped, "for")) {
        const std::map<std::string, bool> live_check = live_strings(strings);
        // Manually parse the for header
        std::size_t k = 3; // after "for"
        k = skip_ws(stripped, k);
        if (k < stripped.size() && stripped[k] == '(') {
          ++k; k = skip_ws(stripped, k);
          // "char"
          if (stripped.compare(k, 4, "char") == 0 && !is_ident(stripped[k+4])) {
            k += 4; k = skip_ws(stripped, k);
            const std::string cvar = read_ident(stripped, k);
            if (!cvar.empty()) {
              k += cvar.size(); k = skip_ws(stripped, k);
              if (k < stripped.size() && stripped[k] == ':') {
                ++k; k = skip_ws(stripped, k);
                const std::string sname = read_ident(stripped, k);
                if (!sname.empty() && live_check.count(sname)) {
                  k += sname.size(); k = skip_ws(stripped, k);
                  if (k < stripped.size() && stripped[k] == ')') {
                    ++k; k = skip_ws(stripped, k);
                    if (k < stripped.size() && stripped[k] == '{') {
                      // Emit lowered range-for
                      const std::string ind = leading_indent(line);
                      const std::string idx = "_dpp_idx_" + cvar;
                      out.push_back(ind + "for (size_t " + idx + " = 0; " + idx +
                                    " < dpp_string_size(&" + sname + "); ++" + idx + ") {");
                      out.push_back(ind + "  char " + cvar + " = dpp_string_c_str(&" +
                                    sname + ")[" + idx + "];");
                      // Note: loop_depths was pushed in the loop-kw check above
                      update_scope(line);
                      continue;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    // ── Per-variable lowering ─────────────────────────────────────────────
    std::string lowered = line;
    const std::map<std::string, bool> live = live_strings(strings);

    // Assignment and append-assign for live vars
    for (const auto &entry : live) {
      const std::string &name = entry.first;
      const std::string ls = trim(lowered);

      // name = EXPR;
      {
        std::size_t p = 0;
        while (p < ls.size()) {
          const std::size_t np = find_word_code(ls, name, p);
          if (np == std::string::npos) break;
          std::size_t k = np + name.size(); k = skip_ws(ls, k);
          if (k >= ls.size() || ls[k] != '=') { p = np + 1; break; }
          // Make sure it's not == or !=
          if (k + 1 < ls.size() && ls[k+1] == '=') { p = np + 1; break; }
          if (np > 0) { // check if there's != preceding
            std::size_t before = np;
            while (before > 0 && ls[before-1] == ' ') --before;
            if (before > 0 && ls[before-1] == '!') { p = np + 1; break; }
          }
          // Found assignment
          ++k;
          const std::size_t semi = ls.rfind(';');
          if (semi == std::string::npos || semi <= k) { p = np + 1; break; }
          const std::string ind = leading_indent(lowered);
          const std::string raw_assign = trim(ls.substr(k, semi - k));
          const std::vector<std::string> concat_parts = split_string_concat(raw_assign);
          if (concat_parts.size() > 1) {
            auto [first_expr, first_is_val] =
                classify_concat_part(concat_parts[0], live, string_fns);
            if (first_is_val) {
              const std::string tmp = "_dpp_concat_tmp";
              lowered = ind + "{ dpp_string " + tmp + " = " + first_expr + ";";
              for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
                auto [expr, is_val] = classify_concat_part(concat_parts[pi], live, string_fns);
                if (is_val) lowered += " dpp_string_append_dpp(&" + tmp + ", " + expr + ");";
                else lowered += " dpp_string_append_cstr(&" + tmp + ", " + expr + ");";
              }
              lowered += " dpp_string_assign_cstr(&" + name +
                         ", dpp_string_c_str(&" + tmp + "));"
                         " dpp_string_destroy(&" + tmp + "); }";
            } else {
              lowered = ind + "dpp_string_assign_cstr(&" + name + ", " + first_expr + ");";
              for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
                auto [expr, is_val] = classify_concat_part(concat_parts[pi], live, string_fns);
                if (is_val)
                  lowered += "\n" + ind + "dpp_string_append_dpp(&" + name + ", " + expr + ");";
                else
                  lowered += "\n" + ind + "dpp_string_append_cstr(&" + name + ", " + expr + ");";
              }
            }
          } else {
            lowered = ind + "dpp_string_assign_cstr(&" + name + ", " +
                      lower_cstr_expr(raw_assign, live) + ");";
          }
          break;
        }
      }

      // name += EXPR;
      {
        const std::string ls2 = trim(lowered);
        const std::size_t np = find_word_code(ls2, name);
        if (np != std::string::npos) {
          std::size_t k = np + name.size(); k = skip_ws(ls2, k);
          if (k + 2 <= ls2.size() && ls2.substr(k, 2) == "+=") {
            k += 2;
            const std::size_t semi = ls2.rfind(';');
            if (semi != std::string::npos && semi > k) {
              const std::string raw = trim(ls2.substr(k, semi - k));
              auto [expr, is_val] = classify_concat_part(raw, live, string_fns);
              const std::string ind = leading_indent(lowered);
              if (is_val)
                lowered = ind + "dpp_string_append_dpp(&" + name + ", " + expr + ");";
              else
                lowered = ind + "dpp_string_append_cstr(&" + name + ", " + expr + ");";
            }
          }
        }
      }

      // name.append(EXPR);
      {
        MethodMatch mm;
        if (find_method_call(trim(lowered), name, "append", 0, mm)) {
          const std::string ind = leading_indent(lowered);
          lowered = ind + "dpp_string_append_cstr(&" + name + ", " +
                    lower_cstr_expr(mm.args, live) + ");";
        }
      }

      // name.clear();
      {
        MethodMatch mm;
        if (find_method_call(trim(lowered), name, "clear", 0, mm)) {
          lowered = leading_indent(lowered) + "dpp_string_assign_cstr(&" + name + ", \"\");";
        }
      }
    }

    // ── std::stoi/stod on live string vars ────────────────────────────────
    for (const auto &entry : live) {
      const std::string &sname = entry.first;
      for (const auto &fn_pair : std::vector<std::pair<std::string,std::string>>{
             {"std::stoi", "atoi"}, {"std::stod", "atof"},
             {"stoi", "atoi"}, {"stod", "atof"}}) {
        const std::string &cpp_fn = fn_pair.first;
        const std::string &c_fn = fn_pair.second;
        // Find: cpp_fn(sname) and replace
        for (;;) {
          const std::size_t fp = lowered.find(cpp_fn + "(");
          if (fp == std::string::npos) break;
          // Check word boundary before (also reject bare stoi/stod when preceded by ::)
          if (fp > 0 && is_ident(lowered[fp - 1])) break;
          if (fp >= 2 && lowered[fp - 1] == ':' && lowered[fp - 2] == ':') break;
          const std::size_t arg_start = fp + cpp_fn.size() + 1;
          std::size_t k = skip_ws(lowered, arg_start);
          if (lowered.compare(k, sname.size(), sname) != 0 ||
              !is_word_after(lowered, k + sname.size())) break;
          std::size_t k2 = skip_ws(lowered, k + sname.size());
          if (k2 >= lowered.size() || lowered[k2] != ')') break;
          const std::string repl = c_fn + "(dpp_string_c_str(&" + sname + "))";
          lowered = lowered.substr(0, fp) + repl + lowered.substr(k2 + 1);
          break;
        }
      }
    }

    // ── cstr param method calls ────────────────────────────────────────────
    for (const std::string &pname : cstr_params) {
      // pname.size() → strlen(pname)
      for (;;) {
        MethodMatch mm;
        if (!find_method_call(lowered, pname, "size", 0, mm)) break;
        lowered = replace_method_at(lowered, mm, "strlen(" + pname + ")");
      }
      // pname.c_str() → pname
      for (;;) {
        MethodMatch mm;
        if (!find_method_call(lowered, pname, "c_str", 0, mm)) break;
        lowered = replace_method_at(lowered, mm, pname);
      }
      // pname.empty() → (pname[0] == '\0')
      for (;;) {
        MethodMatch mm;
        if (!find_method_call(lowered, pname, "empty", 0, mm)) break;
        lowered = replace_method_at(lowered, mm, "(" + pname + "[0] == '\\0')");
      }
      // pname.data() → pname
      for (;;) {
        MethodMatch mm;
        if (!find_method_call(lowered, pname, "data", 0, mm)) break;
        lowered = replace_method_at(lowered, mm, pname);
      }
    }

    // ── General expr lowering for all live vars ────────────────────────────
    lowered = lower_string_exprs(lowered, live, string_fns);

    // ── String field accesses ─────────────────────────────────────────────
    if (!string_field_names.empty())
      lowered = lower_string_field_exprs(lowered, string_field_names);

    // ── Wrap live vars as c_str in function call arguments ─────────────────
    if (!live.empty())
      lowered = wrap_cstr_args(lowered, live);

    const std::string lowered_stripped = trim(lowered);

    // ── Return lowering for string-returning functions ─────────────────────
    if (inside_string_ret_fn && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string ret_expr = lowered_stripped.substr(7); // "return "
      if (!ret_expr.empty() && ret_expr.back() == ';') ret_expr.pop_back();
      ret_expr = trim(ret_expr);

      const bool is_tracked_var = std::any_of(strings.begin(), strings.end(),
          [&](const ScopedString &s) { return s.name == ret_expr; });
      const bool is_dpp_str_call =
          ret_expr.rfind("dpp_string_", 0) == 0 ||
          [&]() {
            const std::size_t p = ret_expr.find('(');
            if (p == std::string::npos) return false;
            return string_fns.count(trim(ret_expr.substr(0, p))) > 0;
          }();

      if (!is_tracked_var && !is_dpp_str_call) {
        const std::vector<std::string> concat_parts = split_string_concat(ret_expr);
        const std::string tmp = "_dpp_str_ret";

        auto classify_ret_part = [&](const std::string &raw) -> std::pair<std::string, bool> {
          const std::string p = trim(raw);
          if (!p.empty() && p.front() == '"') return {p, false};
          if (cstr_params.count(p)) return {p, false};
          if (live.count(p)) return {"dpp_string_c_str(&" + p + ")", false};
          if (p.find("dpp_to_string(") != std::string::npos ||
              p.find("dpp_string_from_") != std::string::npos ||
              p.find("dpp_string_substr(") != std::string::npos) return {p, true};
          const std::size_t pp = p.find('(');
          if (pp != std::string::npos && string_fns.count(trim(p.substr(0, pp)))) return {p, true};
          if (p.find("->") != std::string::npos || p.find('.') != std::string::npos)
            return {"dpp_string_c_str(&" + p + ")", false};
          return {p, false};
        };

        {
          auto [first_expr, first_is_val] = classify_ret_part(concat_parts[0]);
          if (first_is_val)
            out.push_back(indent + "dpp_string " + tmp + " = " + first_expr + ";");
          else {
            out.push_back(indent + "dpp_string " + tmp + ";");
            out.push_back(indent + "dpp_string_init_cstr(&" + tmp + ", " + first_expr + ");");
          }
          for (std::size_t pi = 1; pi < concat_parts.size(); ++pi) {
            auto [expr, is_val] = classify_ret_part(concat_parts[pi]);
            if (is_val)
              out.push_back(indent + "dpp_string_append_dpp(&" + tmp + ", " + expr + ");");
            else
              out.push_back(indent + "dpp_string_append_cstr(&" + tmp + ", " + expr + ");");
          }
          out.push_back(indent + "return " + tmp + ";");
          result.used_string = true;
        }
        update_scope(line);
        continue;
      }
    }

    // ── Return lowering when live string vars exist ────────────────────────
    if (!strings.empty() && lowered_stripped.rfind("return ", 0) == 0) {
      const std::string indent = leading_indent(lowered);
      std::string ret_expr = lowered_stripped.substr(7);
      if (!ret_expr.empty() && ret_expr.back() == ';') ret_expr.pop_back();
      ret_expr = trim(ret_expr);

      const auto ret_it = std::find_if(strings.begin(), strings.end(),
          [&](const ScopedString &s) { return s.name == ret_expr; });
      if (ret_it != strings.end()) {
        // Destroy all other string vars, then return this one.
        for (auto it = strings.rbegin(); it != strings.rend(); ++it) {
          if (it->name != ret_expr)
            out.push_back(indent + "dpp_string_destroy(&" + it->name + ");");
        }
        out.push_back(lowered);
        strings.clear();
        update_scope(line);
        continue;
      }

      // Non-string-var return: use cleanup helper
      const std::vector<std::string> cleanup = close_scope_lines(indent, strings, 0);
      const std::vector<std::string> return_lines = append_cleanup_before_return(
          lowered, "dpp_string_return_value", cleanup);
      out.insert(out.end(), return_lines.begin(), return_lines.end());
      update_scope(line);
      continue;
    }

    // ── Scope close: emit destroys before '}' ─────────────────────────────
    {
      const std::size_t opens = count_char(line, '{');
      const std::size_t closes = count_char(line, '}');
      if (!strings.empty() && closes > opens) {
        const std::size_t remaining =
            closes > before_depth + opens ? std::size_t{0} : before_depth + opens - closes;
        const std::vector<std::string> cleanup =
            close_scope_lines(leading_indent(line), strings, remaining);
        out.insert(out.end(), cleanup.begin(), cleanup.end());
      }
    }

    // ── break/continue: emit destroys for current loop scope ──────────────
    if (!loop_depths.empty() && !strings.empty() &&
        (lowered_stripped == "break;" || lowered_stripped == "continue;")) {
      const std::vector<std::string> cleanup =
          peek_destroy_lines(leading_indent(lowered), strings, loop_depths.back());
      out.insert(out.end(), cleanup.begin(), cleanup.end());
    }

    out.push_back(lowered);
    update_scope(line);

    // Track when we exit a record body (typedef struct Foo { ... } Foo;)
    if (inside_record) {
      // A line like "} TypeName;" marks the end of a typedef struct.
      const std::string s = stripped;
      if (!s.empty() && s[0] == '}' && s.find(';') != std::string::npos) {
        inside_record = false;
      }
    }
  }

  result.source = join_lines(out);
  if (!strings.empty()) result.used_string = true;
  return result;
}

} // namespace dpp::convert
