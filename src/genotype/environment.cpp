#include <bitset>

#include "environment.h"

using namespace genotype;
using CGP = Environment::CGP;
#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, rngSeed, "", 0u, std::numeric_limits<uint>::max())
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, width, "", 10.f, 100.f, 100.f, 1000.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, depth, "", 25, 25)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, minT, "", -20, -20)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, maxT, "", 40, 40)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, voxels, "", 100, 100)

DEFINE_GENOME_FIELD_WITH_BOUNDS(float, inertia, "", 0.f, .9f, .9f, 1.f)

auto cgpFunctor = [] {
  GENOME_FIELD_FUNCTOR(CGP, controller) functor;
  functor.random = [] (auto &dice) { return CGP::null(dice); };
  functor.mutate = [] (auto &cgp, auto &dice) { cgp.mutate(dice); };
  functor.check = [] (auto&) { return true; };
  functor.cross = [] (auto&, auto&, auto&) { assert(false); return CGP(); };
  functor.distance = [] (auto&, auto&) { assert(false); return NAN; };
  return functor;
};
template <>
struct genotype::MutationRatesPrinter<CGP, Environment, &Environment::controller> {
  static constexpr bool recursive = false;
  static void print (std::ostream&, uint, uint, float) {}
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(CGP, controller, "cgp", cgpFunctor())

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(   rngSeed, 0),
  EDNA_PAIR(     width, 0),
  EDNA_PAIR(     depth, 0),
  EDNA_PAIR(      minT, 0),
  EDNA_PAIR(      maxT, 0),
  EDNA_PAIR(    voxels, 0),
  EDNA_PAIR(   inertia, 0),
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

using I = genotype::cgp::Inputs;
using IU = EnumUtils<I>;
DEFINE_CONTAINER_PARAMETER(CFILE::ActiveInputs, cgpActiveInputs,
                           IU::iterator())

using O = genotype::cgp::Outputs;
using OU = EnumUtils<O>;
DEFINE_CONTAINER_PARAMETER(CFILE::ActiveOutputs, cgpActiveOutputs,
                           OU::iterator())

DEFINE_SUBCONFIG(config::CGP, genotypeEnvCtrlConfig)

template <typename E>
auto build (const std::set<E> &values) {
  using U = EnumUtils<E>;
  std::bitset<U::size()> bs;
  bs.reset();
  for (E v: values) bs.set(U::toUnderlying(v), true);
  return bs;
}

bool CGP::isActiveInput(uint i) {
  static const auto active = build(CFILE::cgpActiveInputs());
  return active.test(i);
}

bool CGP::isActiveOutput(uint i) {
  static const auto active = build(CFILE::cgpActiveOutputs());
  return active.test(i);
}

#undef CFILE
}

#undef GENOME
