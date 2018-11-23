#ifndef GENOTYPE_CONFIG_H
#define GENOTYPE_CONFIG_H

#include "kgd/apt/core/crossover.h"

namespace genotype {
struct Plant;

namespace grammar {

using NonTerminal = char;
using Successor = std::string;
using Symbols = std::set<char>;

} // end of namespace grammar
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

  DECLARE_PARAMETER(genotype::grammar::NonTerminal, ls_axiom)
  DECLARE_PARAMETER(genotype::grammar::Successor, ls_shootInitRule)
  DECLARE_PARAMETER(genotype::grammar::Successor, ls_rootInitRule)
  DECLARE_PARAMETER(char, ls_maxNonTerminal)
  DECLARE_PARAMETER(uint, ls_maxRuleSize)
  DECLARE_PARAMETER(float, ls_rotationAngle)
  DECLARE_PARAMETER(float, ls_segmentWidth)
  DECLARE_PARAMETER(float, ls_segmentLength)

  DECLARE_PARAMETER(Bui, ls_recursivityBounds)
  DECLARE_PARAMETER(MutationRates, ls_mutationRates)
  DECLARE_PARAMETER(MutationRates, ls_ruleSetMutationRates)
  DECLARE_PARAMETER(MutationRates, ls_ruleMutationRates)
};
using PlantGenome = SAGConfigFile<genotype::Plant>;

} // end of namespace config

#endif // GENOTYPE_CONFIG_H
