#include "plant.h"


using namespace genotype;
using Config = config::SAGConfigFile<Plant>;

#define GENOME Plant

#define LSYSTEM_FIELD_FUNCTOR(NAME)           \
  lsystemFunctor<decltype(Plant::NAME), \
                 GENOME_FIELD_FUNCTOR(decltype(Plant::NAME), NAME)>()

template <grammar::LSystemType T> struct InitRule;
template <> struct InitRule<grammar::SHOOT> {
  static constexpr auto rule = "G -> s[+l][-l]f";
};
template <> struct InitRule<grammar::ROOT> {
  static constexpr auto rule = "G -> t[+h][-h]";
};

template <typename R, typename Dice>
void mutateLSystemRules (R &r, Dice &dice) {
  assert(false);
}

template <typename R, typename Dice>
void crossLSystemRules (const R &lhs, const R &rhs, R &child, Dice &dice) {
  assert(false);
}

template <typename R>
double distanceLSystemRules (const R &lhs, const R &rhs) {
  assert(false);
}

template <typename R>
bool checkLSystemRules (const R &r) {
  assert(false);
}

template <typename LS, typename F>
auto lsystemFunctor (void) {
  using Rule = typename LS::Rule;

  F functor {};
  const auto &lsRBounds = Config::ls_recursivityBounds();

  functor.random = [&lsRBounds] (auto &dice) {
    return LS {
      lsRBounds.rand(dice),
      {Rule::fromString(InitRule<LS::type>::rule)}
    };
  };

  functor.mutate = [&lsRBounds] (auto &ls, auto &dice) {
    std::string field = dice(Config::ls_mutationRates())->first;
    if (field == "recursivity")
      lsRBounds.mutate(ls.recursivity, dice);

    else if (field == "rules")
      mutateLSystemRules(ls.rules, dice);

    else
      throw std::logic_error("Invalid field for LSystem: " + field);
  };

  functor.cross = [&lsRBounds] (auto &lhs, auto &rhs, auto &dice) {
    LS child;
    child.recursivity = dice.toss(lhs.recursivity, rhs.recursivity);
    crossLSystemRules(lhs.rules, rhs.rules, child.rules, dice);
    return child;
  };

  functor.distance = [&lsRBounds] (auto &lhs, auto &rhs) {
    double d = lsRBounds.distance(lhs.recursivity, rhs.recursivity);
    d += distanceLSystemRules(lhs.rules, rhs.rules);
    return d;
  };

  functor.check = [&lsRBounds] (auto &ls) {
    return lsRBounds.check(ls.recursivity)
        && checkLSystemRules(ls.rules);
  };

  return functor;
}

//DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, recursivity, "", 1u, 2u, 2u, 5u)
//DEFINE_GENOME_FIELD_WITH_FUNCTOR(LSystem::Rules, rules, "", rulesFunctor())

//DEFINE_GENOME_MUTATION_RATES({
//  MUTATION_RATE(recursivity, 1.f)
//})
//#undef GENOME

/// Manually managed data
#define CFILE Config
DEFINE_PARAMETER(uint, ls_maxRules, 7)
DEFINE_PARAMETER(genotype::grammar::Successor, ls_shootInitRule, InitRule<grammar::SHOOT>::rule)
DEFINE_PARAMETER(genotype::grammar::Successor, ls_rootInitRule, InitRule<grammar::ROOT>::rule)
DEFINE_PARAMETER(float, ls_rotationAngle, M_PI/6.)
DEFINE_PARAMETER(float, ls_segmentWidth, .01)
DEFINE_PARAMETER(float, ls_segmentLength, .1)

DEFINE_PARAMETER(Config::Bui, ls_recursivityBounds, 1u, 2u, 2u, 5u)
DEFINE_MAP_PARAMETER(Config::MutationRates, ls_mutationRates, {
  { "recursivity", 1.f }, { "rules", Config::ls_maxRules() }
})
#undef CFILE

/// Auto-managed
DEFINE_GENOME_FIELD_WITH_FUNCTOR(LSystem<grammar::SHOOT>, shoot, "", LSYSTEM_FIELD_FUNCTOR(shoot))
DEFINE_GENOME_FIELD_WITH_FUNCTOR(LSystem<grammar::ROOT>, root, "", LSYSTEM_FIELD_FUNCTOR(root))

DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, dethklok, "", 10u, 100u, 100u, 1000u)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, seedsPerFruit, "", 1u, 3u, 3u, 100u)

DEFINE_GENOME_FIELD_AS_SUBGENOME(BOCData, cdata, "")

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(shoot, 1.f),
  MUTATION_RATE(root, 1.f),
  MUTATION_RATE(seedsPerFruit, 1.f),
  MUTATION_RATE(dethklok, 1.f),
  MUTATION_RATE(cdata, 1.f)
})

#undef GENOME
