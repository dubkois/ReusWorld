#ifndef GNTP_PLANT_H
#define GNTP_PLANT_H

#include <string>

#include "grammar.h"
#include "config.h"

namespace genotype {

template <LSystemType T>
struct LSystem {
  static constexpr LSystemType type = T;
  using NonTerminal = grammar::NonTerminal;
  using Successor = grammar::Successor;
  using Rule = grammar::Rule_t<T>;
  using Rules = std::map<NonTerminal, Rule>;

  uint recursivity;

  Rules rules;

  Successor successor (NonTerminal symbol) const {
    auto it = rules.find(symbol);
    if (it == rules.end())
      return "";
    return it->second.rhs;
  }

  std::string stringPhenotype (const std::string &axiom) const {
    std::string phenotype;
    for (char c: axiom) {
      if (!Rule::isValidNonTerminal(c))
        phenotype += c;
      else
        phenotype += successor(c);
    }
    return phenotype;
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
public:
  using FloatElements = config_t::FloatElements;
  FloatElements conversionRates; // f_c, f_w
  FloatElements resistors;  // rho_c, rho_w
  float growthSpeed;  // k_g
};
DECLARE_GENOME_FIELD(Metabolism, Metabolism::FloatElements, conversionRates)
DECLARE_GENOME_FIELD(Metabolism, Metabolism::FloatElements, resistors)
DECLARE_GENOME_FIELD(Metabolism, float, growthSpeed)


class Plant : public SelfAwareGenome<Plant> {
  APT_SAG()

public:
  LSystem<SHOOT> shoot;
  LSystem<ROOT> root;

  uint dethklok;

  uint seedsPerFruit;

  BOCData cdata;

  Plant clone (void) const {
    Plant other = *this;
    other.cdata.updateCloneLineage();
    return other;
  }

  grammar::Successor successor (LSystemType t, grammar::NonTerminal nt) {
    switch (t) {
    case SHOOT:  return shoot.successor(nt);
    case ROOT:   return root.successor(nt);
    default:  utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    }
    return {};
  }

  auto maxDerivations (LSystemType t) {
    switch (t) {
    case SHOOT:  return shoot.recursivity;
    case ROOT:  return root.recursivity;
    default:  utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    }
    return 0u;
  }

  static grammar::Checkers checkers (LSystemType t) {
    switch(t) {
    case SHOOT:  return decltype(shoot)::Rule::checkers();
    case ROOT:   return decltype(root)::Rule::checkers();
    default:  utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    }
    return {};
  }

//  Plant();
};

DECLARE_GENOME_FIELD(Plant, LSystem<SHOOT>, shoot)
DECLARE_GENOME_FIELD(Plant, LSystem<ROOT>, root)
DECLARE_GENOME_FIELD(Plant, uint, dethklok)
DECLARE_GENOME_FIELD(Plant, uint, seedsPerFruit)
DECLARE_GENOME_FIELD(Plant, BOCData, cdata)


} // end of namespace genotype

#endif // GNTP_PLANT_H
