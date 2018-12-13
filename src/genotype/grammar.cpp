#include <iostream>

#include "grammar.h"

namespace genotype {
namespace grammar {

const Symbols Rule_base::controls {
  '[', ']', '+', '-'
};

template <typename... ARGS>
void doThrow (ARGS... args) {
  utils::doThrow<std::invalid_argument, ARGS...>(args...);
}

Successor toSuccessor (NonTerminal t) {
  return std::string(1, t);
}

std::string extractBranch(const std::string &s, size_t start, size_t &end) {
  static const auto npos = std::string::npos;
  uint bracketDepth = 0;
  end = npos;

  for (size_t i=start; i<s.size() && end == npos; i++) {
    char c = s[i];
    if (c == '[')
      bracketDepth++;
    else if (c == ']')
      bracketDepth--;

    if (bracketDepth == 0)
      end = i;
  }

  if (end == npos)
    doThrow("Not matching closing bracket for ", s, ":", start);

  return s.substr(start+1, end-start-1);
}

void checkPremice(NonTerminal lhs, const Checkers &checkers) {
  if (!checkers.nonTerminal(lhs))
    doThrow("Invalid predecessor '", lhs, "'");
}

void checkSuccessor (const Successor &rhs, const Checkers &checkers) {
  size_t size = rhs.size();
  for (size_t i=0; i<size; i++) {
    char c = rhs[i];

    if (isupper(c)) {
      if (!checkers.nonTerminal(c))
        doThrow("Character at pos ", i, " in ", rhs, " is not a valid non terminal");
    } else if (islower(c)) {
      if (!checkers.terminal(c))
        doThrow("Character at pos ", i, " in ", rhs, " is not a valid terminal");
    } else if (checkers.control(c)) {
      if (c == ']')
        doThrow("Dandling closing bracket at pos ", i, " in ", rhs);
      else if (c == '[') {
        checkSuccessor(extractBranch(rhs, i, i), checkers);
      }
    } else
      doThrow("Unrecognized character '", c, "' at pos ", i, " in ", rhs);
  }
}

void checkRule (const std::string &s, const Checkers &checkers,
                NonTerminal &lhs, Successor &rhs) {
  if (s.size() < 6)
    doThrow("Provided rule '", s, "' is too small");

  lhs = s[0];
  checkPremice(lhs, checkers);

  if (s[1] != ' ' || s[2] != '-' || s[3] != '>' || s[4] != ' ')
    doThrow("Invalid separator in rule '", s, "'");

  rhs = s.substr(5);
  checkSuccessor(rhs, checkers);
}

} // end of namespace grammar
} // end of namespace genotype
