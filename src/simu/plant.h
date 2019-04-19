#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "organ.h"
#include "phylogenystats.hpp"

namespace simu {

struct Environment;

struct Branch {
  Rect bounds;
  Organ::Collection organs;

  Branch (Organ::Collection &bases) : bounds(Rect::invalid()) {
    for (Organ *o: bases) insert(o);
  }

  void insert (Organ *o) {
    organs.insert(o);
    bounds.uniteWith(o->globalCoordinates().boundingRect);
    for (Organ *c: o->children()) insert(c);
  }
};

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

    friend void assertEqual (const Seed &lhs, const Seed &rhs) {
      using utils::assertEqual;
      assertEqual(lhs.biomass, rhs.biomass);
      assertEqual(lhs.genome, rhs.genome);
      assertEqual(lhs.position, rhs.position);
    }
  };
  using Seeds = std::vector<Seed>;

  using Organs = Organ::Collection;

  using decimal = genotype::Metabolism::decimal;

private:
  Genome _genome;
  Point _pos;

  uint _age;

  Organs _organs;

  using OrgansView = Organs;
  OrgansView _bases, _hairs, _sinks;

  using OrgansSortedView = Organ::SortedCollection;
  OrgansSortedView _nonTerminals, _flowers;

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

    friend void assertEqual (const FruitData &lhs, const FruitData &rhs) {
      utils::assertEqual(lhs.genomes, rhs.genomes);
      utils::assertEqual(lhs.fruit->id(), rhs.fruit->id());
    }
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
  Plant(const Genome &g, const Point &pos);
  ~Plant (void);

  void init (Environment &env, float biomass, PStats *pstats);
  void destroy (void);

  void replaceWithFruit (Organ *o, const std::vector<Genome> &litter,
                         Environment &env);

  void resetStamen (Organ *s);

  bool isDirty (State s) const {  return _dirty.test(s);  }

  void updateAltitude (Environment &env, float h);
  void updateGeometry (void);
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

  auto& pistils (void) {
    assert(sex() == Sex::FEMALE);
    return _flowers;
  }

  const auto& flowers (void) const {
    return _flowers;
  }

  const auto& fruits (void) const {
    return _fruits;
  }

  bool hasUncollectedSeeds (void) const {
    return !_currentStepSeeds.empty();
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

  float heatEfficiency (float T) const;

  bool spontaneousDeath (void) const;
  void autopsy (void) const;

  /// Steps the plant by one tick
  /// \returns how many derivations were applied
  uint step(Environment &env);

  void kill (void) {
    _killed = true;
  }

  void setPStatsPointer (PStats *stats) {
    if (_pstats != stats) {
      _pstats = stats;

      if (_pstats) {
        _pstats->plant = this;
        _pstatsWC.reset(new PStatsWorkingCache);

      } else
        _pstatsWC.reset(nullptr);
    }
  }

  const auto& bases (void) const {
    return _bases;
  }

  std::string toString (Layer type) const;

  static void save (nlohmann::json &j, const Plant &p);
  static Plant* load (const nlohmann::json &j);
  friend void assertEqual (const Plant &lhs, const Plant &rhs);

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

  void assignToViews (Organ *o);

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

  Organ* makeOrgan (Organ *parent, float angle, char symbol, Layer type);
  void addOrgan (Organ *o, Environment &env);
  void addSubtree (Organ *o, Environment &env) {
    addOrgan(o, env);
    for (Organ *c: o->children()) addSubtree(c, env);
  }

  void commit (Organ *o) {
    o->unsetCloned();
    assignToViews(o);
    for (Organ *c: o->children()) commit(c);
  }

  void delOrgan (Organ *o, Environment &env);
  bool destroyDeadSubtree(Organ *o, Environment &env);

  Organ* turtleParse (Organ *parent, const std::string &successor, float &angle,
                      Layer type, Organs &newOrgans);

  Organ* turtleParse (Organ *parent, const std::string &successor, float angle,
                      Layer type) {
    Organs newOrgansDecoy;
    return turtleParse(parent, successor, angle, type, newOrgansDecoy);
  }

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
  bool noPlantID;
  OrganID (const Organ *o, bool npi = false) : o(o), noPlantID(npi) {}
  friend std::ostream& operator<< (std::ostream &os, const OrganID &oid) {
    os << "[";
    if (!oid.noPlantID) {
      PlantID::print(os, oid.o->plant());
      os << ":";
    }
    os << EnumUtils<Plant::Layer>::getName(oid.o->layer())[0] << "O";
    if (oid.o->id() == Organ::OID::INVALID)
          os << '?';
    else  os << oid.o->id();
    os << oid.o->symbol() << "]";
    return os;
  }
};

} // end of namespace simu

#endif // SIMU_PLANT_H
