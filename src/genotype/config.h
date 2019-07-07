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
struct EDNA_CONFIG_FILE(Metabolism) {
  using decimal = double;
  using Elements = std::array<decimal, EnumUtils<genotype::Element>::size()>;

  using Be = Bounds<Elements>;
  using Bd = Bounds<decimal>;

  DECLARE_PARAMETER(Be, conversionRatesBounds)
  DECLARE_PARAMETER(Be, resistorsBounds)
  DECLARE_PARAMETER(Bd, growthSpeedBounds)
  DECLARE_PARAMETER(Bd, deltaWidthBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DW, distanceWeights)
};

template <>
struct EDNA_CONFIG_FILE(Plant) {
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
  DECLARE_PARAMETER(DW, distanceWeights)

  using Crossover = genotype::BOCData::config_t;
  DECLARE_SUBCONFIG(Crossover, genotypeCrossoverConfig)

  using Metabolism = EDNAConfigFile<genotype::Metabolism>;
  DECLARE_SUBCONFIG(Metabolism, genotypeMetabolismConfig)

  DECLARE_PARAMETER(NonTerminal, ls_axiom)
  DECLARE_PARAMETER(Successor, ls_shootInitRule)
  DECLARE_PARAMETER(Successor, ls_rootInitRule)
  DECLARE_PARAMETER(NonTerminal, ls_maxNonTerminal)
  DECLARE_PARAMETER(uint, ls_maxRuleSize)
  DECLARE_PARAMETER(float, ls_rotationAngle)
  DECLARE_PARAMETER(float, ls_nonTerminalCost)  /// Ratio of 's' cost

  struct OrganSize {
    float width, length;
    float area (void) const { return width * length; }
  };
  using OrgansSizes = std::map<char, OrganSize>;
  DECLARE_PARAMETER(OrgansSizes, ls_terminalsSizes)
  static const OrganSize& sizeOf (char symbol);


  DECLARE_PARAMETER(Bui, ls_recursivityBounds)
  DECLARE_PARAMETER(MutationRates, ls_mutationRates)
  DECLARE_PARAMETER(MutationRates, ls_ruleSetMutationRates)
  DECLARE_PARAMETER(MutationRates, ls_ruleMutationRates)
};
using PlantGenome = EDNAConfigFile<genotype::Plant>;

std::ostream& operator<< (std::ostream &os, const PlantGenome::OrganSize &ts);
std::istream& operator>> (std::istream &is, PlantGenome::OrganSize &ts);

} // end of namespace config

#endif // GENOTYPE_CONFIG_H
