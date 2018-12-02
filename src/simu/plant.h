#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "../genotype/plant.h"
#include "types.h"

/// TODO list for plant
/// TODO assimilation:check
///   TODO water:check
///   TODO glucose:check
/// TODO transport:check
/// TODO starvation
/// TODO biomass production:FIXME (only produce what's needed)
/// TODO allocation:check
/// TODO reproduction

namespace simu {

struct Environment;

class Organ {
public:
  using Layer = genotype::LSystemType;

private:
  float _localRotation;   // Parent coordinate
  Position _global;       // Plant coordinates
  float _width, _length;  // Constant for now
  char _symbol;           // To choose color and shape
  Layer _layer;           // To check altitude and symbol

  Organ *_parent;
  std::set<Organ*> _children;
  uint _depth;

  float _surface;
  float _baseBiomass, _accumulatedBiomass, _requiredBiomass;
  Rect _boundingRect;

#ifndef NDEBUG
  uint _id;
#endif

public:
#ifndef NDEBUG
#define MAYBE_ID uint id,
#else
#define MAYBE_ID
#endif
  Organ (MAYBE_ID float w, float l, float r, char c, Layer t, Organ *p = nullptr);

  /// surface, biomass
  void accumulate (float biomass);
  void updateDimensions(bool andTransformations);

  /// _boundingRect
  void updateTransformation (void);
  void updateParent (Organ *newParent);
  void updateRotation (float d_angle);

  void removeFromParent(void);

  float localRotation (void) const {  return _localRotation;  }
  const auto& globalCoordinates (void) const {  return _global; }

  const auto& boundingRect (void) const { return _boundingRect; }

  auto width (void) const {   return _width;    }
  auto length (void) const {  return _length;   }
  auto surface (void) const { return _surface;  }
  auto biomass (void) const {
    return _baseBiomass + _accumulatedBiomass;
  }

  void setRequiredBiomass (float rb) {
    _requiredBiomass = rb;
  }
  auto requiredBiomass (void) const {
    return _requiredBiomass;
  }

  auto symbol (void) const {  return _symbol; }
  auto layer (void) const {   return _layer;  }

  auto parent (void) {  return _parent; }
  const auto parent (void) const {  return _parent; }

  auto& children (void) { return _children; }
  const auto& children (void) const { return _children; }

  auto depth (void) const { return _depth;  }

  bool isNonTerminal (void) const {
    return genotype::grammar::Rule_base::isValidNonTerminal(_symbol);
  }
  bool isLeaf (void) const {    return _symbol == 'l';  }
  bool isHair (void) const {    return _symbol == 'h';  }
  bool isFlower (void) const {  return _symbol == 'f';  }
  bool isStructural (void) const {
    return _symbol == 's' || _symbol == 't';
  }

#ifndef NDEBUG
  auto id (void) const {  return _id; }

  friend std::ostream& operator<< (std::ostream &os, const Organ &o);
#endif
};

class Plant {
  using Genome = genotype::Plant;
  using Layer = Organ::Layer;
  using Element = genotype::Element;

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

  using Fruits = std::map<Organ*, Genome>;
  Fruits _fruits;

  bool _killed;

#ifndef NDEBUG
  uint _nextOrganID;
#endif

public:
  Plant(const Genome &g, float x, float y, float biomass);
  ~Plant (void);

  auto id (void) const {
    return _genome.cdata.id;
  }

  const Point& pos (void) const {
    return _pos;
  }

  const auto& organs (void) const {
    return _organs;
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
    return _reserves[l][e] / _biomasses[l];
  }

  bool isSeed (void) const {
    if (_organs.size() == 2) {
      for (Organ *o: _organs)
        if (o->symbol() != config::PlantGenome::ls_axiom())
          return false;
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

  void step (Environment &env);

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

  static float ruleBiomassCost(const Genome &g, Layer l, char symbol);
  float ruleBiomassCost (Layer l, char symbol) const {
    return ruleBiomassCost(_genome, l, symbol);
  }

  static float initialBiomassFor (const Genome &g);

  void attemptReproduction (Environment &env);

  Organ *addOrgan (Organ *parent, float angle, char symbol, Layer type);

  void delOrgan (Organ *o);

  Organ* turtleParse (Organ *parent, const std::string &successor, float &angle,
                      Layer type, Organs &newOrgans,
                      const genotype::grammar::Checkers &checkers);

  void updateGeometry (void);

  void updateSubtree(Organ *oldParent, Organ *newParent, float angle_delta);
};

} // end of namespace simu

#endif // SIMU_PLANT_H
