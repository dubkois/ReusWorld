#include <stack>

#include "tiniestphysicsengine.h"
#include "environment.h"

namespace simu {
namespace physics {

enum EventType { IN, OUT };

static constexpr bool debugUpperLayer = false;
static constexpr bool debugBroadphase = true;
static constexpr bool debugNarrowphase = true;
static constexpr bool debugCollision = false || debugBroadphase || debugNarrowphase;
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
  return os << "{ " << i.l << " - " << i.r << ", " << i.y << ", "
            << i.organ->id() << " }";
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
  Rect boundingRect;

  XEvent (EventType t, Organ *o, const Rect r)
    : type(t), organ(o), boundingRect(r) {}

  auto left (void) const {  return boundingRect.l(); }
  auto right (void) const { return boundingRect.r(); }
  auto top (void) const {   return boundingRect.t(); }
  auto coord (void) const { return type == IN? left() : right(); }

  friend bool operator< (const XEvent &lhs, const XEvent &rhs) {
    if (!fuzzyEqual(lhs.coord(), rhs.coord()))  return fuzzyLower(lhs.coord(), rhs.coord());
    if (!fuzzyEqual(lhs.top(), rhs.top()))      return fuzzyLower(lhs.top(), rhs.top());
    return lhs.organ < rhs.organ;
  }

  friend std::ostream& operator<< (std::ostream &os, const XEvent &e) {
    return os << (e.type == IN ? "I" : "O")
              << "Event(" << e.organ->symbol() << "): "
              << "x = " << e.coord() << ", id = " << e.organ->id()
              << ", bounds = " << e.boundingRect;
  }
};

template <typename T>
struct SortedStack {
  struct Item {
    T value;
    float y;

    friend bool operator< (const Item &lhs, const Item &rhs) {
      if (lhs.y != rhs.y) return lhs.y < rhs.y;
      return lhs.value < rhs.value;
    }
  };
  std::set<Item> stack;

  void push (const T &v, float y) {
    stack.insert({v,y});
  }

  void erase (const T &v, float y) {
    stack.erase({v,y});
  }

  T top (void) const {
    return stack.rbegin()->value;
  }

  bool empty (void) const {
    return stack.empty();
  }
};

} // end of namespace canopy

/// FIXME Inter-shadowing should not be ignored !
void UpperLayer::update (const simu::Environment &env, const Plant *p,
                         const const_Collisions &collisions) {
  using namespace canopy;
  itemsInIsolation.clear();
  std::set<XEvent> xevents;
  for (Organ *o: p->organs()) {
    if (o->length() <= 0) continue; // Skip non terminals

    const Rect &r = o->globalCoordinates().boundingRect;
    float h = env.heightAt(r.center().x);
    if (r.t() < h)  continue; // Skip below surface

    xevents.emplace(IN, o, r);
    xevents.emplace(OUT, o, r);
  }

  SortedStack<Organ*> organs;
  UpperLayer::Item current;
  auto replace = [&current] (auto l, auto y, auto o) {
    current = Item();
    current.l = l;
    current.y = y;
    current.organ = o;

//    if (debugUpperLayer)
//      std::cerr << "\tcurrent item: " << current << std::endl;
  };
  auto emplace = [&current, this] (auto r) {
    current.r = r;

//    if (debugUpperLayer)
//      std::cerr << "\t[" << itemsInIsolation.size() << "] ";

    if (current.r - current.l > 1e-3) { // Ignore segments smaller than 1mm
      itemsInIsolation.push_back(current);

//      if (debugUpperLayer)
//        std::cerr << "Pushed current item: " << current << std::endl;

    } else {
//      if (debugUpperLayer)
//        std::cerr << "Ignored null-sized item: " << current << std::endl;
    }
  };

  for (XEvent e: xevents) {
//    std::cerr << e << std::endl;
    switch (e.type) {
    case IN:
      organs.push(e.organ, e.top());
      if (current.y < e.top()) {
        if (current.organ)  emplace(e.left());
        replace(e.left(), e.top(), e.organ);
      }
      break;

    case OUT:
      assert(!organs.empty());
      Organ *top = organs.top();
      organs.erase(e.organ, e.top());

//      if (debugUpperLayer)
//        std::cerr << "\t\tCurrent top: " << top->id()
//                  << ", bounds: " << top->boundingRect() << std::endl;

      if (top == e.organ) {
        emplace(e.right());

        if (!organs.empty()) {
          Organ *newTop = organs.top();

//          if (debugUpperLayer)
//            std::cerr << "\t\tNew top: " << newTop->id()
//                      << ", bounds: " << newTop->boundingRect() << std::endl;

          replace(e.right(), newTop->globalCoordinates().boundingRect.t(), newTop);
        }
      }
      break;
    }
  }

  assert(organs.empty());

  if (debugUpperLayer)
    std::cerr << "Extracted " << itemsInIsolation.size()
              << " upper layer items from " << p->organs().size() << " organs"
              << std::endl;

  // Now that the individual's canopy is computed, test it against its neighbors
  itemsInWorld = itemsInIsolation;
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
    return lhs.x < rhs.x;
  }
};

bool includes (const CollisionObject *lhs, const CollisionObject *rhs) {
  return lhs->boundingRect.l() <= rhs->boundingRect.l()
      && rhs->boundingRect.r() <= lhs->boundingRect.r();
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

  std::set<CollisionObject*> inside;
  for (const XEvent &e: xevents) {
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
#if 1
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

  // Delete edges
  _leftEdges.erase(object->leftEdge.get());
  _rightEdges.erase(object->rightEdge.get());

  // Delete references in (potentially) englobed/englobing objects
  for (CollisionObject *other: object->englobedObjects)
    broadphase::noLongerEnglobes(object, other);
  for (CollisionObject *other: object->englobingObjects)
    broadphase::noLongerEnglobes(other, object);

  // Also delete any remaining pistils
  /// FIXME This stinks to high heavens: O(n) with no reason...
  for (auto itP = _pistils.begin(); itP != _pistils.end();) {
    if (itP->organ->plant() == p)
      itP = _pistils.erase(itP);
    else  ++itP;
  }

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

  if (object->leftEdge->edge != object->boundingRect.l()) {
    _leftEdges.erase(object->leftEdge.get());
    object->leftEdge->edge = object->boundingRect.l();
    _leftEdges.insert(object->leftEdge.get());
  }

  if (object->rightEdge->edge != object->boundingRect.r()) {
    _rightEdges.erase(object->rightEdge.get());
    object->rightEdge->edge = object->boundingRect.r();
    _rightEdges.insert(object->rightEdge.get());
  }
}

void TinyPhysicsEngine::updateFinal (const Environment &env, Plant *p) {
  auto it = find(p);
  CollisionObject *object = *it;
  _data.erase(it);

  // Collect potentially colliding canopies
  const_Collisions collisions;
  broadphaseCollision(object, collisions);

  object->updateFinal(env, collisions); // Update bounding box and canopy
  it = _data.insert(object).first;

  const auto aabb = object->boundingRect;

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
  for (auto itL = std::next(std::reverse_iterator(it));
       itL!=_data.rend() && aabb.l() <= (*itL)->plant->pos().x; ++itL)
    if (broadphase::includes(object, *itL))
      broadphase::englobes(object, *itL);

  for (auto itR = std::next(it);
       itR != _data.end() && (*itR)->plant->pos().x <= aabb.r(); ++itR)
    if (broadphase::includes(object, *itR))
      broadphase::englobes(object, *itR);
}


// =============================================================================

namespace narrowphase {

enum EventDir { X, Y };

template <EventDir D>
struct Event {
  EventType type;
  Organ const * const organ;

  Event (EventType t, const Organ *o) : type(t), organ(o) {}

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
    std::cerr << "\tSearching separating axis between "
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

} // end of namespace narrowphase

/// \returns true if a collision occured, false otherwise
bool TinyPhysicsEngine::narrowPhaseCollision (const Plant *lhs, const Plant *rhs,
                                              const Rect &intersection) {

  using namespace narrowphase;

  if (debugNarrowphase)
    std::cerr << "Narrowphase collision of " << intersection
              << " between\n\t" << PlantID(lhs) << ":";

  // Collect organs in each plant at least touching the plant-plant intersection
  uint lhsO = 0, rhsO = 0;
  std::set<Event<X>> xevents;
  for (const Organ *o: lhs->organs()) {
    if (intersects(intersection, o->globalCoordinates().boundingRect)) {
      if (debugNarrowphase) std::cerr << " " << OrganID(o, true);
      xevents.emplace(IN, o);
      xevents.emplace(OUT, o);
      lhsO++;
    }
  }

  if (debugNarrowphase)
    std::cerr << "\n\t" << PlantID(rhs) << ":";

  for (const Organ *o: rhs->organs()) {
    if (intersects(intersection, o->globalCoordinates().boundingRect)) {
      if (debugNarrowphase) std::cerr << " " << OrganID(o, true);
      xevents.emplace(IN, o);
      xevents.emplace(OUT, o);
      rhsO++;
    }
  }

  if (debugNarrowphase) std::cerr << std::endl;

  // No collision if one of the plant has no organ in the intersection
  if (!(lhsO && rhsO)) {
    if (debugNarrowphase)
      std::cerr << "\tObject " << (!lhsO ? "lhs" : "rhs") << " has no candidate"
                << " organ(s), no collision(s) possible" << std::endl;
    return false;
  }

  // Linesweep to find lhs-rhs organ collision pair
  std::set<const Organ*> inside;
  std::set<Event<Y>> yevents;
  for (const Event<X> &xevent: xevents) {
    std::initializer_list<Event<Y>> new_yevents {
      Event<Y> (IN, xevent.organ), Event<Y> (OUT, xevent.organ)
    };

    if (debugNarrowphase) std::cerr << "\t" << xevent << std::endl;
    if (xevent.type == IN)
          yevents.insert(new_yevents);

    else {
      for (auto &e: new_yevents)  yevents.erase(e);

      // Organ visited reduce corresponding count
      if (xevent.organ->plant() == lhs)
            lhsO--;
      else  rhsO--;
    }

    // No collision if all the candidate organs of one plant have been visited
    if (!(lhsO && rhsO)) {
      if (debugNarrowphase)
        std::cerr << "\tSeen all organs of " << (!lhsO ? "lhs" : "rhs") << ", no"
                  << " more collision(s) possible" << std::endl;
      return false;
    }

    for (const Event<Y> &yevent: yevents) {
      if (debugNarrowphase) std::cerr << "\t\t" << yevent << std::endl;
      if (yevent.type == IN) {
        const Organ *lhs = yevent.organ;
        for (const Organ *rhs: inside)
          if (lhs->plant() != rhs->plant()
              && collisionSAT(lhs, rhs))
            return true;

        inside.insert(yevent.organ);

      } else
        inside.erase(yevent.organ);
    }
  }

  return false;
}


// =============================================================================

bool intersects (const CollisionObject *lhs, const CollisionObject *rhs) {
  return intersects(lhs->boundingRect, rhs->boundingRect);
}

void TinyPhysicsEngine::broadphaseCollision(const CollisionObject *object,
                                            const_Collisions &objects) const {

  { // Start from right, scanning for edges untils reaching left
    if (debugBroadphase)  std::cerr << "Scanning right to left:";
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
    if (debugBroadphase)  std::cerr << "Scanning left to right:";
    auto it = std::next(_leftEdges.find(object->leftEdge.get()));
    while (it != _leftEdges.end() && (*it)->edge < object->rightEdge->edge) {
      if (debugBroadphase)  std::cerr << " " << (*it)->object->plant->id();
      objects.insert((*it)->object);
      it = std::next(it);
    }
    if (debugBroadphase)  std::cerr << std::endl;
  }

  if (debugBroadphase) {
    std::cerr << "Englobing objects:";
    for (const CollisionObject *eng: object->englobingObjects)
      std::cerr << " " << eng->plant->id();
    std::cerr << std::endl;
  }

  // Include (potentially) englobing objects
  objects.insert(object->englobingObjects.begin(),
                        object->englobingObjects.end());

  objects.erase(object); // No self collision
}

/// FIXME Still not working at 100% (Newly inserted plants have no englobing information)
bool TinyPhysicsEngine::isCollisionFree (const Plant *p) const {
  if (debugCollision) std::cerr << "\nTesting collisions for " << p->id() << std::endl;

  CollisionObject *object = *find(p);
  const_Collisions aabbCandidates;
  broadphaseCollision(object, aabbCandidates);

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

    Rect intersection;
    intersection.ul.x = std::max(that->boundingRect.l(), object->boundingRect.l());
    intersection.ul.y = std::min(that->boundingRect.t(), object->boundingRect.t());
    intersection.br.x = std::min(that->boundingRect.r(), object->boundingRect.r());
    intersection.br.y = std::max(that->boundingRect.b(), object->boundingRect.b());
    if (intersection.isValid()
        && narrowPhaseCollision(object->plant, that->plant, intersection))
      return false;
  }

  return true;
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

void TinyPhysicsEngine::delPistil(Organ *p) {
  auto it = _pistils.find(p->globalCoordinates().center);
  if (it != _pistils.end() && it->organ == p) {
    if (debugReproduction)  std::cerr << "Deleting " << *it << std::endl;

    _pistils.erase(it);
  } else
    if (debugReproduction)  std::cerr << "Could not delete pistil for " << OrganID(p)
                                      << " at " << p->globalCoordinates().center
                                      << ": not found" << std::endl;
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

} // end of namespace physics
} // end of namespace simu
