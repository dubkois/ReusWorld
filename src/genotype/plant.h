#ifndef GNTP_PLANT_H
#define GNTP_PLANT_H

#include <string>

#include "grammar.h"
#include "config.h"

namespace genotype {

template <LSystemType T>
struct LSystem {
  static constexpr LSystemType type = T;
  using NonTerminal = grammar::NonTerminal;
  using Successor = grammar::Successor;
  using Rule = grammar::Rule_t<T>;
  using Rules = std::map<NonTerminal, Rule>;

  uint recursivity;

  Rules rules;

  const Successor& successor (NonTerminal symbol) const {
    static const std::string noSuccessor = "";
    auto it = rules.find(symbol);
    if (it == rules.end())
      return noSuccessor;
    return it->second.rhs;
  }

  std::string stringPhenotype (const std::string &axiom) const {
    std::string phenotype;
    for (char c: axiom) {
      if (!Rule::isValidNonTerminal(c))
        phenotype += c;
      else
        phenotype += successor(c);
    }
    return phenotype;
  }

  friend std::ostream& operator<< (std::ostream &os, const LSystem &ls) {
    os << "[\n\trec: " << ls.recursivity << "\n";
    for (const auto &p: ls.rules) os << "\t" << p.second << "\n";
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

class Metabolism : public EDNA<Metabolism> {
  APT_EDNA()
public:
  using decimal = config_t::decimal;
  using Elements = config_t::Elements;
  Elements resistors;  // rho_c, rho_w
  decimal growthSpeed;  // k_g
  decimal deltaWidth;
};
DECLARE_GENOME_FIELD(Metabolism, Metabolism::Elements, resistors)
DECLARE_GENOME_FIELD(Metabolism, Metabolism::decimal, growthSpeed)
DECLARE_GENOME_FIELD(Metabolism, Metabolism::decimal, deltaWidth)


class Plant : public EDNA<Plant> {
  APT_EDNA()

public:
  BOCData cdata;
  phylogeny::Genealogy gdata;

  LSystem<SHOOT> shoot;
  LSystem<ROOT> root;

  Metabolism metabolism;
  uint dethklok;

  float fruitOvershoot; ///< Ratio of minimal resources per fruit
  uint seedsPerFruit;

  float temperatureOptimal;
  float temperatureRange;

  float structuralLength;

  auto id (void) const {  return gdata.self.gid;  }
  auto species (void) const { return gdata.self.sid;  }

  phylogeny::Genealogy& genealogy (void) { return gdata; }
  const phylogeny::Genealogy& genealogy (void) const {  return gdata; }

  Plant clone (phylogeny::GIDManager &m) const {
    Plant other = *this;
    other.gdata.updateAfterCloning(m);
    return other;
  }

  BOCData& crossoverData (void) {
    return cdata;
  }

  const BOCData& crossoverData (void) const {
    return cdata;
  }

  const grammar::Successor&
  successor (LSystemType t, grammar::NonTerminal nt) const {
    switch (t) {
    case SHOOT:  return shoot.successor(nt);
    case ROOT:   return root.successor(nt);
    }
    utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    __builtin_unreachable();
  }

  auto maxDerivations (LSystemType t) {
    switch (t) {
    case SHOOT:  return shoot.recursivity;
    case ROOT:  return root.recursivity;
    }
    utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    __builtin_unreachable();
  }

  auto sizeOf (grammar::Symbol s) const {
    auto size = config_t::sizeOf(s);
    if (grammar::Rule_base::isStructural(s))
      size.length *= structuralLength;
    return size;
  }

  auto compatibility (float dist) const {
    return cdata(dist);
  }

  static grammar::Checkers checkers (LSystemType t) {
    switch(t) {
    case SHOOT:  return decltype(shoot)::Rule::checkers();
    case ROOT:   return decltype(root)::Rule::checkers();
    }
    utils::doThrow<std::invalid_argument>("Invalid lsystem type ", t);
    __builtin_unreachable();
  }

  std::string extension (void) const override {
    return ".plant.json";
  }

  void to_jsonExtension (json &j) const override {
    j["gen"] = gdata;
  }

  void from_jsonExtension (json &j) override {
    auto it = j.find("gen");
    gdata = *it;
    j.erase(it);
  }

  void to_streamExtension (std::ostream &os) const override {
    os << gdata << "\n";
  }

  void equalExtension (const Plant &that, bool &ok) const override {
    ok &= (this->gdata == that.gdata);
  }

//  Plant();
};

DECLARE_GENOME_FIELD(Plant, BOCData, cdata)
DECLARE_GENOME_FIELD(Plant, LSystem<SHOOT>, shoot)
DECLARE_GENOME_FIELD(Plant, LSystem<ROOT>, root)
DECLARE_GENOME_FIELD(Plant, Metabolism, metabolism)
DECLARE_GENOME_FIELD(Plant, uint, dethklok)
DECLARE_GENOME_FIELD(Plant, float, fruitOvershoot)
DECLARE_GENOME_FIELD(Plant, uint, seedsPerFruit)
DECLARE_GENOME_FIELD(Plant, float, temperatureOptimal)
DECLARE_GENOME_FIELD(Plant, float, temperatureRange)
DECLARE_GENOME_FIELD(Plant, float, structuralLength)


} // end of namespace genotype

#endif // GNTP_PLANT_H
