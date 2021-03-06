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
size_t randomPosNotIn(const std::string &s, Dice &dice, bool (*p) (char)) {
  std::vector<float> weights (s.size(), 1);
  for (uint i=0; i<s.size(); i++) if(p(s[i])) weights[i] = 0;
  return dice(rng::rdist(weights.begin(), weights.end()));
}

template <typename Dice>
size_t nonBracketSymbol (const std::string &s, Dice &dice) {
  return randomPosNotIn(s, dice, grammar::Rule_base::isBracket);
}

template <typename R, typename Dice>
void mutateLSystemRule (R &rule, const grammar::Symbols &nonTerminals, Dice &dice) {
  auto sizeWCC = rule.sizeWithoutControlChars();

  if(rule.sizeWithoutControlChars() > Config::ls_maxRuleSize())
    utils::doThrow<std::logic_error>("Starting from too long a rule!");

  R ruleBefore = rule;
  (void)ruleBefore;
#ifndef NDEBUG
#endif

  bool swappable = false;
  std::vector<float> swapWeights (rule.rhs.size()-1, 0);
  for (uint i=0; i<rule.rhs.size()-1; i++)
    swapWeights[i] = swappable
                   = !grammar::Rule_base::isBracket(rule.rhs[i])
                  && !grammar::Rule_base::isBracket(rule.rhs[i+1]);

  bool extendable = (sizeWCC < Config::ls_maxRuleSize());

  auto ruleRates = Config::ls_ruleMutationRates();
  ruleRates["dupSymb"] *= extendable;
//  ruleRates["swpSymb"] // Always true (at least should be)
  ruleRates["swpSymb"] *= swappable;
//  ruleRates["brkSymb"] // Idem
  ruleRates["delSymb"] *= (sizeWCC > 1);

  std::string field = dice.pickOne(ruleRates);

  if (debugMutations)  std::cerr << "\tMutation " << field << " on '" << rule;

  char c = '?';
  size_t i = std::string::npos;

  if (field == "dupSymb") {
    i = nonBracketSymbol(rule.rhs, dice);
    rule.rhs.replace(i, 1, std::string(2, rule.rhs[i]));

  } else if (field == "mutSymb") {
    i = nonBracketSymbol(rule.rhs, dice);
    c = R::nonBracketSymbol(rule.rhs[i], nonTerminals, dice, !extendable);
    rule.rhs[i] = c;
    assert(R::isValidNonTerminal(c) || R::isValidTerminal(c) || R::isValidControl(c));

  } else if (field == "swpSymb") {
    i = dice(rng::rdist(swapWeights.begin(), swapWeights.end()));
    std::swap(rule.rhs[i], rule.rhs[i+1]);

  } else if (field == "brkSymb") {
    i = nonBracketSymbol(rule.rhs, dice);
    rule.rhs.insert(i, "[");
    rule.rhs.insert(i+2, "]");

  } else if (field == "delSymb") {
    i = nonBracketSymbol(rule.rhs, dice);
    if (i > 0 && i+1 < rule.rhs.size() && rule.rhs[i-1] == '[' && rule.rhs[i+1] == ']')
      rule.rhs.replace(i-1, 3, "");
    else
      rule.rhs.replace(i, 1, "");

  } else
    utils::doThrow<std::logic_error>("Unhandled field '", field,
                                     "' for lsystem rule mutation");

  if(rule.sizeWithoutControlChars() > Config::ls_maxRuleSize())
    utils::doThrow<std::logic_error>("Mutation '", field,
                                     "' generated too long a rule!");

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

    // Pick a char
    // If the rule is saturated, only pick non-control symbols
    bool ruleFull = r.sizeWithoutControlChars() == Config::ls_maxRuleSize();
    size_t i = randomPosNotIn(r.rhs, dice,
                              ruleFull ? grammar::Rule_base::isValidControl
                                       : grammar::Rule_base::isBracket);


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
      d += d_ / Config::ls_maxRuleSize();

    } else {
      d += 1;
    }
  }
  return 2.f * d / maxRuleCount();
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

void printLSystemMutationRates(std::ostream &os, uint width, uint depth,
                               float ratio) {
#define F(M,X) M.at(X), X
  const auto &lsMR = Config::ls_mutationRates();
  const auto &lsrsMR = Config::ls_ruleSetMutationRates();

#define F_(X) F(lsMR,X)
  prettyPrintMutationRate(os, width, depth, ratio, F_("recursivity"), false);
  prettyPrintMutationRate(os, width, depth, ratio, F_("rules"), true);

#undef F_
#define F_(X) F(lsrsMR,X)
  ratio *= lsMR.at("rules");
  depth++;
  prettyPrintMutationRate(os, width, depth, ratio, F_("addRule"), false);
  prettyPrintMutationRate(os, width, depth, ratio, F_("delRule"), false);
  prettyPrintMutationRate(os, width, depth, ratio, F_("mutRule"), true);

#undef F_
  ratio *= lsrsMR.at("mutRule");
  depth++;
  for (const auto &p: Config::ls_ruleMutationRates())
    prettyPrintMutationRate(os, width, depth, ratio, p.second, p.first, false);
#undef F
}

template <>
struct genotype::MutationRatesPrinter<LSystem<SHOOT>, Plant, &Plant::shoot> {
  static constexpr bool recursive = true;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
    printLSystemMutationRates(os, width, depth, ratio);
  }
};
template <>
struct genotype::MutationRatesPrinter<LSystem<ROOT>, Plant, &Plant::root> {
  static constexpr bool recursive = true;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
    printLSystemMutationRates(os, width, depth, ratio);
  }
};


template <LSystemType L>
struct Extractor<LSystem<L>> {
  std::string operator() (const LSystem<L> &ls, const std::string &field) {
    if (field.size() != 1 || !LSystem<L>::Rule::isValidNonTerminal(field[0]))
      utils::doThrow<std::invalid_argument>(
        "'", field, "' is not a valid non terminal");

    auto it = ls.rules.find(field[0]);
    if (it == ls.rules.end())
      return "";
    else
      return it->second.rhs;
  }
};

template <LSystemType L>
struct Aggregator<LSystem<L>, Plant> {
  using LS = LSystem<L>;
  void operator() (std::ostream &os, const std::vector<Plant> &genomes,
                   std::function<const LS& (const Plant&)> access,
                   uint /*verbosity*/) {
    std::set<grammar::NonTerminal> nonTerminals;
    for (const Plant &p: genomes)
      for (const auto &pair: access(p).rules)
        nonTerminals.insert(pair.first);

    os << "\n";
    utils::IndentingOStreambuf indent(os);
    for (grammar::NonTerminal s: nonTerminals) {
      std::map<grammar::Successor, uint> counts;
      for (const Plant &p: genomes) {
        const auto &map = access(p).rules;
        auto it = map.find(s);
        if (it != map.end())
          counts[it->second.rhs]++;
      }

      uint i=0;
      for (const auto &p: counts) {
        if (i++ == 0)
          os << s << " -> ";
        else
          os << "     ";
        os << "(" << p.second << ") " << p.first << "\n";
      }
    }
  }
};


namespace genotype {

namespace grammar {
template <LSystemType L>
void assertEqual (const Rule_t<L> &lhs, const Rule_t<L> &rhs, bool deepcopy) {
  using utils::assertEqual;
  assertEqual(lhs.lhs, rhs.lhs, deepcopy);
  assertEqual(lhs.rhs, rhs.rhs, deepcopy);
}
} // end of namespace grammar

template <LSystemType L>
void assertEqual (const LSystem<L> &lhs, const LSystem<L> &rhs, bool deepcopy) {
  using utils::assertEqual;
  assertEqual(lhs.recursivity, rhs.recursivity, deepcopy);
  assertEqual(lhs.rules, rhs.rules, deepcopy);
}
} // end of namespace genotype

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
DEFINE_GENOME_FIELD_WITH_BOUNDS(ME, resistors, "",
                                utils::uniformStdArray<ME>(0),
                                utils::uniformStdArray<ME>(1),
                                utils::uniformStdArray<ME>(1),
                                utils::uniformStdArray<ME>(10))
DEFINE_GENOME_FIELD_WITH_BOUNDS(Metabolism::decimal, growthSpeed, "", 0., 1., 1., 10.)
DEFINE_GENOME_FIELD_WITH_BOUNDS(Metabolism::decimal, deltaWidth, "", 0., .2, .2, 1.)

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(      resistors, 2.f),
  EDNA_PAIR(    growthSpeed, 1.f),
  EDNA_PAIR(     deltaWidth, 1.f)
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(      resistors, 1.f),
  EDNA_PAIR(    growthSpeed, 1.f),
  EDNA_PAIR(     deltaWidth, 1.f)
})
#undef GENOME

/// ============================================================================
/// == Plant

#define GENOME Plant

#define LSYSTEM_FIELD_FUNCTOR(NAME)     \
  lsystemFunctor<decltype(Plant::NAME), \
                 GENOME_FIELD_FUNCTOR(decltype(Plant::NAME), NAME)>()

/// Manually managed data
#define CFILE Config
auto initRule = [] (const std::string &rhs) {
  return grammar::toSuccessor(Config::ls_axiom()) + " -> " + rhs;
};
DEFINE_PARAMETER(Config::NonTerminal, ls_axiom, 'S')
DEFINE_PARAMETER(Config::Successor, ls_shootInitRule, initRule("s[-l][+l]f"))
DEFINE_PARAMETER(Config::Successor, ls_rootInitRule, initRule("h"))
DEFINE_PARAMETER(Config::NonTerminal, ls_maxNonTerminal, 'F')
DEFINE_PARAMETER(uint, ls_maxRuleSize, 5)
DEFINE_PARAMETER(float, ls_rotationAngle, M_PI/6.)
DEFINE_PARAMETER(float, ls_nonTerminalCost, .1)

static const auto tsize = [] (float wr = 1, float lr = 1) {
  return Config::OrganSize{ .01f * wr, .1f * lr };
};
DEFINE_CONTAINER_PARAMETER(Config::OrgansSizes, ls_terminalsSizes, {
  { 's', tsize(1, .1) },
  { 'l', tsize() },
  { 'f', tsize(1, .5) },
  { 'g', tsize(2, .2) },

  { 't', tsize(1, .1) },
  { 'h', tsize() }
})
const Config::OrganSize& Config::sizeOf (char symbol) {
  static const auto &sizes = ls_terminalsSizes();
  static const auto &other = Config::OrganSize {0,0};
  auto it = sizes.find(symbol);
  if (it != sizes.end())
    return it->second;
  else
    return other;
}


DEFINE_PARAMETER(Config::Bui, ls_recursivityBounds, 1u, 2u, 2u, 5u)
DEFINE_CONTAINER_PARAMETER(Config::MutationRates, ls_mutationRates,
                           utils::normalizeRates({
  { "recursivity", 1.f }, { "rules", maxRuleCount() }
}))
DEFINE_CONTAINER_PARAMETER(Config::MutationRates, ls_ruleSetMutationRates,
                           utils::normalizeRates({
  { "addRule",  .5f },
  { "delRule",  .25f },
  { "mutRule", 9.25f },
}))
DEFINE_CONTAINER_PARAMETER(Config::MutationRates, ls_ruleMutationRates,
                           utils::normalizeRates({
  { "dupSymb", 1.5f },
  { "mutSymb", 3.5f },
  { "swpSymb", 3.f },
  { "brkSymb", 1.f },
  { "delSymb", 1.f },
}))


/// Config file hierarchy
DEFINE_SUBCONFIG(Config::Crossover, genotypeCrossoverConfig)
DEFINE_SUBCONFIG(Config::Metabolism, genotypeMetabolismConfig)
#undef CFILE


/// Auto-managed
DEFINE_GENOME_FIELD_AS_SUBGENOME(BOCData, cdata, "")

DEFINE_GENOME_FIELD_WITH_FUNCTOR(LSystem<SHOOT>, shoot, "", LSYSTEM_FIELD_FUNCTOR(shoot))
DEFINE_GENOME_FIELD_WITH_FUNCTOR(LSystem<ROOT>, root, "", LSYSTEM_FIELD_FUNCTOR(root))

DEFINE_GENOME_FIELD_AS_SUBGENOME(Metabolism, metabolism, "")
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, dethklok, "", 2u, 10u, 10u, 500u)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, fruitOvershoot, "", 1.f, 1.1f, 1.1f, 2.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, seedsPerFruit, "", 1u, 3u, 3u, 100u)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, temperatureOptimal, "", -20.f, 10.f, 10.f, 40.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, temperatureRange, "", 1e-3f, 10.f, 10.f, 30.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, structuralLength, "", 1.f, 1.f, 1.f, 10.f)


DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(              cdata, 4.f),
  EDNA_PAIR(              shoot, 8.f),
  EDNA_PAIR(               root, 8.f),
  EDNA_PAIR(         metabolism, 4.f),
  EDNA_PAIR(           dethklok, 1.f),
  EDNA_PAIR(     fruitOvershoot, .5f),
  EDNA_PAIR(      seedsPerFruit, .5f),
  EDNA_PAIR( temperatureOptimal, .5f),
  EDNA_PAIR(   temperatureRange, .5f),
  EDNA_PAIR(   structuralLength, .5f),
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(              cdata, 0.f),
  EDNA_PAIR(              shoot, 1.f),
  EDNA_PAIR(               root, 1.f),
  EDNA_PAIR(         metabolism, 1.f),
  EDNA_PAIR(           dethklok, .5f),
  EDNA_PAIR(     fruitOvershoot, .5f),
  EDNA_PAIR(      seedsPerFruit, .5f),
  EDNA_PAIR( temperatureOptimal, .5f),
  EDNA_PAIR(   temperatureRange, .5f),
  EDNA_PAIR(   structuralLength, .5f),
})

namespace config {
std::ostream& operator<< (std::ostream &os, const Config::OrganSize &ts) {
  return os << ts.width << 'x' << ts.length;
}
std::istream& operator>> (std::istream &is, Config::OrganSize &ts) {
  char c;
  is >> ts.width >> c >> ts.length;
  return is;
}
void to_json (nlohmann::json &j, const Config::OrganSize &ts) {
  j = { ts.width, ts.length };
}
void from_json (const nlohmann::json &j, Config::OrganSize &ts) {
  uint i = 0;
  ts.width = j[i++], ts.length = j[i++];
}
} // end of namespace config

#undef GENOME
