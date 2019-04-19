#ifndef ORGAN_H
#define ORGAN_H

#include "../genotype/plant.h"

#include "types.h"

namespace simu {

struct Plant;

class Organ {
public:
  using Layer = genotype::LSystemType;
  enum OID : uint { INVALID = uint(-1) };

  using Corners = std::array<Point, 4>;

  struct ParentCoordinates {
   float rotation;
  };

  struct PlantCoordinates {
    Point origin, end;
    Point center;
    float rotation;
    Rect boundingRect;
    Corners corners;
  };

  struct GlobalCoordinates {
    Point origin;
    Point center;
    Rect boundingRect;
    Corners corners;
  };

  using Collection = std::set<Organ*>;
  using const_Collection = std::set<Organ const *>;

  struct OID_CMP {
    using is_transparent = void;
    bool operator() (const OID &lhs, const OID &rhs) const noexcept {
      return lhs < rhs;
    }
    bool operator() (const OID &lhs, const Organ *rhs) const noexcept {
      return operator() (lhs, rhs->_id);
    }
    bool operator() (const Organ *lhs, const OID &rhs) const noexcept {
      return operator() (lhs->_id, rhs);
    }
    bool operator() (const Organ *lhs, const Organ *rhs) const noexcept {
      return operator() (lhs->_id, rhs->_id);
    }
  };
  using SortedCollection = std::set<Organ*, OID_CMP>;

private:
  OID _id;
  Plant *const _plant;

  ParentCoordinates _parentCoordinates;
  PlantCoordinates _plantCoordinates;
  GlobalCoordinates _globalCoordinates;

  float _width;
  float _length;  // Constant for now
  char _symbol;   // To choose color and shape
  Layer _layer;   // To check altitude and symbol

  bool _cloned; ///< Whether there exists another organ with the same id

  Organ *_parent;
  Collection _children;
  uint _depth;  ///< Height of the subtree rooted here

  float _surface;
  float _baseBiomass, _accumulatedBiomass, _requiredBiomass;

public:
  Organ (Plant *plant, float w, float l, float r, char c, Layer t,
         Organ *parent);

  void setID (OID id) {
    _id = id;
  }

  /// surface, biomass
  void accumulate (float biomass);
  void updateDimensions(bool andTransformations);

  /// _boundingRect
  void updateTransformation (void);
  void updateBoundingBox (void);
  void updateGlobalTransformation (void);
  void updateParent (Organ *newParent);
  void updateRotation (float d_angle);

  void removeFromParent(void);

  Organ* cloneAndUpdate (Organ *newParent, float rotation);

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
  auto accumulatedBiomass (void) const {
    return _accumulatedBiomass;
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

  bool isCloned (void) const {  return _cloned; }
  void unsetCloned (void) { _cloned = false;  }

  bool isUncommitted (void) const {
    return isCloned() || id() == OID::INVALID;
  }

  void updateDepth (uint newDepth);
  void updateParentDepth (void);
  auto depth (void) const {
    return _depth;
  }

  bool isDead (void) const {
    return biomass() < 0;
  }

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

  friend std::ostream& operator<< (std::ostream &os, const Organ &o);

  static void save (nlohmann::json &j, const Organ &o);
  static Organ* load (const nlohmann::json &j, Organ *parent, Plant *plant,
                      Collection &organs);

  friend void assertEqual (const Organ &lhs, const Organ &rhs);
};

} // end of namespace simu

#endif // ORGAN_H
