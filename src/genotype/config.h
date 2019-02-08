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
  using decimal = double;
  using Elements = std::array<decimal, EnumUtils<genotype::Element>::size()>;

  using Be = Bounds<Elements>;
  using Bd = Bounds<decimal>;

  DECLARE_PARAMETER(Be, conversionRatesBounds)
  DECLARE_PARAMETER(Be, resistorsBounds)
  DECLARE_PARAMETER(Bd, growthSpeedBounds)
  DECLARE_PARAMETER(Bd, deltaWidthBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
};

template <>
struct SAG_CONFIG_FILE(Plant) {
  using NonTerminal = genotype::grammar::NonTerminal;
  using Successor = genotype::grammar::Successor;

  using Bui = Bounds<uint>;
  using Bf = Bounds<float>;
  DECLARE_PARAMETER(Bui, dethklokBounds)
  DECLARE_PARAMETER(Bf, fruitOvershootBounds)
  DECLARE_PARAMETER(Bui, seedsPerFruitBounds)

  DECLARE_PARAMETER(Bf, temperatureOptimalBounds)
  DECLARE_PARAMETER(Bf, temperatureRangeBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)

  using Crossover = genotype::BOCData::config_t;
  DECLARE_SUBCONFIG(Crossover, genotypeCrossoverConfig)

  using Metabolism = SAGConfigFile<genotype::Metabolism>;
  DECLARE_SUBCONFIG(Metabolism, genotypeMetabolismConfig)

  DECLARE_PARAMETER(NonTerminal, ls_axiom)
  DECLARE_PARAMETER(Successor, ls_shootInitRule)
  DECLARE_PARAMETER(Successor, ls_rootInitRule)
  DECLARE_PARAMETER(NonTerminal, ls_maxNonTerminal)
  DECLARE_PARAMETER(uint, ls_maxRuleSize)
  DECLARE_PARAMETER(float, ls_rotationAngle)

  struct TerminalSize {
    float width, length;
  };
  using TerminalsSizes = std::map<char, TerminalSize>;
  DECLARE_PARAMETER(TerminalsSizes, ls_terminalsSizes)
  static const TerminalSize& sizeOf (char symbol);


  DECLARE_PARAMETER(Bui, ls_recursivityBounds)
  DECLARE_PARAMETER(MutationRates, ls_mutationRates)
  DECLARE_PARAMETER(MutationRates, ls_ruleSetMutationRates)
  DECLARE_PARAMETER(MutationRates, ls_ruleMutationRates)
};
using PlantGenome = SAGConfigFile<genotype::Plant>;

std::ostream& operator<< (std::ostream &os, const PlantGenome::TerminalSize &ts);
std::istream& operator>> (std::istream &is, PlantGenome::TerminalSize &ts);

} // end of namespace config

#endif // GENOTYPE_CONFIG_H
