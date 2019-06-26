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

  static CollisionObject* clone (const CollisionObject *that,
                                 const Plant *clonedPlant,
                                 const std::map<const Organ*,
                                                Organ*> &olookup);

  static Rect boundingRectOf (const Plant *p) {
    return p->translatedBoundingRect();
  }

  friend std::ostream& operator<< (std::ostream &os, const CollisionObject &o) {
    return os << "{" << o.plant->id() << ": " << o.boundingRect << "}";
  }

private:
  CollisionObject (const Plant *p) : plant(p) {}
};

class TinyPhysicsEngine {
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
  void init (void) {}
  void reset (void);

  const auto& data (void) const {     return _data;     }
  const auto& pistils (void) const {  return _pistils;  }

  const UpperLayer::Items& canopy (const Plant *p) const;

  bool addCollisionData (const Environment &env, Plant *p);
  void removeCollisionData (Plant *p);

  CollisionResult collisionTest(const Plant *plant,
                                const Branch &self, const Branch &branch,
                                const Organ::Collection &newOrgans);

  void updateCollisions (Plant *p);
  void updateFinal (const Environment &env, Plant *p);

  void addPistil (Organ *p);
  void updatePistil (Organ *p, const Point &oldPos);
  void delPistil (const Organ *p);
  Pistils_range sporesInRange (Organ *s);

  // Call at the end of a simulation step to register new seeds
  void processNewObjects (void);

  // Call after loading a save file to ensure everything is fine
  void postLoad (void);

#ifndef NDEBUG
  void debug (void) const;
#endif

  void clone (const TinyPhysicsEngine &e,
              const std::map<const Plant*, Plant*> &plookup,
              const std::map<const Plant*,
                             std::map<const Organ *, Organ*>> &olookups);

  friend void assertEqual (const TinyPhysicsEngine &lhs,
                           const TinyPhysicsEngine &rhs, bool deepcopy);

#ifdef DEBUG_COLLISIONS
  struct BranchCollisionObject {
    CollisionResult res;
    Point pos;
    float rot;
    char symb;
    float w,l;
  };
  std::map<const Plant*, std::vector<BranchCollisionObject>> collisionsDebugData;
#endif

private:
  Collisions::iterator find (const Plant *p);
  Collisions::const_iterator find (const Plant *p) const;

  template <EdgeSide S>
  void updateEdge (Edge<S> *edge, float val);

  template <EdgeSide S>
  Edges<S>& getEdges (void);

  void broadphaseCollision (const CollisionObject *object,
                            const_Collisions &objects,
                            const Rect otherBounds = Rect::invalid());

  bool valid (const Pistil &p);
  bool checkAll (void);
};

} // end of namespace physics
} // end of namespace simu

#endif // TINIEST_PHYSICS_ENGINE_H
