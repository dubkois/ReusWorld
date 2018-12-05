#ifndef TINIEST_PHYSICS_ENGINE_H
#define TINIEST_PHYSICS_ENGINE_H

#include "physicstypes.hpp"
#include "plant.h"

namespace simu {
namespace physics {

// =============================================================================

class CollisionData {
  struct CollisionObject {
    Rect boundingRect;
    const Plant *plant;

    UpperLayer layer;

    explicit CollisionObject (const Plant *p) : plant(p) {
      updateFinal();
    }

    void updateCollisions (void) {
      boundingRect = boundingRectOf(plant);
    }

    void updateFinal (void) {
      updateCollisions();
      layer.update(plant);
    }

    static Rect boundingRectOf (const Plant *p) {
      return p->translatedBoundingRect();
    }

#ifndef NDEBUG
    friend std::ostream& operator<< (std::ostream &os, const CollisionObject &o) {
      return os << "{" << o.plant->id() << ": " << o.boundingRect << "}";
    }
#endif
  };

  using Collisions = std::set<CollisionObject, std::less<>>;
  Collisions _data;

  using Pistils = std::set<Pistil, std::less<>>;
  Pistils _pistils;

  using Pistils_range = std::pair<Pistils::iterator, Pistils::iterator>;

public:
  using CObject = CollisionData::CollisionObject;

  void init (void) {}
  void reset (void);

  const auto& data (void) const {     return _data;     }
  const auto& pistils (void) const {  return _pistils;  }

  const UpperLayer::Items& canopy (const Plant *p) const;

  void addCollisionData (Plant *p);
  void removeCollisionData (Plant *p);
  bool isCollisionFree(const Plant *p) const;

  void updateCollisions (Plant *p);
  void updateFinal (Plant *p);

  void addPistil (Organ *p);
  void updatePistil (Organ *p, const Point &oldPos);
  void delPistil (const Pistil &s);
  void delPistil (const Point &pos);
  Pistils_range sporesInRange (Organ *s);

  static bool narrowPhaseCollision (const Plant *lhs, const Plant *rhs);
};

using CObject = CollisionData::CObject;

} // end of namespace physics
} // end of namespace simu

#endif // TINIEST_PHYSICS_ENGINE_H
