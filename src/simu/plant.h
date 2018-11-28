#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "../genotype/plant.h"
#include "types.h"

/// TODO list for plant
/// TODO assimilation:partial
///   TODO water:check
///   TODO glucose
/// TODO transport
/// TODO starvation
/// TODO biomass production (?)
/// TODO allocation
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

  float _surface;
  float _biomass;
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

  void updateCache (void);
  void updateParent (Organ *newParent);
  void updateRotation (float d_angle);

  void removeFromParent(void);

  float localRotation (void) const {  return _localRotation;  }
  const auto& globalCoordinates (void) const {  return _global; }

  const auto& boundingRect (void) const { return _boundingRect; }

  auto width (void) const {   return _width;    }
  auto length (void) const {  return _length;   }
  auto surface (void) const { return _surface;  }
  auto biomass (void) const { return _biomass;  }

  auto symbol (void) const {  return _symbol; }
  auto layer (void) const {   return _layer;  }

  auto parent (void) {  return _parent; }
  const auto parent (void) const {  return _parent; }

  auto& children (void) { return _children; }
  const auto& children (void) const { return _children; }

  bool isNonTerminal (void) const {
    return genotype::grammar::Rule_base::isValidNonTerminal(_symbol);
  }
  bool isLeaf (void) const {  return _symbol == 'l'; }
  bool isHair (void) const {  return _symbol == 'h'; }

#ifndef NDEBUG
  auto id (void) const {  return _id; }

  friend std::ostream& operator<< (std::ostream &os, const Organ &o);
#endif
};

class Plant {
  using Genome = genotype::Plant;
  using Layer = Organ::Layer;
  using Element = genotype::Element;

  Genome _genome;
  Point _pos;

  uint _age;

  using Organs = std::set<Organ*>;
  Organs _organs;

  using OrgansView = std::set<Organ*>;
  OrgansView _bases, _nonTerminals, _leaves, _hairs;

  uint _derived;  ///< Number of times derivation rules were applied

  Rect _boundingRect;

  using Masses = std::array<float, EnumUtils<Layer>::size()>;
  Masses _biomasses;

  using Reserves = std::array<genotype::Metabolism::FloatElements,
                              EnumUtils<Layer>::size()>;
  Reserves _reserves;

  bool _killed;

#ifndef NDEBUG
  uint _nextOrganID;
#endif

public:
  Plant(const Genome &g, float x, float y);
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

  auto concentration (Layer l, Element e) {
    return _reserves[l][e] / _biomasses[l];
  }

  bool isDead (void) const {
    return _killed || spontaneousDeath();
  }

  bool spontaneousDeath (void) const {
    return false;
  }

  void step (Environment &env);

  void kill (void) {
    _killed = true;
  }

  uint deriveRules(Environment &env);

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

private:
  void metabolicStep (Environment &env);
  void updateInternals (void);

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
