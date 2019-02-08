#ifndef TINIEST_PHYSICS_ENGINE_H
#define TINIEST_PHYSICS_ENGINE_H

#include "physicstypes.hpp"
#include "plant.h"

namespace simu {
namespace physics {

// =============================================================================

class TinyPhysicsEngine {
  struct CollisionObject {
    Rect boundingRect;
    const Plant *plant;

    UpperLayer layer;

    explicit CollisionObject (const Environment &env, const Plant *p)
      : plant(p) {  updateFinal(env);  }

    void updateCollisions (void) {
      boundingRect = boundingRectOf(plant);
    }

    void updateFinal (const Environment &env) {
      updateCollisions();
      layer.update(env, plant);
    }

    static Rect boundingRectOf (const Plant *p) {
      return p->translatedBoundingRect();
    }

    friend std::ostream& operator<< (std::ostream &os, const CollisionObject &o) {
      return os << "{" << o.plant->id() << ": " << o.boundingRect << "}";
    }
  };

  const Environment &environment;

  using Collisions = std::set<CollisionObject, std::less<>>;
  Collisions _data;

  using Pistils = std::set<Pistil, std::less<>>;
  Pistils _pistils;

  using Pistils_range = std::pair<Pistils::iterator, Pistils::iterator>;

public:
  using CObject = TinyPhysicsEngine::CollisionObject;

  TinyPhysicsEngine (const Environment &env) : environment(env) {}

  void init (void) {}
  void reset (void);

#ifndef NDEBUG
  void debug (void) const;
#endif

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

  static bool narrowPhaseCollision (const Plant *lhs, const Plant *rhs);

private:
  Collisions::iterator find (const Plant *p);
  Collisions::const_iterator find (const Plant *p) const;

  bool valid (const Pistil &p);
  bool checkAll (void);
};

using CObject = TinyPhysicsEngine::CObject;

} // end of namespace physics
} // end of namespace simu

#endif // TINIEST_PHYSICS_ENGINE_H
