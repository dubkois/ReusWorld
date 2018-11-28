#ifndef GENOTYPE_CONFIG_H
#define GENOTYPE_CONFIG_H

#include "kgd/apt/core/crossover.h"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(genotype, Element, GLUCOSE = 0, WATER = 1)
DEFINE_NAMESPACE_PRETTY_ENUMERATION(genotype, LSystemType, SHOOT = 0, ROOT = 1)


namespace genotype {
struct Metabolism;
struct Plant;

namespace grammar {

using NonTerminal = char;
using Successor = std::string;
using Symbols = std::set<char>;

} // end of namespace grammar
} // end of namespace genotype


namespace config {

template <>
struct SAG_CONFIG_FILE(Metabolism) {
  template <typename T>
  using Elements_array = std::array<T, EnumUtils<genotype::Element>::size()>;

  using FloatElements = Elements_array<float>;

  using Bfe = Bounds<FloatElements>;
  using Bf = Bounds<float>;

  DECLARE_PARAMETER(Bfe, conversionRatesBounds)
  DECLARE_PARAMETER(Bfe, resistorsBounds)
  DECLARE_PARAMETER(Bf, growthSpeedBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
};

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
