#include <regex>

#include "plant.h"


using namespace genotype;
using Config = config::PlantGenome;

static constexpr bool debugMutations = false;

template <LSystemType T> struct InitRule;
template <> struct InitRule<SHOOT> {
  static auto rule (void) { return Config::ls_shootInitRule(); }
};
template <> struct InitRule<ROOT> {
  static auto rule (void) { return Config::ls_rootInitRule(); }
};

uint maxRuleCount (void) {
  return uint(Config::ls_maxNonTerminal() - 'A' + 2);
}

void replace (std::string &s, const std::string &from, const std::string &to) {
  std::regex_replace(s, std::regex(from), to);
}

void remove (std::string &s, const std::string &from) {
  replace(s, from, "");
}

void remove (std::string &s, char c) {
  remove(s, grammar::toSuccessor(c));
}

template <typename Dice>
size_t nonBracketSymbol (const std::string &s, Dice &dice) {
  std::vector<float> weights (s.size(), 1);
  for (uint i=0; i<s.size(); i++)
    if(grammar::Rule_base::isBracket(s[i]))
      weights[i] = 0;

  return dice(rng::rdist(weights.begin(), weights.end()));
}

template <typename R, typename Dice>
void mutateLSystemRule (R &rule, const grammar::Symbols &nonTerminals, Dice &dice) {
  auto sizeWCC = rule.sizeWithoutControlChars();

  bool swappable = false;
  std::vector<float> swapWeights (rule.rhs.size()-1, 0);
  for (uint i=0; i<rule.rhs.size()-1; i++)
    swapWeights[i] = swappable
                   = !grammar::Rule_base::isBracket(rule.rhs[i])
                  && !grammar::Rule_base::isBracket(rule.rhs[i+1]);

  auto ruleRates = Config::ls_ruleMutationRates();
  ruleRates["dupSymb"] *= (sizeWCC < Config::ls_maxRuleSize());
//  ruleRates["swpSymb"] // Always true (at least should be)
  ruleRates["swpSymb"] *= swappable;
//  ruleRates["brkSymb"] // Idem
  ruleRates["delSymb"] *= (sizeWCC > 1);

  std::string field = dice.pickOne(ruleRates);

  if (debugMutations)  std::cerr << "\tMutation " << field << " on '" << rule;

  if (field == "dupSymb") {
    size_t i = nonBracketSymbol(rule.rhs, dice);
    rule.rhs.replace(i, 1, std::string(2, rule.rhs[i]));

  } else if (field == "mutSymb") {
    size_t i = nonBracketSymbol(rule.rhs, dice);
    char c = R::nonBracketSymbol(nonTerminals, dice);
    rule.rhs[i] = c;
    assert(R::isValidNonTerminal(c) || R::isValidTerminal(c) || R::isValidControl(c));

  } else if (field == "swpSymb") {

    size_t i = dice(rng::rdist(swapWeights.begin(), swapWeights.end()));
    std::swap(rule.rhs[i], rule.rhs[i+1]);

  } else if (field == "brkSymb") {
    size_t i = nonBracketSymbol(rule.rhs, dice);
    rule.rhs.insert(i, "[");
    rule.rhs.insert(i+2, "]");

  } else if (field == "delSymb") {
    size_t i = nonBracketSymbol(rule.rhs, dice);
    if (i > 0 && i+1 < rule.rhs.size() && rule.rhs[i-1] == '[' && rule.rhs[i+1] == ']')
      rule.rhs.replace(i-1, 3, "");
    else
      rule.rhs.replace(i, 1, "");

  } else
    utils::doThrow<std::logic_error>("Unhandled field '", field,
                                     "' for lsystem rule mutation");

  if (debugMutations)  std::cerr << " >> " << rule << std::endl;
}

template <typename R, typename Dice>
void mutateLSystemRules (R &rules, Dice &dice) {
  using Rule = typename R::mapped_type;

  // First check bounds
  auto ruleSetRates = Config::ls_ruleSetMutationRates();
  ruleSetRates["addRule"] *= (rules.size() < maxRuleCount());
  ruleSetRates["delRule"] *= (rules.size() > 2);

  std::string field = dice.pickOne(ruleSetRates);
  if (field == "addRule") {
    // Pick a rule
    Rule &r = dice(rules)->second;

    // Pick a non bracket char
    size_t i = nonBracketSymbol(r.rhs, dice);

    // Find next non-terminal
    char c = 'A';
    while (rules.find(c) != rules.end() && c <= Config::ls_maxNonTerminal())
      c++;

    rules[c] = Rule::fromTokens(c, grammar::Successor(1, r.rhs[i]));
    r.rhs[i] = c;

  } else if (field == "delRule") {
    // Pick a rule (except G)
    auto rit = dice(rules.begin(), std::prev(rules.end()));
    Rule r = rit->second;
    rules.erase(rit);

    assert(r.lhs != Config::ls_axiom());

    // Remove all reference
    for (auto &p: rules)
      remove(p.second.rhs, r.lhs);

  } else if (field == "mutRule") {
    grammar::Symbols nonTerminals;
    for (const auto &p: rules)
      if (p.second.lhs != Config::ls_axiom())
        nonTerminals.insert(p.second.lhs);

    Rule &r = dice(rules.begin(), rules.end())->second;
    mutateLSystemRule(r, nonTerminals, dice);

  } else
    utils::doThrow<std::logic_error>("Unhandled field '", field,
                                     "' for lsystem rules mutation");
}

template <typename R, typename Dice>
void crossLSystemRules (const R &lhs, const R &rhs, R &child, Dice &dice) {
  std::set<grammar::NonTerminal> nonTerminals;
  for (auto &p: lhs)  nonTerminals.insert(p.first);
  for (auto &p: rhs)  nonTerminals.insert(p.first);
  for (grammar::NonTerminal nt: nonTerminals) {
    auto itLhs = lhs.find(nt), itRhs = rhs.find(nt);
    if (itLhs == lhs.end()) {
      if (dice(.5)) child.insert(*itRhs);
    } else if (itRhs == rhs.end()) {
      if (dice(.5)) child.insert(*itLhs);
    } else
      child.insert(dice.toss(*itLhs, *itRhs));
  }
}

template <typename R>
double distanceLSystemRules (const R &lhs, const R &rhs) {
  double d = 0;
  std::set<grammar::NonTerminal> nonTerminals;
  for (auto &p: lhs)  nonTerminals.insert(p.first);
  for (auto &p: rhs)  nonTerminals.insert(p.first);
  for (grammar::NonTerminal nt: nonTerminals) {
    auto itLhs = lhs.find(nt), itRhs = rhs.find(nt);
    if (itLhs != lhs.end() && itRhs != rhs.end()) {
      uint i;
      double d_ = 0;
      auto rLhs = itLhs->second, rRhs = itRhs->second;
      for (i=0; i<rLhs.size() && i<rRhs.size(); i++)
        d_ += (rLhs.rhs[i] != rRhs.rhs[i]);
      for (; i<rLhs.size(); i++)  d_ += 1;
      for (; i<rRhs.size(); i++)  d_ += 1;
      d += d_ / double(2 * Config::ls_maxRuleSize());

    } else {
      d += 1;
    }
  }
  return d / double(maxRuleCount());
}

template <typename R>
bool checkLSystemRules (const R &r) {
  using Rule = typename R::mapped_type;
  static const auto &checkers = Rule::checkers();
  if (r.size() > maxRuleCount())  return false;
  for (auto &p: r) {
    if (!Rule::isValidNonTerminal(p.first)) {
      std::cerr << "ERROR: " << p.first << " is not a valid non terminal" << std::endl;
      return false;
    }
    if (p.second.sizeWithoutControlChars() > Config::ls_maxRuleSize()) {
      std::cerr << "ERROR: " << p.second << " is longer than limit ("
                << Config::ls_maxRuleSize() << ")" << std::endl;
      return false;
    }
    grammar::checkSuccessor(p.second.rhs, checkers);
  }
  return true;
}

template <typename LS, typename F>
auto lsystemFunctor (void) {
  using Rule = typename LS::Rule;

  F functor {};
  const auto &lsRBounds = Config::ls_recursivityBounds();

  functor.random = [&lsRBounds] (auto &dice) {
    return LS {
      lsRBounds.rand(dice),
      {Rule::fromString(InitRule<LS::type>::rule()).toPair()}
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

namespace nlohmann {

template <typename T>
using Map = std::map<char, T>;

template <typename T>
struct adl_serializer<Map<T>> {
  static void to_json(json& j, const Map<T> &m) {
    for (const auto &p: m)
      j[grammar::toSuccessor(p.first)] = p.second;
  }

  static void from_json(const json& j, Map<T> &m) {
    for (const auto &jp: j.items())
      m[jp.key()[0]] = jp.value();
  }
};
}

/// ============================================================================
/// == Metabolism

#define GENOME Metabolism

using ME = Metabolism::Elements;
DEFINE_GENOME_FIELD_WITH_BOUNDS(ME, conversionRates, "",
                                utils::uniformStdArray<ME>(1e-3),
                                utils::uniformStdArray<ME>(1))
DEFINE_GENOME_FIELD_WITH_BOUNDS(ME, resistors, "",
                                utils::uniformStdArray<ME>(0),
                                utils::uniformStdArray<ME>(1),
                                utils::uniformStdArray<ME>(1),
                                utils::uniformStdArray<ME>(10))
DEFINE_GENOME_FIELD_WITH_BOUNDS(Metabolism::decimal, growthSpeed, "", 0., 1., 1., 10.)
DEFINE_GENOME_FIELD_WITH_BOUNDS(Metabolism::decimal, deltaWidth, "", 0., .2, .2, 1.)

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(conversionRates, 2.f),
  MUTATION_RATE(      resistors, 2.f),
  MUTATION_RATE(    growthSpeed, 1.f),
  MUTATION_RATE(     deltaWidth, 1.f)
})
#undef GENOME

/// ============================================================================
/// == Plant

#define GENOME Plant

#define LSYSTEM_FIELD_FUNCTOR(NAME)           \
  lsystemFunctor<decltype(Plant::NAME), \
                 GENOME_FIELD_FUNCTOR(decltype(Plant::NAME), NAME)>()

/// Manually managed data
#define CFILE Config
auto initRule = [] (const std::string &rhs) {
  return grammar::toSuccessor(Config::ls_axiom()) + " -> " + rhs;
};
DEFINE_PARAMETER(Config::NonTerminal, ls_axiom, 'S')
DEFINE_PARAMETER(Config::Successor, ls_shootInitRule, initRule("s[+l][-l]f"))
DEFINE_PARAMETER(Config::Successor, ls_rootInitRule, initRule("[+h][-h]"))
DEFINE_PARAMETER(Config::NonTerminal, ls_maxNonTerminal, 'F')
DEFINE_PARAMETER(uint, ls_maxRuleSize, 10)
DEFINE_PARAMETER(float, ls_rotationAngle, M_PI/6.)

static const auto tsize = [] (float wr = 1, float lr = 1) {
  return Config::TerminalSize{ .01f * wr, .1f * lr };
};
DEFINE_MAP_PARAMETER(Config::TerminalsSizes, ls_terminalsSizes, {
  { 's', tsize() },
  { 'l', tsize() },
  { 'f', tsize(1, .5) },
  { 'g', tsize(2, .2) },

  { 't', tsize() },
  { 'h', tsize() }
})
const Config::TerminalSize& Config::sizeOf (char symbol) {
  static const auto &sizes = ls_terminalsSizes();
  static const auto null = TerminalSize{0,0};
  auto it = sizes.find(symbol);
  if (it != sizes.end())
    return it->second;
  else
    return null;
}


DEFINE_PARAMETER(Config::Bui, ls_recursivityBounds, 1u, 2u, 2u, 5u)
DEFINE_MAP_PARAMETER(Config::MutationRates, ls_mutationRates, {
  { "recursivity", 1.f }, { "rules", maxRuleCount() }
})
DEFINE_MAP_PARAMETER(Config::MutationRates, ls_ruleSetMutationRates, {
  { "addRule", 2.f },
  { "delRule", 1.f },
  { "mutRule", 7.f },
})
DEFINE_MAP_PARAMETER(Config::MutationRates, ls_ruleMutationRates, {
  { "dupSymb", 1.f },
  { "mutSymb", 4.f },
  { "swpSymb", 3.f },
  { "brkSymb", .5f },
  { "delSymb", .5f },
})


/// Config file hierarchy
DEFINE_SUBCONFIG(Config::Crossover, genotypeCrossoverConfig)
DEFINE_SUBCONFIG(Config::Metabolism, genotypeMetabolismConfig)
#undef CFILE


/// Auto-managed
DEFINE_GENOME_FIELD_AS_SUBGENOME(BOCData, cdata, "")

DEFINE_GENOME_FIELD_WITH_FUNCTOR(LSystem<SHOOT>, shoot, "", LSYSTEM_FIELD_FUNCTOR(shoot))
DEFINE_GENOME_FIELD_WITH_FUNCTOR(LSystem<ROOT>, root, "", LSYSTEM_FIELD_FUNCTOR(root))

DEFINE_GENOME_FIELD_AS_SUBGENOME(Metabolism, metabolism, "")
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, dethklok, "", 10u, 10u, 10u, 1000u)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, fruitOvershoot, "", 1.f, 1.1f, 1.1f, 2.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, seedsPerFruit, "", 1u, 3u, 3u, 100u)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, temperatureOptimal, "", -20.f, 10.f, 10.f, 40.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, temperatureRange, "", 1e-3f, 10.f, 10.f, 30.f)


DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(              cdata, 4.f),
  MUTATION_RATE(              shoot, 8.f),
  MUTATION_RATE(               root, 8.f),
  MUTATION_RATE(         metabolism, 4.f),
  MUTATION_RATE(           dethklok, 1.f),
  MUTATION_RATE(     fruitOvershoot, .5f),
  MUTATION_RATE(      seedsPerFruit, .5f),
  MUTATION_RATE( temperatureOptimal, .5f),
  MUTATION_RATE(   temperatureRange, .5f),
})

namespace config {
std::ostream& operator<< (std::ostream &os, const Config::TerminalSize &ts) {
  return os << ts.width << 'x' << ts.length;
}
std::istream& operator>> (std::istream &is, Config::TerminalSize &ts) {
  char c;
  is >> ts.width >> c >> ts.length;
  return is;
}
} // end of namespace config

#undef GENOME
