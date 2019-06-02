#include <stack>

#include "kgd/utils/indentingostream.h"

#include "tiniestphysicsengine.h"
#include "environment.h"

/// FIXME Pistils yet again bugging out

namespace simu {
namespace physics {

enum EventType { IN = 1, OUT = 2 };

static constexpr bool debugBroadphase = false;
static constexpr bool debugNarrowphase = false;
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

using Organs = Organ::Collection;
using const_Organs = Organ::const_Collection;

template <EventDir D>
struct Event {
  EventType type;
  Organ const * const organ;
  const int flag;

  Event (EventType t, const Organ *o, int f = 0) : type(t), organ(o), flag(f) {}

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

using XEvent = Event<X>;
using YEvent = Event<Y>;
template<typename E> using Events = std::set<E>;
using XEvents = Events<XEvent>;
using YEvents = Events<YEvent>;

enum PreSAT {
  SKIP,   ///< No need to check for separating axis
  ABORT,  ///< Invalid state detected before collision test
  TEST    ///< Validity rests upon result of separating axis test
};

struct Item {
  Organ const * const organ;
  const int flag;

  Item (const YEvent yevent) : organ(yevent.organ), flag(yevent.flag) {}

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

template <typename FUNCTOR>
bool narrowPhaseCollision (FUNCTOR &functor) {

  // == Debug ==
  auto indent = utils::make_if<debugNarrowphase,
                               utils::IndentingOStreambuf>(std::cerr);
  // ===========

  XEvents xevents;
  if (!functor.prepare(xevents))  return false;

  // Linesweep to find lhs-rhs organ collision pair
  std::set<Item> inside;
  YEvents yevents;
  for (const XEvent &xevent: xevents) {
    std::initializer_list<YEvent> new_yevents {
      Event<Y> (IN, xevent.organ, xevent.flag),
      Event<Y> (OUT, xevent.organ, xevent.flag)
    };

    if (debugNarrowphase) std::cerr << xevent << std::endl;
    if (xevent.type == IN)
          yevents.insert(new_yevents);
    else  for (auto &e: new_yevents)  yevents.erase(e);
    if (!functor.processXEvent(xevent)) return false;

    for (const YEvent &yevent: yevents) {
      // == Debug ==
      auto indent = utils::make_if<debugNarrowphase,
                                   utils::IndentingOStreambuf>(std::cerr);
      if (debugNarrowphase) std::cerr << yevent << std::endl;
      // ===========

      if (yevent.type == IN) {
        const Organ *lhs = yevent.organ;
        for (const Item &i: inside) {
          // == Debug ==
          auto indent = utils::make_if<debugNarrowphase,
                                       utils::IndentingOStreambuf>(std::cerr);

          if (debugNarrowphase)
            std::cerr << OrganID(lhs) << "/" << OrganID(i.organ) << std::endl;
          // ===========

          switch (functor.preSAT(yevent, i)) {
          case SKIP:  continue;
          case ABORT: return true;
          case TEST:  break;
          }

          if (collisionSAT(lhs, i.organ)) return true;
        }

        inside.insert(Item(yevent));

      } else
        inside.erase(Item(yevent));
    }
  }

  return false;
}

bool alignedOrgans (const Organ *lhs, const Organ *rhs) {
  return lhs->globalCoordinates().origin == rhs->globalCoordinates().origin
      && lhs->inPlantCoordinates().rotation == rhs->inPlantCoordinates().rotation;
}

const Organ* terminalParent (const Organ *o) {
  Organ const *p = o->parent();
  while (p && p->isNonTerminal()) p = p->parent();
  return p;
}

bool immediateFamily(const Organ *lhs, const Organ *rhs) {
  auto lhsTP = terminalParent(lhs), rhsTP = terminalParent(rhs);
  return lhsTP == rhsTP // siblings
      || lhs == rhsTP || lhsTP == rhs;  // parent-child
}

auto intraPlantCollisionTest (const Organ *lhs, const Organ *rhs) {
  if (lhs->isNonTerminal()) return SKIP;
  if (rhs->isNonTerminal()) return SKIP;
  if (alignedOrgans(lhs, rhs))  return ABORT;
  if (immediateFamily(lhs, rhs)) return SKIP;
  return TEST;
}

struct AutocollisionFunctor {
  const Organs &organs;

  AutocollisionFunctor (const Organs &o) : organs(o) {}

  bool prepare (XEvents &xevents) {
    for (const Organ *o: organs) {
      xevents.emplace(IN, o);
      xevents.emplace(OUT, o);
    }

    return true;  // Good to go
  }

  bool processXEvent (const XEvent &/*xevent*/) {
    return true;
  }

  auto preSAT (const YEvent &lhs, const Item &rhs) {
    return intraPlantCollisionTest(lhs.organ, rhs.organ);
  }
};

struct BranchcollisionFunctor {
  const Organs &organs;
  const Branch &branch;

  uint counts [2];

  BranchcollisionFunctor (const Organs &o, const Branch &b)
    : organs(o), branch(b) {}

  bool prepare (XEvents &xevents) {
    if (organs.empty()) return false;
    for (const Organ *o: organs) {
      xevents.emplace(IN, o, 1);
      xevents.emplace(OUT, o, 1);
    }
    counts[0] = organs.size();

    for (Organ *o: branch.organs) {
      if (organs.find(o) == organs.end()) {
        xevents.emplace(IN, o, 2);
        xevents.emplace(OUT, o, 2);
        counts[1]++;
      }
    }
    if (!counts[1]) return false;

    return true;  // Good to go
  }

  bool processXEvent (const XEvent &xevent) {
    // Organ visited reduce corresponding count
    if (xevent.type == OUT) counts[xevent.flag-1]--;

    // No collision if all the candidate organs of one plant have been visited
    if (!(counts[0] && counts[1])) {
      if (debugNarrowphase)
        std::cerr << "Seen all organs of " << (!counts[0] ? "self" : "branch")
            << ", no more collision(s) possible" << std::endl;
      return false;
    }

    return true;
  }

  auto preSAT (const YEvent &lhs, const Item &rhs) {
    if (lhs.flag == rhs.flag) return SKIP;
    return intraPlantCollisionTest(lhs.organ, rhs.organ);
  }
};

struct IntracollisionFunctor {
  const Branch &self, branch;
  const Rect inter;

  uint counts [2];

  IntracollisionFunctor (const Branch &s, const Branch &b)
    : self(s), branch(b), inter(intersection(s.bounds, b.bounds)), counts{0} {}

  bool prepare (XEvents &xevents) {
    // Collect organs at least touching the plant-plant intersection
    for (const Organ *o: branch.organs) {
      if (intersects(inter, o->globalCoordinates().boundingRect)) {
        xevents.emplace(IN, o, 1);
        xevents.emplace(OUT, o, 1);
        counts[0]++;
      }
    }
    if (!counts[0])  return false;

    for (const Organ *o: self.organs) {
      if (intersects(inter, o->globalCoordinates().boundingRect)) {
        xevents.emplace(IN, o, 2);
        xevents.emplace(OUT, o, 2);
        counts[1]++;
      }
    }
    if (!counts[1])  return false;

    return true;  // Good to go
  }

  bool processXEvent (const XEvent &xevent) {
    // Organ visited reduce corresponding count
    if (xevent.type == OUT) counts[xevent.flag-1]--;

    // No collision if all the candidate organs of one plant have been visited
    if (!(counts[0] && counts[1])) {
      if (debugNarrowphase)
        std::cerr << "Seen all organs of " << (!counts[0] ? "self" : "branch")
            << ", no more collision(s) possible" << std::endl;
      return false;
    }

    return true;
  }

  auto preSAT (const YEvent &lhs, const Item &rhs) {
    if (lhs.flag == rhs.flag) return SKIP;
    return intraPlantCollisionTest(lhs.organ, rhs.organ);
  }
};

struct IntercollisionFunctor {
  const Branch &branch;
  const Plant *other;
  const Rect &intersection;

  uint counts [2];

  IntercollisionFunctor (const Branch &b, const Plant *p, const Rect &i)
    : branch(b), other(p), intersection(i), counts{0} {}

  bool prepare (XEvents &xevents) {
    // Collect organs at least touching the plant-plant intersection
    for (const Organ *o: branch.organs) {
      if (intersects(intersection, o->globalCoordinates().boundingRect)) {
        xevents.emplace(IN, o, 1);
        xevents.emplace(OUT, o, 1);
        counts[0]++;
      }
    }
    if (!counts[0])  return false;

    for (const Organ *o: other->organs()) {
      if (intersects(intersection, o->globalCoordinates().boundingRect)) {
        xevents.emplace(IN, o, 2);
        xevents.emplace(OUT, o, 2);
        counts[1]++;
      }
    }
    if (!counts[1])  return false;

    return true;  // Good to go
  }

  bool processXEvent (const XEvent &xevent) {
    // Organ visited reduce corresponding count
    if (xevent.type == OUT) counts[xevent.flag-1]--;

    // No collision if all the candidate organs of one plant have been visited
    if (!(counts[0] && counts[1])) {
      if (debugNarrowphase)
        std::cerr << "Seen all organs of " << (!counts[0] ? "branch" : "other")
            << ", no more collision(s) possible" << std::endl;
      return false;
    }

    return true;
  }

  auto preSAT (const YEvent &lhs, const Item &rhs) {
    if (lhs.flag == rhs.flag) return SKIP;
    return TEST;
  }
};

} // end of namespace narrowphase


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

CollisionResult
TinyPhysicsEngine::collisionTest(const Plant *plant,
                                 const Branch &self, const Branch &branch,
                                 const Organ::Collection &newOrgans) {

  // == Debug ==
  auto indent = utils::make_if<debugCollision,
                               utils::IndentingOStreambuf>(std::cerr);
  // ===========

  CollisionResult res = NO_COLLISION;

  // =============================================================
  // == Testing collision of rule against itself
  if (debugCollision) std::cerr << "Testing for rule validity" << std::endl;

  narrowphase::AutocollisionFunctor autoFunctor (newOrgans);
  if (narrowPhaseCollision(autoFunctor))
    res = CollisionResult::AUTO_COLLISION;

  if (debugCollision) std::cerr << "Rule is valid" << std::endl;

  // =============================================================
  // == Testing collision of rule against rest of the branch
  if (res == NO_COLLISION) {
    if (debugCollision) std::cerr << "Testing for rule collision with branch"
                                  << std::endl;

    narrowphase::BranchcollisionFunctor branchFunctor (newOrgans, branch);
    if (narrowPhaseCollision(branchFunctor))
      res = CollisionResult::BRANCH_COLLISION;

    if (debugCollision) std::cerr << "Rule does not collide with branch"
                                  << std::endl;
  }

  // =============================================================
  // == Testing collision of branch against self
  if (res == NO_COLLISION) {
    if (debugCollision) std::cerr << "Testing for rule collision with plant"
                                  << std::endl;

    narrowphase::IntracollisionFunctor intraFunctor (self, branch);
    if (narrowPhaseCollision(intraFunctor))
      res = CollisionResult::INTRA_COLLISION;

    if (debugCollision) std::cerr << "Branch does not collide with plant"
                                  << std::endl;
  }

  // =============================================================
  // == Testing collision against neighbour plants
  if (res == NO_COLLISION) {
    Rect otherBounds = Rect::invalid();
    if (!broadphase::includes(plant->boundingRect(), branch.bounds))
      otherBounds = branch.bounds;

    CollisionObject *object = *find(plant);
    const_Collisions aabbCandidates;
    broadphaseCollision(object, aabbCandidates, otherBounds);

    if (debugCollision) {
      std::cerr << "Possible collisions for " << plant->id() << " ("
                << object->boundingRect << "): " << aabbCandidates.size() << " [";
      for (const CollisionObject *o: aabbCandidates)
        std::cerr << " " << o->plant->id();
      std::cerr << " ]" << std::endl;
    }

    for (const CollisionObject *that: aabbCandidates) {
      // Ignore collision with seeds
      if (that->plant->isInSeedState()) continue;

      Rect i = intersection(that->boundingRect, branch.bounds);
      if (!i.isValid()) continue;

      narrowphase::IntercollisionFunctor iterFunctor (branch, that->plant, i);
      if(narrowPhaseCollision(iterFunctor))
        res = CollisionResult::INTER_COLLISION;
    }

    if (debugCollision) std::cerr << "Branch does not collide with other plants"
                                  << std::endl;
  }

#ifdef DEBUG_COLLISIONS
  if (res != NO_COLLISION) {
    auto &v = collisionsDebugData[plant];
    for (const Organ *o: branch.organs) {
      BranchCollisionObject bco;
      bco.res = res;

      const auto &coords = o->globalCoordinates();
      bco.pos = coords.origin;
      bco.rot = o->inPlantCoordinates().rotation;
      bco.symb = o->symbol();
      bco.w = o->width();
      bco.l = o->length();

      v.push_back(bco);
    }
  }
#endif

  return res;
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
