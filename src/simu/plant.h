#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "organ.h"
#include "phylogenystats.hpp"

namespace simu {

struct Environment;

class Plant {
public:
  using Genome = genotype::Plant;
  using Sex = genotype::BOCData::Sex;
  using ID = genotype::BOCData::GID;

  using Layer = Organ::Layer;
  using Element = genotype::Element;

  struct Seed {
    float biomass;
    Genome genome;
    Point position;
  };
  using Seeds = std::vector<Seed>;

  using decimal = genotype::Metabolism::decimal;

private:
  Genome _genome;
  Point _pos;

  uint _age;

  using Organs = std::set<Organ*>;
  Organs _organs;

  using OrgansView = std::set<Organ*>;
  OrgansView _bases, _nonTerminals, _hairs, _sinks, _flowers;

  uint _derived;  ///< Number of times derivation rules were applied

  Rect _boundingRect;

  using Masses = std::array<decimal, EnumUtils<Layer>::size()>;
  Masses _biomasses;

  using Reserves = std::array<genotype::Metabolism::Elements,
                              EnumUtils<Layer>::size()>;
  Reserves _reserves;

  using OID = Organ::OID;
  struct FruitData {
    std::vector<Genome> genomes;
    Organ *fruit;
  };
  using Fruits = std::map<OID, FruitData>;
  Fruits _fruits;

  Seeds _currentStepSeeds;

  OID _nextOrganID;

  bool _killed;

  enum State {
    DIRTY_METABOLISM, DIRTY_COLLISION
  };
  std::bitset<2> _dirty;

  PStats *_pstats;
  std::unique_ptr<PStatsWorkingCache> _pstatsWC;

public:
  Plant(const Genome &g, float x, float y);
  ~Plant (void);

  void init (Environment &env, float biomass, PStats *pstats);
  void destroy (void);

  void replaceWithFruit (Organ *o, const std::vector<Genome> &litter,
                         Environment &env);

  void update (Environment &env);

  auto id (void) const {
    return _genome.cdata.id;
  }

  auto sex (void) const {
    return _genome.cdata.sex;
  }

  const Point& pos (void) const {
    return _pos;
  }

  const auto& organs (void) const {
    return _organs;
  }

  auto& stamens (void) {
    assert(sex() == Sex::MALE);
    return _flowers;
  }

  const auto& flowers (void) const {
    return _flowers;
  }

  const auto& fruits (void) const {
    return _fruits;
  }

  void collectCurrentStepSeeds (Seeds &seeds) {
    seeds.insert(seeds.end(), _currentStepSeeds.begin(), _currentStepSeeds.end());
    _currentStepSeeds.clear();
  }

  float age (void) const;

  const auto& genome (void) const {
    return _genome;
  }

  const auto& boundingRect (void) const {
    return _boundingRect;
  }

  auto translatedBoundingRect (void) const {
    return _boundingRect.translated(_pos);
  }

  auto biomass (void) const {
    return _biomasses[Layer::SHOOT] + _biomasses[Layer::ROOT];
  }

  auto concentration (Layer l, Element e) const {
    if (auto b = _biomasses[l])
      return _reserves[l][e] / b;
    else
      return decimal(0);
  }

  bool isInSeedState (void) const {
    if (_organs.size() == 2) {
      for (Organ *o: _organs) if (!o->isSeed()) return false;
      return true;
    }
    return false;
  }

  bool isDead (void) const {
    return _killed || spontaneousDeath();
  }

  bool spontaneousDeath (void) const;
  void autopsy (void) const;

  void step (Environment &env);

  void kill (void) {
    _killed = true;
  }

  const auto& bases (void) const {
    return _bases;
  }

  void removedFromEnveloppe (void);

  std::string toString (Layer type) const;

  friend std::ostream& operator<< (std::ostream &os, const Plant &p);

  static float initialAngle (Layer t) {
    switch (t) {
    case Layer::SHOOT: return  M_PI/2.;
    case Layer::ROOT:  return -M_PI/2.;
    }
    utils::doThrow<std::logic_error>("Invalid lsystem type", t);
    return 0;
  }

  static float primordialPlantBaseBiomass (const Genome &g) {
    return seedBiomassRequirements(g, g);
  }

private:  
  uint deriveRules(Environment &env);

  void metabolicStep (Environment &env);
  void updateMetabolicValues (void);

  bool isSink (Organ *o) const;
  float sinkRequirement (Organ *o) const;  // How much more biomass it needs in [0,1]
  void biomassRequirements (Masses &wastes, Masses &growth);
  void updateRequirements (void);

  template <typename F>
  void distributeBiomass (decimal amount, Organs &organs, F match);

  template <typename F>
  void distributeBiomass (decimal amount, Organs &organs, decimal total, F match);

  static float ruleBiomassCost(const Genome &g, Layer l, char symbol);
  float ruleBiomassCost (Layer l, char symbol) const {
    return ruleBiomassCost(_genome, l, symbol);
  }
  static float seedBiomassRequirements(const Genome &mother, const Genome &child);

  static float initialBiomassFor (const Genome &g);

  Organ *addOrgan (Organ *parent, float angle, char symbol, Layer type,
                   Environment &env);

  void delOrgan (Organ *o, Environment &env);
  bool destroyDeadSubtree(Organ *o, Environment &env);

  Organ* turtleParse (Organ *parent, const std::string &successor, float &angle,
                      Layer type, Organs &newOrgans,
                      const genotype::grammar::Checkers &checkers,
                      Environment &env);

  Organ* turtleParse (Organ *parent, const std::string &successor, float angle,
                      Layer type, const genotype::grammar::Checkers &checkers,
                      Environment &env) {
    Organs newOrgans;
    return turtleParse(parent, successor, angle, type, newOrgans, checkers, env);
  }

  void updateGeometry (void);

  void updateSubtree(Organ *oldParent, Organ *newParent, float angle_delta);

  void updatePStats (Environment &env);

  void collectSeedsFrom(Organ *fruit);
  void processFruits (Environment &env);
};


struct PlantID {
  const Plant *p;
  PlantID (const Plant *p) : p(p) {}
  static void print(std::ostream &os, const Plant *p) {
    os << "P" << p->id();
  }
  friend std::ostream& operator<< (std::ostream &os, const PlantID &pid) {
    os << "[";
    print(os, pid.p);
    return os << "]";
  }
};
struct OrganID {
  const Organ *o;
  OrganID (const Organ *o) : o(o) {}
  friend std::ostream& operator<< (std::ostream &os, const OrganID &oid) {
    os << "[";
    PlantID::print(os, oid.o->plant());
    return os << ":" << EnumUtils<Plant::Layer>::getName(oid.o->layer())[0]
              << "O" << oid.o->id() << oid.o->symbol() << "]";
  }
};

} // end of namespace simu

#endif // SIMU_PLANT_H
