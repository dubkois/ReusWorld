#ifndef PHYSICS_TYPES_HPP
#define PHYSICS_TYPES_HPP

#include <vector>
#include <set>

#include "types.h"

namespace genotype {
struct Plant;
} // end of namespace genotype

namespace simu {
struct Environment;
struct Plant;
struct Organ;

namespace physics {

struct CollisionObject;

enum EdgeSide { LEFT, RIGHT };

template <EdgeSide S>
struct Edge {
  CollisionObject const * const object;
  float edge;

  Edge (const CollisionObject *object) : object(object), edge(NAN) {}

  friend bool operator< (const Edge &lhs, const Edge &rhs) {
    if (lhs.edge != rhs.edge) return lhs.edge < rhs.edge;
    return lhs.object < rhs.object;
  }
};

namespace _details {
struct CO_CMP {
  // Delegate comparison of collision objects to their managed plant
  using is_transparent = void;

  bool operator() (const Plant &lhs, const Plant &rhs) const;
  bool operator() (const CollisionObject *lhs, const Plant &rhs) const;
  bool operator() (const Plant &lhs, const CollisionObject *rhs) const;
  bool operator() (const CollisionObject *lhs, const CollisionObject *rhs) const;
};
} // end of namespace _details
using Collisions = std::set<CollisionObject*, _details::CO_CMP>;
using const_Collisions = std::set<const CollisionObject*, _details::CO_CMP>;

struct UpperLayer {
  struct Item {
    float l, r, y;
    Organ *organ;

    Item (void);
  };

  using Items = std::vector<Item>;
  Items itemsInIsolation, itemsInWorld;

  void updateInIsolation (const Environment &env, const Plant *p);
  void updateInWorld (const Plant *p, const const_Collisions &collisions);
};


struct Pistil {
  Organ *const organ;

  Disk boundingDisk;

  Pistil (void) : Pistil(nullptr, Disk()) {}

  Pistil (Organ *const f, const Disk &d) : organ(f), boundingDisk(d) {}

  bool isValid (void) const {
    return organ && boundingDisk.radius > 0;
  }
};

} // end of namespace physics
} // end of namespace simu

#endif // PHYSICS_TYPES_HPP
