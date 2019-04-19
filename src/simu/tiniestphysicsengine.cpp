#include <stack>

#include "kgd/utils/indentingostream.h"

#include "tiniestphysicsengine.h"
#include "environment.h"

/// FIXME Pistils yet again bugging out

namespace simu {
namespace physics {

enum EventType { IN = 1, OUT = 2 };

static constexpr bool debugBroadphase = false;
static constexpr bool debugNarrowphase = true;
static constexpr bool debugCollision = false || debugBroadphase || debugNarrowphase;
static constexpr bool debugUpperLayer = false;
static constexpr bool debugReproduction = false;

static constexpr float floatPrecision = 1e6;
bool fuzzyEqual (float lhs, float rhs) {
  return int(lhs * floatPrecision) == int(rhs * floatPrecision);
}
bool fuzzyLower (float lhs, float rhs) {
  return int(lhs * floatPrecision) < int(rhs * floatPrecision);
}

bool _details::CO_CMP::operator ()(const Plant &lhs, const Plant &rhs) const {
  return fuzzyLower(lhs.pos().x, rhs.pos().x);
}

bool _details::CO_CMP::operator() (const CollisionObject *lhs, const Plant &rhs) const {
  return this->operator ()(*lhs->plant, rhs);
}
bool _details::CO_CMP::operator() (const Plant &lhs, const CollisionObject *rhs) const {
  return this->operator ()(lhs, *rhs->plant);
}
bool _details::CO_CMP::operator() (const CollisionObject *lhs, const CollisionObject *rhs) const {
  return this->operator ()(*lhs->plant, *rhs->plant);
}


std::ostream& operator<< (std::ostream &os, const UpperLayer::Item &i) {
  if (i.organ)
    return os << "{ " << OrganID(i.organ) << ", [" << i.l << ":" << i.r
              << "], " << i.y << " }";
  else
    return os << "{ null, [" << i.l << ":nan], " << i.y << " }";
}

std::ostream& operator<< (std::ostream &os, const Pistil &p) {
  if (p.isValid())
    return os << "{P:" << OrganID(p.organ) << "," << p.boundingDisk << "}";
  else
    return os << "{P:invalid}";
}


// =============================================================================

bool operator< (const Rect &lhs, const Rect &rhs) {
  if (!fuzzyEqual(lhs.ul.x, rhs.ul.x)) return fuzzyLower(lhs.ul.x, rhs.ul.x);
  if (!fuzzyEqual(lhs.br.x, rhs.br.x)) return fuzzyLower(lhs.br.x, rhs.br.x);
  if (!fuzzyEqual(lhs.ul.y, rhs.ul.y)) return fuzzyLower(lhs.ul.y, rhs.ul.y);
  return fuzzyLower(lhs.br.y, rhs.br.y);
}

using fnl = std::numeric_limits<float>;
UpperLayer::Item::Item (void)
  : l(fnl::max()), r(-fnl::max()), y(-fnl::max()), organ(nullptr) {}

namespace canopy {

struct XEvent {
  EventType type;
  Organ *organ;

  float r, t, l;

  XEvent (EventType t, Organ *o, float right, float top, float left)
    : type(t), organ(o), r(right), t(top), l(left) {}

  XEvent (EventType t, Organ *o, const Rect r)
    : XEvent(t, o, r.r(), r.t(), r.l()) {}

  XEvent (EventType t, const UpperLayer::Item &item)
    : XEvent(t, item.organ, item.r, item.y, item.l) {}

  auto left (void) const {  return l; }
  auto right (void) const { return r; }
  auto top (void) const {   return t; }
  auto coord (void) const { return type == IN? left() : right(); }

  friend bool operator< (const XEvent &lhs, const XEvent &rhs) {
    if (!fuzzyEqual(lhs.coord(), rhs.coord()))  return fuzzyLower(lhs.coord(), rhs.coord());
    if (!fuzzyEqual(lhs.top(), rhs.top()))      return fuzzyLower(lhs.top(), rhs.top());
    if (lhs.type != rhs.type)                   return lhs.type < rhs.type;
    return lhs.organ < rhs.organ;
  }

  friend std::ostream& operator<< (std::ostream &os, const XEvent &e) {
    return os << (e.type == IN ? "I" : "O")
              << "Event(" << OrganID(e.organ) << "): " << "x = " << e.coord()
              << ", from " << e.left() << " to " << e.right() << " at "
              << e.top();
  }
};

template <typename T>
struct SortedStack {
  struct Item {
    T value;
    float y;

    mutable uint refCount;

    Item (T v, float y) : value(v), y(y), refCount(1) {}

    friend bool operator< (const Item &lhs, const Item &rhs) {
      if (lhs.y != rhs.y) return lhs.y < rhs.y;
      if (lhs.value->plant()->id() != rhs.value->plant()->id())
        return lhs.value->plant()->id() < rhs.value->plant()->id();
      return lhs.value->id() != rhs.value->id();
    }
  };
  std::set<Item> stack;

  void push (const T &v, float y) {
    Item i (v, y);
    auto it = stack.find(i);
    if (it != stack.end())
      it->refCount++;
    else
      stack.insert(i);
  }

  void erase (const T &v, float y) {
    Item i (v, y);
    auto it = stack.find(i);
    assert(it != stack.end());
    it->refCount--;
    if (it->refCount <= 0)
      stack.erase(i);
  }

  T top (void) const {
    return stack.rbegin()->value;
  }

  bool inTop (const T &value, float y) {
    auto it = stack.rbegin();
    while (it != stack.rend() && it->y == y) {
      if (it->value == value)
        return true;
      ++it;
    }
    return false;
  }

  bool empty (void) const {
    return stack.empty();
  }
};

void resetItem (UpperLayer::Item &i, float left, float top, Organ *organ) {
  i = UpperLayer::Item();
  i.l = left;
  if (!isnan(top)) {
    i.y = top;
    i.organ = organ;
  }
}

void resetItem (UpperLayer::Item &i, float left) {
  resetItem(i, left, NAN, nullptr);
}

void emplaceItem (UpperLayer::Items &items, UpperLayer::Item &i, float right) {
  i.r = right;

  // Ignore segments smaller than 1mm
  if (i.r - i.l > 1e-3) items.push_back(i);
}

using include_f = bool (*) (const Plant *, const UpperLayer::Item &);
bool include_all (const Plant *, const UpperLayer::Item &) {
  return true;
}
bool include_mine (const Plant *plant, const UpperLayer::Item &item) {
  return plant == item.organ->plant();
}

void doLineSweep (const Plant *plant,
                  const std::set<canopy::XEvent> &xevents,
                  UpperLayer::Items &items,
                  include_f include) {

  canopy::SortedStack<Organ*> stack;
  UpperLayer::Item current;

  if (debugUpperLayer)
    std::cerr << PlantID(plant) << " Performing linesweep for upper layer" << std::endl;

  for (canopy::XEvent e: xevents) {
    if (debugUpperLayer)  std::cerr << "\t" << e << std::endl;

    switch (e.type) {
    case IN:
      stack.push(e.organ, e.top());
      if (current.y < e.top()) {
        if (current.organ && include(plant, current)) {
          emplaceItem(items, current, e.left());
          if (debugUpperLayer)  std::cerr << "\t\tEmplaced " << current << std::endl;
        }
        resetItem(current, e.left(), e.top(), e.organ);
        if (debugUpperLayer)  std::cerr << "\t\tNew top " << current << std::endl;
      }
      break;

    case OUT:
      assert(!stack.empty());
      bool inTop = stack.inTop(e.organ, e.top());
//     Organ *top = stack.top();
      stack.erase(e.organ, e.top());

      if (inTop) {
        if (include(plant, current)) {
          emplaceItem(items, current, e.right());
          if (debugUpperLayer)  std::cerr << "\t\tEmplaced " << current << std::endl;
        }

        if (!stack.empty()) {
          Organ *newTop = stack.top();
          resetItem(current, e.right(),
                    newTop->globalCoordinates().boundingRect.t(), newTop);
        } else
          resetItem(current, e.right());
        if (debugUpperLayer)  std::cerr << "\t\tNew current" << current << std::endl;
      }
      break;
    }
  }

  assert(stack.empty());
}

} // end of namespace canopy

void UpperLayer::updateInIsolation (const simu::Environment &env, const Plant *p) {
  std::set<canopy::XEvent> xevents;
  for (Organ *o: p->organs()) {
    if (o->length() <= 0) continue; // Skip non terminals

    const Rect &r = o->globalCoordinates().boundingRect;
    float h = env.heightAt(r.center().x);
    if (r.t() < h)  continue; // Skip below surface

    xevents.emplace(IN, o, r);
    xevents.emplace(OUT, o, r);
  }

  itemsInIsolation.clear();
  canopy::doLineSweep(p, xevents, itemsInIsolation, canopy::include_all);

  if (debugUpperLayer)
    std::cerr << "Extracted " << itemsInIsolation.size()
              << " potential upper layer items from " << p->organs().size()
              << " organs" << std::endl;
}

void UpperLayer::updateInWorld(const Plant *p, const const_Collisions &collisions) {
  if (collisions.empty())
    itemsInWorld = itemsInIsolation;

  else {
    if (debugUpperLayer) {
      std::cerr << PlantID(p) << " Performing canopy narrowphase against [";
      for (const CollisionObject *object: collisions)
        std::cerr << " " << object->plant->id();
      std::cerr << " ] " << std::endl;
    }

    const Rect aabb = p->translatedBoundingRect();
    std::set<canopy::XEvent> xevents;

    for (const UpperLayer::Item &i: itemsInIsolation) {
      if (debugUpperLayer)
        std::cerr << "\tInserting events for " << i << ":\n\t\t"
                  << canopy::XEvent(IN, i) << "\n\t\t" << canopy::XEvent(OUT, i)
                  << std::endl;
      xevents.emplace(IN, i);
      xevents.emplace(OUT, i);
    }

    for (const CollisionObject *object: collisions) {
      for (const UpperLayer::Item &i: object->layer.itemsInIsolation) {
        if (i.r >= aabb.l() && aabb.r() >= i.l) {
          if (debugUpperLayer)
            std::cerr << "\tInserting events for " << i << ":\n\t\t"
                      << canopy::XEvent(IN, i) << "\n\t\t"
                      << canopy::XEvent(OUT, i) << std::endl;
          xevents.emplace(IN, i);
          xevents.emplace(OUT, i);
        }
      }
    }

    itemsInWorld.clear();
    canopy::doLineSweep(p, xevents, itemsInWorld, canopy::include_mine);

    if (debugUpperLayer)
      std::cerr << "Extract in " << itemsInWorld.size()
                << " upper layer items from " << xevents.size() / 2
                << " combined items" << std::endl;
  }
}

// =============================================================================

void TinyPhysicsEngine::reset(void) {
  _data.clear();
}

namespace broadphase {

struct XEvent {
  float x;
  EventType type;
  CollisionObject *object;

  friend bool operator< (const XEvent &lhs, const XEvent &rhs) {
    if (lhs.x != rhs.x) return lhs.x < rhs.x;
    if (lhs.type < rhs.type)  return lhs.type < rhs.type;
    return lhs.object < rhs.object;
  }

  friend std::ostream& operator<< (std::ostream &os, const XEvent &e) {
    return os << (e.type == IN ? "I" : "O") << "Event(" << e.x << ",P"
              << e.object->plant->id() << ")";
  }
};

bool includes (const Rect &lhs, const Rect &rhs) {
  return lhs.l() <= rhs.l() && rhs.r() <= lhs.r();
}

bool includes (const CollisionObject *lhs, const CollisionObject *rhs) {
  return includes(lhs->boundingRect, rhs->boundingRect);
}

void englobes (CollisionObject *lhs, CollisionObject *rhs) {
  if (debugBroadphase)
    std::cerr << PlantID(lhs->plant) << " englobes "
              << PlantID(rhs->plant) << std::endl;

  lhs->englobedObjects.insert(rhs);
  rhs->englobingObjects.insert(lhs);
}

void noLongerEnglobes (CollisionObject *lhs, CollisionObject *rhs) {
  if (debugBroadphase)
    std::cerr << PlantID(lhs->plant) << " no longer englobes "
              << PlantID(rhs->plant) << std::endl;

  lhs->englobedObjects.erase(rhs);
  rhs->englobingObjects.erase(lhs);
}

} // end of namespace broadphase

void TinyPhysicsEngine::processNewObjects(void) {
  // Linesweep to detect englobing plants (i-e completely containing another)
  using namespace broadphase;
  std::set<XEvent> xevents;
  for (auto *obj: _data) {
    xevents.insert(XEvent{obj->boundingRect.l(), IN, obj});
    xevents.insert(XEvent{obj->boundingRect.r(), OUT, obj});
//    obj->englobingObjects.clear();
  }

  if (debugBroadphase)
    std::cerr << "Registering broadphase collision for newly created objects"
              << std::endl;

  std::set<CollisionObject*> inside;
  for (const XEvent &e: xevents) {
    if (debugBroadphase)  std::cerr << "\t" << e << std::endl;
    switch (e.type) {
    case IN:
      for (CollisionObject *lhs: inside)
        if (includes(lhs, e.object))
          englobes(lhs, e.object);
      inside.insert(e.object);
      break;
    case OUT:
      inside.erase(e.object);
      break;
    }
    if (debugBroadphase) {
      std::cerr << "\t\tInside [";
      for (CollisionObject *object: inside)
        std::cerr << " " << object->plant->id();
      std::cerr << " ]" << std::endl;
    }
  }
}

#ifndef NDEBUG
void TinyPhysicsEngine::debug (void) const {
#if 1
  // Check that no pistil outlive its plant
  for (const Pistil &ps: _pistils) {
    if (find(ps.organ->plant()) == _data.end())
      utils::doThrow<std::logic_error>("Leftover pistil!");
  }
#endif
#if 0
  // Check that every pistil is correctly placed
  for (const CollisionObject *co: _data) {
    const Plant *p = co->plant;
    for (const Pistil &ps: _pistils) {
      if (ps.organ->plant() != p)  continue;

      std::map<float, std::vector<Organ*>> pistils;
      for (Organ *o: p->flowers())
        pistils[int(o->globalCoordinates().center.x * floatPrecision) / floatPrecision].push_back(o);

      for (auto &pair: pistils) {
        bool found = false, matched = false;
        for (Organ *o: pair.second) {
          auto it = _pistils.find(o->globalCoordinates().center);
          if (it != _pistils.end()) {
            found = true;

            Disk bd = it->boundingDisk;
            if(bd.center == o->globalCoordinates().center) {
              matched = true;
              break;
            }
          }
        }
        if (!(found && matched)) {
          std::cerr << "Something went wrong... (" << found << matched << ")\n";
          std::cerr << "Here is the replay:\n";
          found = false, matched = false;
          uint i = 1;
          for (Organ *o: pair.second) {
            std::cerr << "Attempt " << i++ << " / " << pair.second.size() << "\n";
            std::cerr << "\tLooking for " << OrganID(o) << " at "
                      << o->globalCoordinates().center << "\n";
            auto it = _pistils.find(o->globalCoordinates().center);
            if (it != _pistils.end()) {
              found = true;

              std::cerr << "\t\tFound something!\n";

              Disk bd = it->boundingDisk;
              std::cerr << "\t\t" << bd.center << " ~= "
                        << o->globalCoordinates().center << "? ";
              if(bd.center == o->globalCoordinates().center) {
                std::cerr << "Match!\n";
                matched = true;
                break;
              } else
                std::cerr << "Mismatch...\n";

            } else
              std::cerr << "\t\tNot found...\n";
          }
          std::cerr << "Here are the pistils for "
                    << PlantID(p) << ":" << std::endl;
          for (const Pistil &ps: _pistils)
            if (ps.organ->plant() == p)
              std::cerr << "\t" << ps << "\n";
        }
        if(!(found && matched))
          utils::doThrow<std::logic_error>("Something went wrong...");
      }
    }
  }
#endif
}
#endif

Collisions::iterator TinyPhysicsEngine::find (const Plant *p) {
  auto it = _data.find(*p);
  if (it == _data.end())
    utils::doThrow<std::logic_error>("No collision data for plant ", p->id());
  return it;
}

Collisions::const_iterator TinyPhysicsEngine::find (const Plant *p) const {
  auto it = _data.find(*p);
  if (it == _data.end())
    utils::doThrow<std::logic_error>("No collision data for plant ", p->id());
  return it;
}

const UpperLayer::Items& TinyPhysicsEngine::canopy(const Plant *p) const {
  return (*find(p))->layer.itemsInWorld;
}

bool TinyPhysicsEngine::addCollisionData (const Environment &env, Plant *p) {
  CollisionObject *object = new CollisionObject (env, p);

  auto res = _data.insert(object);
  if (res.second) {
    const auto &aabb = object->boundingRect;
    object->leftEdge->edge = aabb.l();
    _leftEdges.insert(object->leftEdge.get());
    object->rightEdge->edge = aabb.r();
    _rightEdges.insert(object->rightEdge.get());

    if (debugCollision)
      std::cerr << "Inserted collision data " << aabb << " for "
                << p->id() << std::endl;
  } else {
    delete object;

    if (debugCollision)
      std::cerr << "Could not insert plant " << p->id() << std::endl;
  }

  return res.second;
}

void TinyPhysicsEngine::removeCollisionData (Plant *p) {
  auto it = find(p);
  CollisionObject *object = *it;

  /// Update colliding objects canopies
  const_Collisions thisCollisions;
  broadphaseCollision(object, thisCollisions);
  for (const CollisionObject *that: thisCollisions) {
    const_Collisions thatCollisions;
    broadphaseCollision(that, thatCollisions);
    thatCollisions.erase(object);
    that->layer.updateInWorld(that->plant, thatCollisions);
  }

  // Delete edges
  _leftEdges.erase(object->leftEdge.get());
  _rightEdges.erase(object->rightEdge.get());

  // Delete references in (potentially) englobed/englobing objects
  for (CollisionObject *other: object->englobedObjects)
    broadphase::noLongerEnglobes(object, other);
  for (CollisionObject *other: object->englobingObjects)
    broadphase::noLongerEnglobes(other, object);

  // Also delete any remaining pistils
//  /// FIXME This stinks to high heavens: O(n) with no reason...
  for (auto itP = _pistils.begin(); itP != _pistils.end();) {
    if (itP->organ->plant() == p)
      itP = _pistils.erase(itP);
    else  ++itP;
  }
//  for (const Organ *p: object->plant->flowers())  delPistil(p);

  _data.erase(it);
  delete object;
}

void TinyPhysicsEngine::updateCollisions (Plant *p) {
  auto it = find(p);
  CollisionObject *object = *it;
  _data.erase(it);
  if (debugCollision) std::cerr << "\nUpdated collision data for " << p->id()
                                << " from " << object->boundingRect;
  object->updateCollisions();
  if (debugCollision) std::cerr << " to " << object->boundingRect << std::endl;
  it = _data.insert(object).first;

  if (object->leftEdge->edge != object->boundingRect.l())
    updateEdge(object->leftEdge.get(), object->boundingRect.l());
  if (object->rightEdge->edge != object->boundingRect.r())
    updateEdge(object->rightEdge.get(), object->boundingRect.r());
}

void TinyPhysicsEngine::updateFinal (const Environment &env, Plant *p) {
  auto it = find(p);
  CollisionObject *object = *it;
  _data.erase(it);

  object->updateFinal(env); // Update bounding box and canopy
  it = _data.insert(object).first;

  const auto aabb = object->boundingRect;
  if (object->leftEdge->edge != aabb.l())  updateEdge(object->leftEdge.get(), aabb.l());
  if (object->rightEdge->edge != aabb.r())  updateEdge(object->rightEdge.get(), aabb.r());

  if (debugBroadphase)
    std::cerr << PlantID(p) << " Performing post-derivation physics update"
              << std::endl;

  // Check that englobing object are still englobing
  Collisions oldEnglobing;
  for (CollisionObject *that: object->englobingObjects)
    if (!broadphase::includes(that, object))
      oldEnglobing.insert(that);
  for (CollisionObject *that: oldEnglobing)
    broadphase::noLongerEnglobes(that, object);

  if (debugBroadphase) {
    std::cerr << "\tEnglobing objects are:";
    for (CollisionObject *that: object->englobingObjects)
      std::cerr << " " << that->plant->id();
    std::cerr << std::endl;
  }

  // Search plants in this' bounding box to warn about inclusion
  for (auto itL = std::reverse_iterator(it);
            itL!=_data.rend() && aabb.l() <= (*itL)->plant->pos().x; ++itL)
    if (broadphase::includes(object, *itL))
      broadphase::englobes(object, *itL);

  for (auto itR = std::next(it);
       itR != _data.end() && (*itR)->plant->pos().x <= aabb.r(); ++itR)
    if (broadphase::includes(object, *itR))
      broadphase::englobes(object, *itR);

  if (debugBroadphase) {
    std::cerr << "\tEnglobed objects are:";
    for (CollisionObject *that: object->englobedObjects)
      std::cerr << " " << that->plant->id();
    std::cerr << std::endl;
  }

  // Update this' and neighbors' (world) canopies
  const_Collisions thisCollisions;
  broadphaseCollision(object, thisCollisions);
  object->layer.updateInWorld(object->plant, thisCollisions);

  for (const CollisionObject *neighbor: thisCollisions) {
    const_Collisions thatCollisions;
    broadphaseCollision(neighbor, thatCollisions);
    neighbor->layer.updateInWorld(neighbor->plant, thatCollisions);
  }
}


// =============================================================================

namespace narrowphase {

enum EventDir { X, Y };
enum Flag { LHS, RHS };

using Organs = Organ::const_Collection;

template <EventDir D>
struct Event {
  EventType type;
  Organ const * const organ;
  Flag flag;

  Event (EventType t, const Organ *o, Flag f) : type(t), organ(o), flag(f) {}

  auto coord (void) const;

  friend bool operator< (const Event &lhs, const Event &rhs) {
    if (lhs.coord() != rhs.coord()) return lhs.coord() < rhs.coord();
    // Do not care at this point just make them different
    return lhs.organ < rhs.organ;
  }

  friend std::ostream& operator<< (std::ostream &os, const Event &e) {
    return os << (D == X ? "X" : "Y")
              << (e.type == IN ? "I" : "O" )
              << OrganID(e.organ) << " at " << e.coord();
  }
};

template <>
auto Event<X>::coord (void) const {
  const auto &aabb = organ->globalCoordinates().boundingRect;
  return (type == IN) ? aabb.l() : aabb.r();
}

template <>
auto Event<Y>::coord (void) const {
  const auto &aabb = organ->globalCoordinates().boundingRect;
  return (type == IN) ? aabb.b() : aabb.t();
}

struct Item {
  Organ const * const organ;
  const Flag flag;


  Item (const Event<Y> yevent) : organ(yevent.organ), flag(yevent.flag) {}

  friend bool operator< (const Item &lhs, const Item &rhs) {
    return lhs.organ < rhs.organ;
  }
};

Point mainNormal (const Organ *o) {
  const Organ::GlobalCoordinates &coords = o->globalCoordinates();
  return {
    coords.center.x - coords.origin.x,
    coords.center.y - coords.origin.y,
  };
}

float dotProduct (const Point &lhs, const Point &rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y;
}

/*!
 * \brief collisionSAT
 *
 * Performs a collision detection between two oriented rectangles using the
 * Separating Axis Theorem
 * \author Kah Shiu Chong, at https://gamedevelopment.tutsplus.com/tutorials/collision-detection-using-the-separating-axis-theorem--gamedev-169
 *
 * \param lhs Left hand side organ for the collision check
 * \param rhs Right hand side organ for the collision check
 * \return Whether or not a collision was detected
 */
bool collisionSAT (const Organ *lhs, const Organ *rhs) {
  if (debugNarrowphase)
    std::cerr << "Searching separating axis between "
              << OrganID(lhs) << " (" << lhs->globalCoordinates().boundingRect
              << ") and "
              << OrganID(rhs) << " (" << rhs->globalCoordinates().boundingRect
              << ")" << std::endl;

  assert(intersects(lhs->globalCoordinates().boundingRect,
                    rhs->globalCoordinates().boundingRect));

  // Compute normals
  std::vector<Point> normals;
  normals.push_back(mainNormal(lhs));
  normals.push_back({-normals[0].y, normals[0].x});
  if (lhs->inPlantCoordinates().rotation != rhs->inPlantCoordinates().rotation) {
    normals.push_back(mainNormal(rhs));
    normals.push_back({-normals[2].y, normals[2].x});
  }

  // Project onto normal
  for (const Point &n: normals) {
    const Organ::Corners &lhsCorners = lhs->globalCoordinates().corners,
                         &rhsCorners = rhs->globalCoordinates().corners;

    float min_proj_lhs = dotProduct(lhsCorners[0], n),
          max_proj_lhs = min_proj_lhs,
          min_proj_rhs = dotProduct(rhsCorners[0], n),
          max_proj_rhs = min_proj_rhs;

    // Comptude min & max projections
    for (uint i=1; i<4; i++) {
      float proj_lhs = dotProduct(lhsCorners[i], n);
      if (proj_lhs < min_proj_lhs)  min_proj_lhs = proj_lhs;
      if (max_proj_lhs < proj_lhs)  max_proj_lhs = proj_lhs;

      float proj_rhs = dotProduct(rhsCorners[i], n);
      if (proj_rhs < min_proj_rhs)  min_proj_rhs = proj_rhs;
      if (max_proj_rhs < proj_rhs)  max_proj_rhs = proj_rhs;
    }

    if (max_proj_lhs <= min_proj_rhs || max_proj_rhs <= min_proj_lhs) {
      if (debugNarrowphase) std::cerr << "\t\tFound " << n << std::endl;
      return false; // No collision
    }

    // Maybe collision: keep searching
  }

  if (debugNarrowphase) std::cerr << "\t\tFound none" << std::endl;

  return true;  // No separating axis found: there must be a collision
}

template <typename F>
bool narrowPhaseCollision (const Organs &lhs, const Organs &rhs,
                           const F &filter) {

  utils::IndentingOStreambuf indent(std::cerr);

  // No collision if one of the plant has no organ in the intersection
  uint lhsO = lhs.size(), rhsO = rhs.size();
  if (!(lhsO && rhsO)) {
    if (debugNarrowphase)
      std::cerr << "Object " << (!lhsO ? "lhs" : "rhs") << " has no "
                << "candidate organ(s), no collision(s) possible" << std::endl;
    return false;
  }

  std::set<Event<X>> xevents;
  for (const Organ *o: lhs) {
    xevents.emplace(IN, o, LHS);
    xevents.emplace(OUT, o, LHS);
  }

  for (const Organ *o: rhs) {
    xevents.emplace(IN, o, RHS);
    xevents.emplace(OUT, o, RHS);
  }

  // Linesweep to find lhs-rhs organ collision pair
  std::set<Item> inside;
  std::set<Event<Y>> yevents;
  for (const Event<X> &xevent: xevents) {
    std::initializer_list<Event<Y>> new_yevents {
      Event<Y> (IN, xevent.organ, xevent.flag),
      Event<Y> (OUT, xevent.organ, xevent.flag)
    };

    if (debugNarrowphase) std::cerr << xevent << std::endl;
    if (xevent.type == IN)
          yevents.insert(new_yevents);

    else {
      for (auto &e: new_yevents)  yevents.erase(e);

      // Organ visited reduce corresponding count
      if (xevent.flag == LHS)
            lhsO--;
      else  rhsO--;
    }

    // No collision if all the candidate organs of one plant have been visited
    if (!(lhsO && rhsO)) {
      if (debugNarrowphase)
        std::cerr << "Seen all organs of " << (!lhsO ? "lhs" : "rhs") << ", no"
                  << " more collision(s) possible" << std::endl;
      return false;
    }

    for (const Event<Y> &yevent: yevents) {
      utils::IndentingOStreambuf indent(std::cerr);

      if (debugNarrowphase) std::cerr << yevent << std::endl;
      if (yevent.type == IN) {
        const Organ *lhs = yevent.organ;
        for (const Item &i: inside) {
          utils::IndentingOStreambuf indent(std::cerr);

          if (debugNarrowphase)
            std::cerr << OrganID(lhs) << "/" << OrganID(i.organ)
                      << ": ";

          if (yevent.flag == i.flag) {
            if (debugNarrowphase) std::cerr << "Same sets" << std::endl;
            continue;
          }

          if (filter(yevent.organ, i.organ)) {
            if (debugNarrowphase) std::cerr << "Filtered out" << std::endl;
            continue;
          }

          if(collisionSAT(lhs, i.organ))
            return true;
        }

        inside.insert(Item(yevent));

      } else
        inside.erase(Item(yevent));
    }
  }

  return false;
}

} // end of namespace narrowphase

bool TinyPhysicsEngine::narrowPhaseCollision(const Branch &self,
                                             const Branch &branch) {

  Rect i = intersection(self.bounds, branch.bounds);
  if (debugNarrowphase)
    std::cerr << "Narrowphase collision of " << i << " between\n\tSelf:";

  narrowphase::Organs lhsOrgans, rhsOrgans;

  // Collect organs in each container at least touching the self-branch
  // intersection
  for (const Organ *o: self.organs) {
    if (intersects(i, o->globalCoordinates().boundingRect)) {
      if (debugNarrowphase) std::cerr << " " << OrganID(o, true);
      lhsOrgans.insert(o);
    }
  }

  if (debugNarrowphase)
    std::cerr << "\n\tBranch:";

  for (const Organ *o: branch.organs) {
    if (intersects(i, o->globalCoordinates().boundingRect)) {
      if (debugNarrowphase) std::cerr << " " << OrganID(o, true);
      rhsOrgans.insert(o);
    }
  }

  if (debugNarrowphase) std::cerr << std::endl;

  return narrowphase::narrowPhaseCollision(lhsOrgans, rhsOrgans,
    [] (const Organ *lhs, const Organ *rhs) {
      return lhs->parent() == rhs->parent();
  });
}

bool
TinyPhysicsEngine::narrowPhaseCollision (const Plant *lhs, const Plant *rhs,
                                         const Branch &branch,
                                         const Rect &intersection) {

//  using namespace narrowphase;

  if (debugNarrowphase)
    std::cerr << "Narrowphase collision of " << intersection
              << " between\n\t" << PlantID(lhs) << ":";

  narrowphase::Organs lhsOrgans, rhsOrgans;

  // Collect organs in each plant at least touching the plant-plant intersection
  for (const Organ *o: branch.organs) {
    if (intersects(intersection, o->globalCoordinates().boundingRect)) {
      if (debugNarrowphase) std::cerr << " " << OrganID(o, true);
      lhsOrgans.insert(o);
    }
  }

  if (debugNarrowphase)
    std::cerr << "\n\t" << PlantID(rhs) << ":";

  for (const Organ *o: rhs->organs()) {
    if (intersects(intersection, o->globalCoordinates().boundingRect)) {
      if (debugNarrowphase) std::cerr << " " << OrganID(o, true);
      rhsOrgans.insert(o);
    }
  }

  if (debugNarrowphase) std::cerr << std::endl;

  return narrowphase::narrowPhaseCollision(lhsOrgans, rhsOrgans,
    [] (const Organ *, const Organ *) { return false;  });
}


// =============================================================================

bool intersects (const CollisionObject *lhs, const CollisionObject *rhs) {
  return intersects(lhs->boundingRect, rhs->boundingRect);
}

template <>
TinyPhysicsEngine::Edges<EdgeSide::LEFT>&
TinyPhysicsEngine::getEdges<EdgeSide::LEFT> (void) {
  return _leftEdges;
}

template <>
TinyPhysicsEngine::Edges<EdgeSide::RIGHT>&
TinyPhysicsEngine::getEdges<EdgeSide::RIGHT> (void) {
  return _rightEdges;
}

template <EdgeSide S>
void TinyPhysicsEngine::updateEdge (Edge<S> *edge, float val) {
  auto &edges = getEdges<S>();
  if (edge->edge != val) {
    edges.erase(edge);
    edge->edge = val;
    edges.insert(edge);
  }
}

void TinyPhysicsEngine::broadphaseCollision(const CollisionObject *object,
                                            const_Collisions &objects,
                                            const Rect otherBounds) {

  // Update edges to include new organs
  Rect unitedBounds = object->boundingRect;
  const float lEdge = object->leftEdge->edge,   // Keep backups
              rEdge = object->rightEdge->edge;  //
  if (otherBounds.isValid()) {
    unitedBounds.uniteWith(otherBounds);
    if (unitedBounds.l() < object->leftEdge->edge)
      updateEdge(object->leftEdge.get(), unitedBounds.l());
    if (object->rightEdge->edge < unitedBounds.r())
      updateEdge(object->rightEdge.get(), unitedBounds.r());
  }

  if (debugBroadphase)
    std::cerr << "\tOriginal bounds: " << object->boundingRect
              << "\n\t   Other bounds: " << otherBounds
              << "\n\t  United bounds: " << unitedBounds << std::endl;

  { // Start from right, scanning for edges untils reaching left
    if (debugBroadphase)  std::cerr << "\tScanning right to left:";
    auto it = _rightEdges.find(object->rightEdge.get());
    auto rit = typename Edges<RIGHT>::reverse_iterator(it);
    while (rit != _rightEdges.rend() && object->leftEdge->edge < (*rit)->edge) {
      if (debugBroadphase)  std::cerr << " " << (*rit)->object->plant->id();
      objects.insert((*rit)->object);
      rit = std::next(rit);
    }
    if (debugBroadphase)  std::cerr << std::endl;
  }
  { // Start from left edge, scanning for edges until reaching right
    if (debugBroadphase)  std::cerr << "\tScanning left to right:";
    auto it = std::next(_leftEdges.find(object->leftEdge.get()));
    while (it != _leftEdges.end() && (*it)->edge < object->rightEdge->edge) {
      if (debugBroadphase)  std::cerr << " " << (*it)->object->plant->id();
      objects.insert((*it)->object);
      it = std::next(it);
    }
    if (debugBroadphase)  std::cerr << std::endl;
  }

  if (debugBroadphase) {
    std::cerr << "\tEnglobing objects:";
    for (const CollisionObject *eng: object->englobingObjects)
      std::cerr << " " << eng->plant->id();
    std::cerr << std::endl;
  }

  // Restore edges if needed
  if (object->leftEdge->edge < lEdge) updateEdge(object->leftEdge.get(), lEdge);
  if (rEdge < object->rightEdge->edge) updateEdge(object->rightEdge.get(), rEdge);

  // Include (potentially) englobing objects
  objects.insert(object->englobingObjects.begin(),
                        object->englobingObjects.end());

  objects.erase(object); // No self collision
}

bool TinyPhysicsEngine::isCollidingWithSelf(const Branch &s, const Branch &b) {
  return narrowPhaseCollision(s, b);
}

bool TinyPhysicsEngine::isCollidingWithOthers (const Plant *p,
                                               const Branch &branch) {
  if (debugCollision) std::cerr << "\nTesting collisions for " << p->id() << std::endl;

  Rect otherBounds = Rect::invalid();
  if (!broadphase::includes(p->boundingRect(), branch.bounds))
    otherBounds = branch.bounds;

  CollisionObject *object = *find(p);
  const_Collisions aabbCandidates;
  broadphaseCollision(object, aabbCandidates, otherBounds);

  if (debugCollision) {
    std::cerr << "Possible collisions for " << p->id() << " ("
              << object->boundingRect << "): " << aabbCandidates.size() << " [";
    for (const CollisionObject *o: aabbCandidates)
      std::cerr << " " << o->plant->id();
    std::cerr << " ]" << std::endl;
  }

  for (const CollisionObject *that: aabbCandidates) {
    // Ignore collision with seeds
    if (that->plant->isInSeedState()) continue;

    Rect i = intersection(that->boundingRect, branch.bounds);
    if (i.isValid()
      && narrowPhaseCollision(object->plant, that->plant, branch, i))
      return true;
  }

  return false;
}


// =============================================================================


bool operator< (const Pistil &lhs, const Pistil &rhs) {
  return fuzzyLower(lhs.boundingDisk.center.x, rhs.boundingDisk.center.x);
}

bool operator< (const Pistil &lhs, const Point &rhs) {
  return fuzzyLower(lhs.boundingDisk.center.x, rhs.x);
}

bool operator< (const Point &lhs, const Pistil &rhs) {
  return fuzzyLower(lhs.x, rhs.boundingDisk.center.x);
}

Disk boundingDiskFor (const Organ *o) {
  return {
    o->globalCoordinates().center,
    (o->inPlantCoordinates().center.y + 1) * 5.f
  };
}

void TinyPhysicsEngine::addPistil(Organ *p) {
#ifdef NDEBUG
  _pistils.emplace(p, boundingDiskFor(p));
#else
  auto res = _pistils.emplace(p, boundingDiskFor(p));
  assert(p->globalCoordinates().center == boundingDiskFor(p).center);
  if (debugReproduction) {
    if (res.second)
      std::cerr << "Added " << *res.first << std::endl;
    else
      std::cerr << "Unable to insert pistil " << Pistil(p, boundingDiskFor(p))
                << " (equal to " << *res.first << ")" << std::endl;
  }
#endif
}

void TinyPhysicsEngine::delPistil(const Organ *p) {
  auto it = _pistils.find(p->globalCoordinates().center);
  if (it != _pistils.end() && it->organ == p) {
    if (debugReproduction)  std::cerr << "Deleting " << *it << std::endl;

    _pistils.erase(it);
  } else {
#if !defined(NDEBUG) && 1

#endif
    if (debugReproduction)  std::cerr << "Could not delete pistil for " << OrganID(p)
                                      << " at " << p->globalCoordinates().center
                                      << ": not found" << std::endl;
  }
}

void TinyPhysicsEngine::updatePistil (Organ *p, const Point &oldPos) {
  auto it = _pistils.find(oldPos);
  if (it != _pistils.end() && it->organ == p) {
    Pistil s = *it;
    _pistils.erase(it);

    if (debugReproduction)
      std::cerr << "Updating" << s;

    s.boundingDisk = boundingDiskFor(p);

    if (debugReproduction)
      std::cerr << " >> " << s.boundingDisk << std::endl;

    _pistils.insert(s);
  } else {
    if (debugReproduction)
      std::cerr << "Could not find pistil for " << OrganID(p) << " at pos "
                << oldPos << " thus ";
    addPistil(p);
  }
}

struct LowerBound {
  Disk boundingDisk;

  static bool leftOf (const Disk &lhs, const Disk &rhs) {
    return fuzzyLower(lhs.center.x + lhs.radius, rhs.center.x - rhs.radius);
  }

  friend bool operator< (const LowerBound &lhs, const Pistil &rhs) {
    return leftOf(lhs.boundingDisk, rhs.boundingDisk);
  }

  friend bool operator< (const Pistil &lhs, const LowerBound &rhs) {
    return leftOf(lhs.boundingDisk, rhs.boundingDisk);
  }
};
TinyPhysicsEngine::Pistils_range TinyPhysicsEngine::sporesInRange(Organ *s) {
  Disk boundingDisk = boundingDiskFor(s);
  auto itL = _pistils.lower_bound(LowerBound{boundingDisk});
  if (itL == _pistils.end())  return { itL, itL };

  auto itR = itL;
  while (itR != _pistils.end() && intersects(itR->boundingDisk, boundingDisk))
    itR = std::next(itR);

  return { itL, itR };
}

// =============================================================================

void TinyPhysicsEngine::postLoad(void) {
  processNewObjects();

  for (CollisionObject *obj: _data) {
    const_Collisions thisCollisions;
    broadphaseCollision(obj, thisCollisions);
    obj->layer.updateInWorld(obj->plant, thisCollisions);
  }
}

// =============================================================================

template <EdgeSide S>
void assertEqual (const Edge<S> &lhs, const Edge<S> &rhs) {
  utils::assertEqual(lhs.object->plant->id(), rhs.object->plant->id());
  utils::assertEqual(lhs.edge, rhs.edge);
}

void assertEqual(const UpperLayer::Item &lhs, const UpperLayer::Item &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.l, rhs.l);
  assertEqual(lhs.r, rhs.r);
  assertEqual(lhs.y, rhs.y);
  assertEqual(lhs.organ->id(), rhs.organ->id());
}

void assertEqual(const UpperLayer &lhs, const UpperLayer &rhs) {
  utils::assertEqual(lhs.itemsInIsolation, rhs.itemsInIsolation);
  utils::assertEqual(lhs.itemsInWorld, rhs.itemsInWorld);
}

void assertEqual(const Pistil &lhs, const Pistil &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.organ->id(), rhs.organ->id());
  assertEqual(lhs.boundingDisk, rhs.boundingDisk);
}

void assertEqualShallow (const Collisions &lhs, const Collisions &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.size(), rhs.size());
  for (auto lhsIt = lhs.begin(), rhsIt = rhs.begin();
       lhsIt != lhs.end(); ++lhsIt, ++rhsIt)
    assertEqual((*lhsIt)->plant->id(), (*rhsIt)->plant->id());
}

void assertEqual (const CollisionObject &lhs, const CollisionObject &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.plant->id(), rhs.plant->id());
  assertEqual(lhs.boundingRect, rhs.boundingRect);
  assertEqualShallow(lhs.englobedObjects, rhs.englobedObjects);
  assertEqualShallow(lhs.englobingObjects, rhs.englobingObjects);
  assertEqual(lhs.leftEdge, rhs.leftEdge);
  assertEqual(lhs.rightEdge, rhs.rightEdge);
  assertEqual(lhs.layer, rhs.layer);
}

void assertEqual (const TinyPhysicsEngine &lhs, const TinyPhysicsEngine &rhs) {
  using utils::assertEqual;
  assertEqual(lhs._data, rhs._data);
  assertEqual(lhs._leftEdges, rhs._leftEdges);
  assertEqual(lhs._rightEdges, rhs._rightEdges);
  assertEqual(lhs._pistils, rhs._pistils);
}

} // end of namespace physics
} // end of namespace simu
