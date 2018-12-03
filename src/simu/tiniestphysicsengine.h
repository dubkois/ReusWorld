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

    CollisionObject (const Plant *p) : plant(p) {
      updateFinal();
    }

    void updateCollisions (void) {
      boundingRect = plant->translatedBoundingRect();
    }

    void updateFinal (void) {
      updateCollisions();
      layer.update(plant);
    }

#ifndef NDEBUG
    friend std::ostream& operator<< (std::ostream &os, const CollisionObject &o) {
      return os << "{" << o.plant->id() << ": " << o.boundingRect << "}";
    }
#endif
  };

  std::set<CollisionObject, std::less<>> _data;

  using Spores = std::set<Spore, std::less<>>;
  Spores _spores;

  using Spores_range = std::pair<Spores::iterator, Spores::iterator>;

public:
  using CObject = CollisionData::CollisionObject;

  void init (void) {}
  void reset (void);

  const auto& data (void) const {   return _data;   }
  const auto& spores (void) const { return _spores; }

  const UpperLayer::Items& canopy (const Plant *p) const;

  void addCollisionData (Plant *p);
  void removeCollisionData (Plant *p);
  bool isCollisionFree(const Plant *p) const;

  void updateCollisions (Plant *p);
  void updateFinal (Plant *p);

  void addSpore (Plant *p, Organ *f);
  void delSpore (Plant *p, Organ *f);
  void delSpore (const Spore &s);
  Spores_range sporesInRange (Plant *p, Organ *f);

  static bool narrowPhaseCollision (const Plant *lhs, const Plant *rhs);
};

using CObject = CollisionData::CObject;

inline bool operator< (const CObject &lhs, const Plant *rhs) {
  return lhs.plant < rhs;
}

inline bool operator< (const Plant *lhs, const CObject &rhs) {
  return lhs < rhs.plant;
}

bool operator< (const CObject &lhs, const CObject &rhs);

} // end of namespace physics
} // end of namespace simu

#endif // TINIEST_PHYSICS_ENGINE_H
