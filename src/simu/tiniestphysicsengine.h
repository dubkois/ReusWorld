#ifndef TINIEST_PHYSICS_ENGINE_H
#define TINIEST_PHYSICS_ENGINE_H

#include "plant.h"

namespace simu {
namespace physics {
// =============================================================================

struct UpperLayer {
  struct Item {
    float l, r, y;
    Organ *organ;

    Item (void);
  };

  std::vector<Item> items;

  struct Sweeper;
  void update (const Plant *p);
};

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

public:
  using CObject = CollisionData::CollisionObject;

  void init (void) {}
  void reset (void);

  const auto& data (void) const { return _data; }

  void addCollisionData (Plant *p);
  void removeCollisionData (Plant *p);
  bool isCollisionFree(const Plant *p) const;

  void updateCollisions (Plant *p);
  void updateFinal (Plant *p);

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
