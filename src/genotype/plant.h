#ifndef GNTP_PLANT_H
#define GNTP_PLANT_H

#include <string>

#include "grammar.h"

namespace genotype {

template <grammar::LSystemType T>
class LSystem {
public:
  static constexpr grammar::LSystemType type = T;
  using NonTerminal = grammar::NonTerminal;
  using Successor = grammar::Successor;
  using Rule = grammar::Rule_t<T>;
  using Rules = std::map<NonTerminal, Rule>;

  uint recursivity;

  Rules rules;

  Successor successor (NonTerminal symbol) {
    auto it = rules.find(symbol);
    if (it == rules.end())
      return std::string(1, symbol);
    return it->second.rhs;
  }

  friend std::ostream& operator<< (std::ostream &os, const LSystem &ls) {
    os << "[\n\trec: " << ls.recursivity << "\n";
    for (const auto &p: ls.rules) os << "\t" << p.second << "\n";
    return os << "]";
  }

  friend bool operator== (const LSystem &lhs, const LSystem &rhs) {
    return lhs.recursivity == rhs.recursivity
        && lhs.rules == rhs.rules;
  }

  friend void to_json (nlohmann::json &j, const LSystem<T> &ls) {
    j["recursivity"] = ls.recursivity;
    j["rules"] = ls.rules;
  }

  friend void from_json (const nlohmann::json &j, LSystem<T> &ls) {
    ls.recursivity = j["recursivity"];
    ls.rules = j["rules"].get<Rules>();
  }
};

class Metabolism : public SelfAwareGenome<Metabolism> {
  APT_SAG()

};

class Plant : public SelfAwareGenome<Plant> {
  APT_SAG()

public:
  LSystem<grammar::SHOOT> shoot;
  LSystem<grammar::ROOT> root;

  uint dethklok;

  uint seedsPerFruit;

  BOCData cdata;

  Plant clone (void) const {
    Plant other = *this;
    other.cdata.updateCloneLineage();
    return other;
  }

  grammar::Successor successor (grammar::LSystemType t, grammar::NonTerminal nt) {
    switch (t) {
    case grammar::SHOOT:  return shoot.successor(nt);
    case grammar::ROOT:   return root.successor(nt);
    default:  utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    }
    return {};
  }

  auto maxDerivations (grammar::LSystemType t) {
    switch (t) {
    case grammar::SHOOT:  return shoot.recursivity;
    case grammar::ROOT:  return root.recursivity;
    default:  utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    }
    return 0u;
  }

  static grammar::Checkers checkers (grammar::LSystemType t) {
    switch(t) {
    case grammar::SHOOT:  return decltype(shoot)::Rule::checkers();
    case grammar::ROOT:   return decltype(root)::Rule::checkers();
    default:  utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    }
    return {};
  }

//  Plant();
};

DECLARE_GENOME_FIELD(Plant, LSystem<grammar::SHOOT>, shoot)
DECLARE_GENOME_FIELD(Plant, LSystem<grammar::ROOT>, root)
DECLARE_GENOME_FIELD(Plant, uint, dethklok)
DECLARE_GENOME_FIELD(Plant, uint, seedsPerFruit)
DECLARE_GENOME_FIELD(Plant, BOCData, cdata)


} // end of namespace genotype

#endif // GNTP_PLANT_H
