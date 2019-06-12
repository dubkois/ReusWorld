#include "environment.h"

using namespace genotype;
using CGP = Environment::CGP;
#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, rngSeed, "", 0u, std::numeric_limits<uint>::max())
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, width, "", 10.f, 1000.f, 1000.f, 10000.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, depth, "", 50, 50)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, minT, "", -20, -20)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, maxT, "", 40, 40)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, voxels, "", 100, 100)

auto cgpFunctor = [] {
  GENOME_FIELD_FUNCTOR(CGP, controller) functor;
  functor.random = [] (auto &dice) { return CGP::null(dice); };
  functor.mutate = [] (auto &cgp, auto &dice) { cgp.mutate(dice); };
//  functor.cross = [] (auto&, auto&, auto&) { assert(false); };
//  functor.distance = [] (auto&, auto&) { assert(false); };
//  functor.check = [] (auto&) { assert(false); };
  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(CGP, controller, "cgp", cgpFunctor())

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(   rngSeed, .1f),
  EDNA_PAIR(     width, .1f),
  EDNA_PAIR(     depth, .1f),
  EDNA_PAIR(      minT, .1f),
  EDNA_PAIR(      maxT, .1f),
  EDNA_PAIR(    voxels, .1f),
  EDNA_PAIR(controller, 1.f)
})

template <>
struct genotype::Extractor<CGP> {
  std::string operator() (const CGP &/*cgp*/, const std::string &/*field*/) {
    return "N/A";
  }
};

template <>
struct genotype::Aggregator<CGP, Environment> {
  void operator() (std::ostream &, const std::vector<Environment> &,
                   std::function<const CGP& (const Environment&)>, uint) {}
};

namespace config {
#define CFILE config::EDNAConfigFile<genotype::Environment>
DEFINE_SUBCONFIG(config::CGP, genotypeEnvCtrlConfig)
#undef CFILE
}

#undef GENOME
