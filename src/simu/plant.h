#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "../genotype/plant.h"
#include "types.h"

/// TODO list for plant
/// TODO assimilation:check
///   TODO water:check
///   TODO glucose:check
/// TODO transport:check
/// TODO starvation:
///   FIXME How do you die?
/// TODO biomass production:
///   FIXME Numerical traps (bandaged for now)
/// TODO allocation:
///   FIXME Repartition between root and shoot apexes
/// TODO reproduction

namespace simu {

struct Plant;
struct Environment;

class Organ {
public:
  using Layer = genotype::LSystemType;
  enum OID : uint { INVALID = uint(-1) };

  struct ParentCoordinates {
   float rotation;
  };

  struct PlantCoordinates {
    Point origin, end;
    Point center;
    float rotation;
    Rect boundingRect;
  };

  struct GlobalCoordinates {
    Point origin;
    Point center;
    Rect boundingRect;
  };

private:
  OID _id;
  Plant *const _plant;

  ParentCoordinates _parentCoordinates;
  PlantCoordinates _plantCoordinates;
  GlobalCoordinates _globalCoordinates;

  float _width, _length;  // Constant for now
  char _symbol;           // To choose color and shape
  Layer _layer;           // To check altitude and symbol

  Organ *_parent;
  std::set<Organ*> _children;
  uint _depth;

  float _surface;
  float _baseBiomass, _accumulatedBiomass, _requiredBiomass;

public:
  Organ (OID id, Plant *plant, float w, float l, float r, char c, Layer t,
         Organ *p = nullptr);

  /// surface, biomass
  void accumulate (float biomass);
  void updateDimensions(bool andTransformations);

  /// _boundingRect
  void updateTransformation (void);
  void updateParent (Organ *newParent);
  void updateRotation (float d_angle);

  void removeFromParent(void);

  auto id (void) const {    return _id;     }
  auto plant (void) const { return _plant;  }

  float localRotation (void) const {
    return _parentCoordinates.rotation;
  }
  const auto& inPlantCoordinates (void) const {  return _plantCoordinates; }
  const auto& globalCoordinates (void) const { return _globalCoordinates; }

  auto width (void) const {   return _width;    }
  auto length (void) const {  return _length;   }
  auto surface (void) const { return _surface;  }

  auto baseBiomass (void) const {
    return _baseBiomass;
  }
  auto biomass (void) const {
    return _baseBiomass + _accumulatedBiomass;
  }

  void setRequiredBiomass (float rb) {
    _requiredBiomass = rb;
  }
  auto requiredBiomass (void) const {
    return std::max(0.f, _requiredBiomass);
  }
  float fullness (void) const;  ///< Returns ratio of accumulated biomass [0,1]

  auto symbol (void) const {  return _symbol; }
  auto layer (void) const {   return _layer;  }

  auto parent (void) {  return _parent; }
  const auto parent (void) const {  return _parent; }

  auto& children (void) { return _children; }
  const auto& children (void) const { return _children; }

  auto depth (void) const { return _depth;  }

  bool isSeed (void) const {
    return _symbol == config::PlantGenome::ls_axiom();
  }
  bool isNonTerminal (void) const {
    return genotype::grammar::Rule_base::isValidNonTerminal(_symbol);
  }
  bool isLeaf (void) const {    return _symbol == 'l';  }
  bool isHair (void) const {    return _symbol == 'h';  }
  bool isFlower (void) const {  return _symbol == 'f';  }
  bool isStructural (void) const {
    return _symbol == 's' || _symbol == 't';
  }

  bool isFruit (void) const {
    return _symbol == genotype::grammar::Rule_base::fruitSymbol();
  }

#ifndef NDEBUG
  friend std::ostream& operator<< (std::ostream &os, const Organ &o);
#endif
};

class Plant {
public:
  using Genome = genotype::Plant;
  using Sex = genotype::BOCData::Sex;

  using Layer = Organ::Layer;
  using Element = genotype::Element;

  struct Seed {
    float biomass;
    Genome genome;
    Point position;
  };
  using Seeds = std::vector<Seed>;

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

  using decimal = genotype::Metabolism::decimal;
  using Masses = std::array<decimal, EnumUtils<Layer>::size()>;
  Masses _biomasses;

  using Reserves = std::array<genotype::Metabolism::Elements,
                              EnumUtils<Layer>::size()>;
  Reserves _reserves;

  using OID = Organ::OID;
  struct FruitData {
    Genome genome;
    Organ *fruit;
  };
  using Fruits = std::map<OID, FruitData>;
  Fruits _fruits;

  OID _nextOrganID;

  bool _killed;

public:
  Plant(const Genome &g, float x, float y);
  ~Plant (void);

  void init (Environment &env, float biomass);

  void replaceWithFruit (Organ *o, const Genome &g, Environment &env);

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

  bool spontaneousDeath (void) const {
    if (_genome.dethklok <= age())  return true;
    for (Element e: EnumUtils<Element>::iterator())
      if (_reserves[Layer::SHOOT][e] + _reserves[Layer::ROOT][e] <= 0)
          return true;
    return false;
  }

  void step (Environment &env, Seeds &seeds);

  void kill (void) {
    _killed = true;
  }

  const auto& bases (void) const {
    return _bases;
  }

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
    return initialBiomassFor(g);
  }

private:  
  uint deriveRules(Environment &env);

  void metabolicStep (Environment &env);
  void updateInternals (void);

  bool isSink (Organ *o) const;
  float sinkRequirement (Organ *o) const;  // How much more biomass it needs in [0,1]
  void biomassRequirements (Masses &wastes, Masses &growth);
  static float seedBiomassRequirements(const Genome &mother, const Genome &child);

  static float ruleBiomassCost(const Genome &g, Layer l, char symbol);
  float ruleBiomassCost (Layer l, char symbol) const {
    return ruleBiomassCost(_genome, l, symbol);
  }

  static float initialBiomassFor (const Genome &g);

  Organ *addOrgan (Organ *parent, float angle, char symbol, Layer type,
                   Environment &env);

  void delOrgan (Organ *o, Environment &env);

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

  void collectFruits (Seeds &seeds, Environment &env);
};

#ifndef NDEBUG

struct PlantID {
  Plant *p;
  PlantID (Plant *p) : p(p) {}
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
    return os << ":O" << oid.o->id() << oid.o->symbol() << "]";
  }
};
#endif

} // end of namespace simu

#endif // SIMU_PLANT_H
