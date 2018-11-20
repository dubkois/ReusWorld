#ifndef GRAMMAR_H
#define GRAMMAR_H

#include "kgd/settings/prettyenums.hpp"
#include "kgd/external/json.hpp"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(
    genotype::grammar, ShootTerminalSymbols,
    STEM,   // 's'
    LEAF,   // 'l'
    FLOWER  // 'f'
)

DEFINE_NAMESPACE_PRETTY_ENUMERATION(
    genotype::grammar, RootTerminalSymbols,
    TRUNK,  // 't'
    HAIR    // 'h'
)

namespace genotype {
namespace grammar {

enum LSystemType {  SHOOT = 0, ROOT = 1 };

template <LSystemType T> struct TerminalSymbolSet;
template <> struct TerminalSymbolSet<SHOOT> { using enum_t = ShootTerminalSymbols; };
template <> struct TerminalSymbolSet<ROOT> { using enum_t = RootTerminalSymbols; };

struct Checkers {
  using Checker_f = bool (*) (char c);
  Checker_f nonTerminal, terminal, control;
};

using NonTerminal = char;
using Successor = std::string;

void checkSuccessor (const std::string &s, const Checkers &checkers);
void checkRule (const std::string &s, const Checkers &checkers,
                NonTerminal &lhs, Successor &rhs);

template <LSystemType T>
struct Rule_t {
  using NonTerminal = grammar::NonTerminal;
  NonTerminal lhs;

  using Successor = grammar::Successor;
  Successor rhs;

  std::string toString (void) const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
  }

  using Symbols = std::set<char>;
  static Symbols buildTerminalSymbolSet (void) {
    using Enum = typename TerminalSymbolSet<T>::enum_t;
    using EU = EnumUtils<Enum>;

    Symbols s;
    for (Enum e: EU::iterator())
      s.insert(std::tolower(EU::getName(e)[0]));
    return s;
  }

  static bool isValidNonTerminal (char c) {
    static const Symbols symbols {  'A', 'B', 'C', 'D', 'E', 'F', 'G'  };
    return symbols.find(c) != symbols.end();
  }

  static bool isValidControl (char c) {
    static const Symbols symbols {  '[', ']', '+', '-'  };
    return symbols.find(c) != symbols.end();
  }

  static bool isValidTerminal (char c) {
    static const Symbols symbols = buildTerminalSymbolSet();
    return symbols.find(c) != symbols.end();
  }

private:
  static grammar::Checkers checkers (void) {
    return {
      isValidNonTerminal, isValidTerminal, isValidControl
    };
  }

public:
  static Rule_t<T> fromString (const std::string &s) {
    Rule_t<T> r;
    checkRule(s, checkers(), r.lhs, r.rhs);
    return r;
  }

  friend std::ostream& operator<< (std::ostream &os, const Rule_t &r) {
    return os << r.lhs << " -> " << r.rhs;
  }

  friend bool operator< (const Rule_t &lhs, const Rule_t &rhs) {
    return lhs.lhs < rhs.lhs;
  }

  friend bool operator< (const Rule_t &lhs, const NonTerminal &rhs) {
    return lhs.lhs < rhs;
  }

  friend bool operator< (const NonTerminal &lhs, const Rule_t &rhs) {
    return lhs < rhs.lhs;
  }

  friend bool operator== (const Rule_t &lhs, const Rule_t &rhs) {
    return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs;
  }

  friend void to_json (nlohmann::json &j, const Rule_t &r) {
    j = r.toString();
  }

  friend void from_json (const nlohmann::json &j, Rule_t<T> &r) {
    r = Rule_t<T>::fromString(j.get<std::string>());
  }
};

} // end of namespace grammar
} // end of namespace genotype

#endif // GRAMMAR_H
