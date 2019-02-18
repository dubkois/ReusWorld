#ifndef TINIEST_PHYSICS_ENGINE_H
#define TINIEST_PHYSICS_ENGINE_H

#include "physicstypes.hpp"
#include "plant.h"

namespace simu {
namespace physics {

// =============================================================================

struct CollisionObject {
  Rect boundingRect;
  Plant const *const plant;

  Collisions englobedObjects;
  Collisions englobingObjects;

  std::unique_ptr<Edge<LEFT>> leftEdge;
  std::unique_ptr<Edge<RIGHT>> rightEdge;

  mutable UpperLayer layer;

  explicit CollisionObject (const Environment &env, const Plant *p)
    : plant(p),
      leftEdge(std::make_unique<Edge<LEFT>>(this)),
      rightEdge(std::make_unique<Edge<RIGHT>>(this)) {  updateFinal(env); }

  void updateCollisions (void) {
    boundingRect = boundingRectOf(plant);
  }

  void updateFinal (const Environment &env) {
    updateCollisions();
    layer.updateInIsolation(env, plant);
  }

  void computeCanopyInWorld (const const_Collisions &collisions) {
    layer.updateInWorld(plant, collisions);
  }

  static Rect boundingRectOf (const Plant *p) {
    return p->translatedBoundingRect();
  }

  friend std::ostream& operator<< (std::ostream &os, const CollisionObject &o) {
    return os << "{" << o.plant->id() << ": " << o.boundingRect << "}";
  }
};

class TinyPhysicsEngine {
  const Environment &environment;

  Collisions _data;

  template <EdgeSide S>
  struct PE_CMP {
    bool operator() (const Edge<S> *lhs, const Edge<S> *rhs) const {
      return *lhs < *rhs;
    }
  };

  template <EdgeSide S>
  using Edges = std::set<Edge<S>*, PE_CMP<S>>;
  Edges<LEFT> _leftEdges;
  Edges<RIGHT> _rightEdges;

  using Pistils = std::set<Pistil, std::less<>>;
  Pistils _pistils;

  using Pistils_range = std::pair<Pistils::iterator, Pistils::iterator>;

public:
  TinyPhysicsEngine (const Environment &env) : environment(env) {}

  void init (void) {}
  void reset (void);

  const auto& data (void) const {     return _data;     }
  const auto& pistils (void) const {  return _pistils;  }

  const UpperLayer::Items& canopy (const Plant *p) const;

  bool addCollisionData (const Environment &env, Plant *p);
  void removeCollisionData (Plant *p);
  bool isCollisionFree(const Plant *p) const;

  void updateCollisions (Plant *p);
  void updateFinal (const Environment &env, Plant *p);

  void addPistil (Organ *p);
  void updatePistil (Organ *p, const Point &oldPos);
  void delPistil (Organ *p);
  Pistils_range sporesInRange (Organ *s);

  // Call at the end of a simulation step to register new seeds
  void processNewObjects (void);

  // Call at the end of a simulation step to update canopies with new morphologies
  void updateCanopies (const std::set<Plant *> &plants);

private:
  Collisions::iterator find (const Plant *p);
  Collisions::const_iterator find (const Plant *p) const;

  void broadphaseCollision (const CollisionObject *object,
                            const_Collisions &objects) const;

  static bool narrowPhaseCollision (const Plant *lhs, const Plant *rhs,
                                    const Rect &intersection);

#ifndef NDEBUG
  void debug (void) const;
#endif

  bool valid (const Pistil &p);
  bool checkAll (void);
};

} // end of namespace physics
} // end of namespace simu

#endif // TINIEST_PHYSICS_ENGINE_H
