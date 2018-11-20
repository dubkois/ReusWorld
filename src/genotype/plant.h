#ifndef GNTP_PLANT_H
#define GNTP_PLANT_H

#include <string>

#include "kgd/apt/core/crossover.h"

#include "grammar.h"

namespace genotype {

template <grammar::LSystemType T>
class LSystem {
public:
  static constexpr grammar::LSystemType type = T;
  using Rule = grammar::Rule_t<T>;
  using Rules = std::set<Rule, std::less<>>;
  using NonTerminal = typename Rule::NonTerminal;
  using Successor = typename Rule::Successor;

  uint recursivity;

  static const typename Rule::NonTerminal axiom;

  Rules rules;

  Successor successor (NonTerminal symbol) {
    auto it = rules.find(symbol);
    if (it == rules.end())
      utils::doThrow<std::invalid_argument>(
        "No rule for symbol '", symbol, " found in ", *this);
    return it->rhs;
  }

  friend std::ostream& operator<< (std::ostream &os, const LSystem &ls) {
    os << "[\n\trec: " << ls.recursivity << "\n";
    for (const Rule &r: ls.rules) os << "\t" << r << "\n";
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

template <grammar::LSystemType T>
const typename LSystem<T>::Rule::NonTerminal LSystem<T>::axiom = 'G';

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

//  Plant();
};

DECLARE_GENOME_FIELD(Plant, LSystem<grammar::SHOOT>, shoot)
DECLARE_GENOME_FIELD(Plant, LSystem<grammar::ROOT>, root)
DECLARE_GENOME_FIELD(Plant, uint, dethklok)
DECLARE_GENOME_FIELD(Plant, uint, seedsPerFruit)
DECLARE_GENOME_FIELD(Plant, BOCData, cdata)


} // end of namespace genotype

namespace config {

template <>
struct SAG_CONFIG_FILE(Plant) {
  using Bui = Bounds<uint>;
  DECLARE_PARAMETER(Bui, dethklokBounds)
  DECLARE_PARAMETER(Bui, seedsPerFruitBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)

  using BOCConfig = genotype::BOCData::config_t;
  DECLARE_SUBCONFIG(BOCConfig, crossoverConfig)

  DECLARE_PARAMETER(uint, ls_maxRules)
  DECLARE_PARAMETER(genotype::grammar::Successor, ls_shootInitRule)
  DECLARE_PARAMETER(genotype::grammar::Successor, ls_rootInitRule)
  DECLARE_PARAMETER(float, ls_rotationAngle)
  DECLARE_PARAMETER(float, ls_segmentWidth)
  DECLARE_PARAMETER(float, ls_segmentLength)

  DECLARE_PARAMETER(Bui, ls_recursivityBounds)
  DECLARE_PARAMETER(MutationRates, ls_mutationRates)
};

} // end of namespace config

#endif // GNTP_PLANT_H
