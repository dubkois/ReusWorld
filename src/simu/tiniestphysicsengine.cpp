#include <stack>

#include "tiniestphysicsengine.h"

namespace simu {
namespace physics {

static constexpr bool debugUpperLayer = false;
static constexpr bool debugCollision = false;

// =============================================================================

bool operator< (const Rect &lhs, const Rect &rhs) {
  if (lhs.ul.x != rhs.ul.x) return lhs.ul.x < rhs.ul.x;
  if (lhs.br.x != rhs.br.x) return lhs.br.x < rhs.br.x;
  if (lhs.ul.y != rhs.ul.y) return lhs.ul.y < rhs.ul.y;
  return lhs.br.y < rhs.br.y;
}

using fnl = std::numeric_limits<float>;
UpperLayer::Item::Item (void)
  : l(fnl::max()), r(-fnl::max()), y(-fnl::max()), organ(nullptr) {}

enum EventType { IN, OUT };
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
    if (lhs.coord() != rhs.coord()) return lhs.coord() < rhs.coord();
    if (lhs.top() != rhs.top()) return lhs.top() < rhs.top();
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

std::ostream& operator<< (std::ostream &os, const UpperLayer::Item &i) {
  return os << "{ " << i.l << " - " << i.r << ", " << i.y << ", "
            << i.organ->id() << " }";
}

void UpperLayer::update (const Plant *p) {
  items.clear();
  std::set<XEvent> xevents;
  for (Organ *o: p->organs()) {
    if (o->length() <= 0) continue; // Skip non terminals

    Rect r = p->organBoundingRect(o);
    if (r.t() < 0)  continue; // Skip below surface

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
//      std::cerr << "\t[" << items.size() << "] ";

    if (current.r - current.l > 1e-3) { // Ignore segments smaller than 1mm
      items.push_back(current);

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

          replace(e.right(), p->organBoundingRect(newTop).t(), newTop);
        }
      }
      break;
    }
  }

  assert(organs.empty());

  if (debugUpperLayer)
    std::cerr << "Extracted " << items.size() << " upper layer items from "
              << p->organs().size() << " organs" << std::endl;
}

// =============================================================================

void CollisionData::reset(void) {
  _data.clear();
}

const UpperLayer::Items& CollisionData::canopy(const Plant *p) const {
  return _data.find(p)->layer.items;
}

void CollisionData::addCollisionData (Plant *p) {
  _data.insert(physics::CObject(p));
}

void CollisionData::removeCollisionData (Plant *p) {
  _data.erase(p);

  for (auto it = _spores.begin(); it != _spores.end();) {
    if (it->plant == p)
      it = _spores.erase(it);
    else  ++it;
  }
}

void CollisionData::updateCollisions (Plant *p) {
  auto it = _data.find(p);
  physics::CObject object = *it;
  _data.erase(it);
  object.updateCollisions();
  _data.insert(object);
}

void CollisionData::updateFinal (Plant *p) {
  auto it = _data.find(p);
  physics::CObject object = *it;
  _data.erase(it);
  object.updateFinal();
  _data.insert(object);
}


// =============================================================================

Disk boundingDiskFor (const Plant *p, const Organ *o) {
  const auto &gc = p->organGlobalCoordinates(o);
  Point c = .5f * (gc.start + gc.end);
  return { c, c.y * 5.f };
}

struct SphereXIntersection {
  Spore obj;

  explicit SphereXIntersection (const Spore &o) : obj(o) {}
  explicit SphereXIntersection(Plant *p, Organ *f)
    : SphereXIntersection(Spore(p,f, boundingDiskFor(p, f))) {}

  static bool intersection (const Disk &lhs, const Disk &rhs) {
    double dXSquared = lhs.center.x - rhs.center.x; // calc. delta X
    dXSquared *= dXSquared; // square delta X

    double dYSquared = lhs.center.y - lhs.center.y; // calc. delta Y
    dYSquared *= dYSquared; // square delta Y

    // Calculate the sum of the radii, then square it
    double sumRadiiSquared = lhs.radius + rhs.radius;
    sumRadiiSquared *= sumRadiiSquared;

    return dXSquared + dYSquared <= sumRadiiSquared;
  }

  static bool leftOf (const Disk &lhs, const Disk &rhs) {
    return lhs.center.x + lhs.radius < rhs.center.x - rhs.radius;
  }

  friend bool operator< (const SphereXIntersection &lhs, const Spore &rhs) {
    return leftOf(lhs.obj.boundingDisk, rhs.boundingDisk);
  }

  friend bool operator< (const Spore &lhs, const SphereXIntersection &rhs) {
    return leftOf(lhs.boundingDisk, rhs.obj.boundingDisk);
  }
};

bool operator< (const Spore &lhs, const Spore &rhs) {
  return lhs.boundingDisk.center.x < rhs.boundingDisk.center.x;
}


void CollisionData::addSpore(Plant *p, Organ *f) {
  assert(p->sex() == Plant::Sex::FEMALE);
  _spores.emplace(p, f, boundingDiskFor(p, f));
}

void CollisionData::delSpore(Plant *p, Organ *f) {
  assert(p->sex() == Plant::Sex::FEMALE);
  delSpore(Spore(p, f, boundingDiskFor(p, f)));
}

void CollisionData::delSpore(const Spore &s) {
  _spores.erase(s);
}

CollisionData::Spores_range CollisionData::sporesInRange(Plant *p, Organ *f) {
  assert(p->sex() == Plant::Sex::MALE);
  return _spores.equal_range(SphereXIntersection(p,f));
}

// =============================================================================

bool CollisionData::narrowPhaseCollision (const Plant *lhs, const Plant *rhs) {
  return true;
}


// =============================================================================

bool operator< (const CObject &lhs, const CObject &rhs) {
  return lhs.boundingRect < rhs.boundingRect;
}

// =============================================================================

struct RectXIntersection {
  const CObject &obj;

  explicit RectXIntersection (const CObject &o) : obj(o) {}

  static bool leftOf (const Rect &lhs, const Rect &rhs) {
    return lhs.br.x < rhs.ul.x;
  }

  friend bool operator< (const RectXIntersection &lhs, const CObject &rhs) {
    bool b = leftOf(lhs.obj.boundingRect, rhs.boundingRect);

//    if (debugCollision)
//      std::cerr << "\t" << lhs.obj.boundingRect << " < " << rhs.boundingRect
//                << " ? " << std::boolalpha << b << std::endl;

    return b;
  }

  friend bool operator< (const CObject &lhs, const RectXIntersection &rhs) {
    bool b = leftOf(lhs.boundingRect, rhs.obj.boundingRect);

//    if (debugCollision)
//      std::cerr << "\t" << lhs.boundingRect << " < " << rhs.obj.boundingRect
//                << " ? " << std::boolalpha << b << std::endl;

    return b;
  }
};

bool CollisionData::isCollisionFree (const Plant *p) const {
  auto pit = _data.find(p);
  if (pit == _data.end())
    utils::doThrow<std::logic_error>(
      "Could not find collision data for plant ", p->id());

  const CObject &c = *pit;
  assert(p == c.plant);

  if (debugCollision) std::cerr << "Collision data " << c << std::endl;

  auto itP = _data.equal_range(RectXIntersection(c));
  assert(itP.first != _data.end()); // p, at least, is not less than p

  if (debugCollision)
    std::cerr << "Possible collisions for " << p->id() << ": "
              << std::distance(itP.first, itP.second) - 1 << " (excluding self)\n";

  for (auto it = itP.first; it != itP.second; ++it) {
    const Plant *p_ = it->plant;
    if (p_ == p) continue;

//    if (debugCollision) std::cerr << "\t" << p_->id() << "\n";

    if (narrowPhaseCollision(p, p_))
      return false;
  }

  if (debugCollision) std::cerr << std::endl;

  return true;
}

// =============================================================================

} // end of namespace physics
} // end of namespace simu
