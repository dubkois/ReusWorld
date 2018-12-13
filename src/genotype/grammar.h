#ifndef GRAMMAR_H
#define GRAMMAR_H

#include "kgd/settings/prettyenums.hpp"
#include "kgd/external/json.hpp"

#include "config.h"

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

template <LSystemType T> struct TerminalSymbolSet;
template <> struct TerminalSymbolSet<SHOOT> { using enum_t = ShootTerminalSymbols; };
template <> struct TerminalSymbolSet<ROOT> { using enum_t = RootTerminalSymbols; };

struct Checkers {
  using Checker_f = bool (*) (char c);
  Checker_f nonTerminal, terminal, control;
};

Successor toSuccessor (NonTerminal t);

std::string extractBranch(const std::string &s, size_t start, size_t &end);

void checkPremice (NonTerminal lhs, const Checkers &checkers);
void checkSuccessor (const Successor &rhs, const Checkers &checkers);
void checkRule (const std::string &s, const Checkers &checkers,
                NonTerminal &lhs, Successor &rhs);

struct Rule_base {
  NonTerminal lhs;
  Successor rhs;

  static bool isBracket (char c) {
    return c == '[' || c == ']';
  }

  std::string toString (void) const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
  }

  auto size (void) const {
    return rhs.size();
  }

  static auto sizeWithoutControlChars (const std::string &rhs) {
    size_t size = 0;
    for (char c: rhs)
      size += !isValidControl(c);
    return size;
  }

  auto sizeWithoutControlChars (void) const {
    return sizeWithoutControlChars(rhs);
  }

  static bool isValidNonTerminal (char c) {
    return nonTerminals().find(c) != nonTerminals().end();
  }

  static bool isValidControl (char c) {
    return controls.find(c) != controls.end();
  }

  static auto fruitSymbol (void) {
    return 'g';
  }

  friend std::ostream& operator<< (std::ostream &os, const Rule_base &r) {
    return os << r.lhs << " -> " << r.rhs;
  }

private:
  static const Symbols& nonTerminals (void) {
    static const Symbols symbols = [] {
      Symbols symbols;
      for (char c = 'A'; c <= config::PlantGenome::ls_maxNonTerminal(); c++)
        symbols.insert(c);
      symbols.insert(config::PlantGenome::ls_axiom());
      return symbols;
    }();
    return symbols;
  }
  static const Symbols controls;
};

template <LSystemType T>
struct Rule_t : public Rule_base {
  Rule_t (void) : Rule_base{'?', "?"} {}

  std::pair<NonTerminal, Rule_t> toPair (void) const {
    return std::make_pair(lhs, *this);
  }

  static bool isValidTerminal (char c) {
    return terminals.find(c) != terminals.end();
  }

  template <typename Dice>
  static auto nonBracketSymbol (const Symbols &nonTerminals, Dice &dice) {
    static const Symbols base = [] {
      Symbols symbols;
      symbols.insert(terminals.begin(), terminals.end());
      for (char c: controls)
        if (!isBracket(c))
          symbols.insert(c);
      return symbols;
    }();
    Symbols nonBrackets = base;
    nonBrackets.insert(nonTerminals.begin(), nonTerminals.end());
    return *dice(nonBrackets);
  }

  static auto checkers (void) {
    grammar::Checkers checkers {};
    checkers.nonTerminal = isValidNonTerminal;
    checkers.terminal = isValidTerminal;
    checkers.control = isValidControl;
    return checkers;
  }

  static Rule_t fromString (const std::string &s) {
    Rule_base r;
    checkRule(s, checkers(), r.lhs, r.rhs);
    return Rule_t(r);
  }

  static Rule_t fromTokens (NonTerminal lhs, const Successor &rhs) {
    Rule_base r{lhs,rhs};
    checkPremice(r.lhs, checkers());
    checkSuccessor(r.rhs, checkers());
    return Rule_t(r);
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

private:
  static const Symbols terminals;

  Rule_t (const Rule_base &r) : Rule_base(r) {}
};

template <LSystemType T>
const Symbols Rule_t<T>::terminals = [] {
  using Enum = typename TerminalSymbolSet<T>::enum_t;
  using EU = EnumUtils<Enum>;

  Symbols s;
  for (Enum e: EU::iterator())
    s.insert(std::tolower(EU::getName(e)[0]));
  return s;
}();

} // end of namespace grammar
} // end of namespace genotype

#endif // GRAMMAR_H
