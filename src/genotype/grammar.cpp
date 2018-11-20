#include "grammar.h"

namespace genotype {
namespace grammar {

template <typename... ARGS>
void doThrow (ARGS... args) {
  utils::doThrow<std::invalid_argument, ARGS...>(args...);
}

void checkSuccessor (const std::string &s, const Checkers &checkers) {
  int bracketDepth = 0;
  size_t bracketStart = std::string::npos;

  size_t size = s.size();
  for (size_t i=0; i<size; i++) {
    char c = s[i];

    if (bracketDepth > 0) {
      if (c == '[') bracketDepth++;
      else if (c == ']')  bracketDepth--;
      if (bracketDepth == 0)
        checkSuccessor(s.substr(bracketStart+1, i-bracketStart-1), checkers);

    } else {
      if (isupper(c)) {
        if (!checkers.nonTerminal(c))
          doThrow("Character at pos ", i, " in ", s, " is not a valid non terminal");
      } else if (islower(c)) {
        if (!checkers.terminal(c))
          doThrow("Character at pos ", i, " in ", s, " is not a valid terminal");
      } else if (checkers.control(c)) {
        if (c == ']')
          doThrow("Dandling closing bracket at pos ", i, " in ", s);
        else if (c == '[') {
          bracketStart = i;
          bracketDepth = 1;
        }
      } else
        doThrow("Unrecognized character '", c, "' at pos ", i, " in ", s);
    }
  }
}

void checkRule (const std::string &s, const Checkers &checkers,
                NonTerminal &lhs, Successor &rhs) {
  if (s.size() < 6)
    doThrow("Provided rule '", s, "' is too small");

  lhs = s[0];
  if (!checkers.nonTerminal(lhs))
    doThrow("Invalid predecessor in rule '", s, "'");

  if (s[1] != ' ' || s[2] != '-' || s[3] != '>' || s[4] != ' ')
    doThrow("Invalid separator in rule '", s, "'");

  rhs = s.substr(6);
  checkSuccessor(rhs, checkers);
}

} // end of namespace grammar
} // end of namespace genotype
