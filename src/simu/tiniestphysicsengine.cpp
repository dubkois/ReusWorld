#include <stack>

#include "tiniestphysicsengine.h"

namespace simu {
namespace physics {

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

  XEvent (EventType t, Organ *o) : type(t), organ(o) {}

  auto left (void) const {  return organ->boundingRect().l(); }
  auto right (void) const { return organ->boundingRect().r(); }
  auto top (void) const {   return organ->boundingRect().t(); }
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
              << ", bounds = " << e.organ->boundingRect();
  }
};

struct OrganStackCMP {
  bool operator() (const Organ *lhs, const Organ *rhs) {
    if (lhs->boundingRect().t() != rhs->boundingRect().t())
      return lhs->boundingRect().t() < rhs->boundingRect().t();
    return lhs < rhs;
  }
};

template <typename T, typename CMP>
struct SortedStack {
  std::set<T, CMP> stack;

  void push (Organ *o) {
    stack.insert(o);
  }

  void erase (Organ *o) {
    stack.erase(o);
  }

  T top (void) const {
    return *stack.rbegin();
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
  const Point &pos = p->pos();

  items.clear();
  std::set<XEvent> xevents;
  for (Organ *o: p->organs()) {
    if (o->length() <= 0) continue; // Skip non terminals
    if (o->boundingRect().t() < 0)  continue; // Skip below surface
    xevents.emplace(IN, o);
    xevents.emplace(OUT, o);
  }

  SortedStack<Organ*, OrganStackCMP> organs;
  UpperLayer::Item current;
  auto replace = [&pos, &current] (auto l, auto y, auto o) {
    current = Item();
    current.l = l + pos.x;
    current.y = y + pos.y;
    current.organ = o;
//    std::cerr << "\tcurrent item: " << current << std::endl;
  };
  auto emplace = [&pos, &current, this] (auto r) {
    current.r = r + pos.x;
//    std::cerr << "\t[" << items.size() << "] ";
    if (current.r - current.l > 1e-3) /*{*/ // Ignore segments smaller than 1mm
      items.push_back(current);
//      std::cerr << "Pushed current item: " << current << std::endl;
//    } else
//      std::cerr << "Ignored null-sized item: " << current << std::endl;
  };

  for (XEvent e: xevents) {
//    std::cerr << e << std::endl;
    switch (e.type) {
    case IN:
      organs.push(e.organ);
      if (current.y < e.top()) {
        if (current.organ)  emplace(e.left());
        replace(e.left(), e.top(), e.organ);
      }
      break;

    case OUT:
      assert(!organs.empty());
      Organ *top = organs.top();
      organs.erase(e.organ);
//      std::cerr << "\t\tCurrent top: " << top->id()
//                << ", bounds: " << top->boundingRect() << std::endl;
      if (top == e.organ) {
        emplace(e.right());

        if (!organs.empty()) {
          Organ *newTop = organs.top();
//          std::cerr << "\t\tNew top: " << newTop->id()
//                    << ", bounds: " << newTop->boundingRect() << std::endl;
          replace(e.right(), newTop->boundingRect().t(), newTop);
        }
      }
      break;
    }
  }

  assert(organs.empty());
  std::cerr << "Extracted " << items.size() << " upper layer items from "
            << p->organs().size() << " organs" << std::endl;
}

// =============================================================================

void CollisionData::reset(void) {
  _data.clear();
}

void CollisionData::addCollisionData (Plant *p) {
  _data.insert(physics::CObject(p));
}

void CollisionData::removeCollisionData (Plant *p) {
  _data.erase(p);
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
//    std::cerr << "\t" << lhs.obj.boundingRect << " < " << rhs.boundingRect
//              << " ? " << std::boolalpha << b << std::endl;
    return b;
  }

  friend bool operator< (const CObject &lhs, const RectXIntersection &rhs) {
    bool b = leftOf(lhs.boundingRect, rhs.obj.boundingRect);
//    std::cerr << "\t" << lhs.boundingRect << " < " << rhs.obj.boundingRect
//              << " ? " << std::boolalpha << b << std::endl;
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
  std::cerr << "Collision data " << c << std::endl;
  auto itP = _data.equal_range(RectXIntersection(c));
  assert(itP.first != _data.end()); // p, at least, is not less than p

  std::cerr << "Possible collisions for " << p->id() << ": "
            << std::distance(itP.first, itP.second) - 1 << " (excluding self)\n";
  for (auto it = itP.first; it != itP.second; ++it) {
    const Plant *p_ = it->plant;
    if (p_ == p) continue;

    std::cerr << "\t" << p_->id() << "\n";

    if (narrowPhaseCollision(p, p_))
      return false;
  }
  std::cerr << std::endl;

  return true;
}

// =============================================================================

} // end of namespace physics
} // end of namespace simu
