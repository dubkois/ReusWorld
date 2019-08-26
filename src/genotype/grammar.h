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
  using Checker_f = bool (*) (Symbol s);
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

  static bool isBracket (Symbol s) {
    return s == '[' || s == ']';
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
    for (Symbol s: rhs)
      size += !isValidControl(s);
    return size;
  }

  auto sizeWithoutControlChars (void) const {
    return sizeWithoutControlChars(rhs);
  }

  static bool isValidNonTerminal (Symbol s) {
    return nonTerminals().find(s) != nonTerminals().end();
  }

  static bool isValidControl (Symbol s) {
    return controls.find(s) != controls.end();
  }

  static bool isTerminal (Symbol s) {
    return std::islower(s);
  }

  static bool isStructural (Symbol s) {
    return s == 's' || s == 't';
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
      for (Symbol s = 'A'; s <= config::PlantGenome::ls_maxNonTerminal(); s++)
        symbols.insert(s);
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

  static bool isValidTerminal (Symbol s) {
    return terminals.find(s) != terminals.end();
  }

  template <typename Dice>
  static auto nonBracketSymbol (char original, const Symbols &nonTerminals,
                                Dice &dice, bool controlOnly) {
    static const Symbols nonBracketsControl = [] {
      Symbols symbols;
      for (Symbol s: controls)
        if (!isBracket(s))
          symbols.insert(s);
      return symbols;
    }();
    Symbols candidates = nonBracketsControl;
    if (!controlOnly) {
      candidates.insert(terminals.begin(), terminals.begin());
      candidates.insert(nonTerminals.begin(), nonTerminals.end());
    }
    candidates.erase(original);
    return *dice(candidates);
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
