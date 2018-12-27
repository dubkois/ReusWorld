#include <stack>

#include "tiniestphysicsengine.h"

namespace simu {
namespace physics {

static constexpr bool debugUpperLayer = false;
static constexpr bool debugCollision = false;
static constexpr bool debugReproduction = false;

static constexpr float floatPrecision = 1e6;
bool fuzzyEqual (float lhs, float rhs) {
  return int(lhs * floatPrecision) == int(rhs * floatPrecision);
}
bool fuzzyLower (float lhs, float rhs) {
  return int(lhs * floatPrecision) < int(rhs * floatPrecision);
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


void UpperLayer::update (const Plant *p) {
  items.clear();
  std::set<XEvent> xevents;
  for (Organ *o: p->organs()) {
    if (o->length() <= 0) continue; // Skip non terminals

    const Rect &r = o->globalCoordinates().boundingRect;
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

          replace(e.right(), newTop->globalCoordinates().boundingRect.t(), newTop);
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

CollisionData::Collisions::iterator CollisionData::find (const Plant *p) {
  auto it = _data.find(*p);
  if (it == _data.end())
    utils::doThrow<std::logic_error>("No collision data for plant ", p->id());
  return it;
}

CollisionData::Collisions::const_iterator CollisionData::find (const Plant *p) const {
  auto it = _data.find(*p);
  if (it == _data.end())
    utils::doThrow<std::logic_error>("No collision data for plant ", p->id());
  return it;
}

const UpperLayer::Items& CollisionData::canopy(const Plant *p) const {
  return find(p)->layer.items;
}

bool CollisionData::addCollisionData (Plant *p) {
  CObject object (p);
  if (debugCollision) std::cerr << "Inserted collision data "
                                << object.boundingRect << " for " << p->id()
                                << std::endl;
  return _data.insert(object).second;
}

void CollisionData::removeCollisionData (Plant *p) {
  auto it = find(p);
  _data.erase(it);

  for (auto it = _pistils.begin(); it != _pistils.end();) {
    if (it->organ->plant() == p)
      it = _pistils.erase(it);
    else  ++it;
  }
}

void CollisionData::updateCollisions (Plant *p) {
  auto it = find(p);
  CObject object = *it;
  _data.erase(it);
  if (debugCollision) std::cerr << "Updated collision data for " << p->id()
                                << " from " << object.boundingRect;
  object.updateCollisions();
  if (debugCollision) std::cerr << " to " << object.boundingRect << std::endl;
  _data.insert(object);
}

void CollisionData::updateFinal (Plant *p) {
  auto it = find(p);
  CObject object = *it;
  _data.erase(it);
  object.updateFinal();
  _data.insert(object);

#ifndef NDEBUG
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
#endif
}


// =============================================================================

bool CollisionData::narrowPhaseCollision (const Plant */*lhs*/, const Plant */*rhs*/) {
#warning No narrow phase collision
  return true;
}


// =============================================================================

bool operator< (const CObject &lhs, const CObject &rhs) {
  return fuzzyLower(lhs.plant->pos().x, rhs.plant->pos().x);
}

bool operator< (const CObject &lhs, const Plant &rhs) {
  return fuzzyLower(lhs.plant->pos().x, rhs.pos().x);
}

bool operator< (const Plant &lhs, const CObject &rhs) {
  return fuzzyLower(lhs.pos().x, rhs.plant->pos().x);
}

// =============================================================================

bool intersects (const CObject &lhs, const CObject &rhs) {
  return intersects(lhs.boundingRect, rhs.boundingRect);
}

bool CollisionData::isCollisionFree (const Plant *p) const {
  auto vit = find(p);

  const auto &cv = *vit;

  auto itLR = typename Collisions::reverse_iterator(vit);
  while (itLR != _data.rend() && intersects(*itLR, cv))
    itLR = std::next(itLR);

  auto itR = vit;
  while (itR != _data.end() && intersects(*itR, cv))
    itR = std::next(itR);

  auto range = std::make_pair(
    itLR.base(), // should be std::prev(itLR.base()) but we want the next
    itR
  );

  if (debugCollision)
    std::cerr << "Possible collisions for " << p->id() << " ("
              << cv.boundingRect << "): "
              << range.first->plant->id() << " to "
              << std::prev(range.second)->plant->id()
              << " (" << std::distance(range.first, range.second) - 1
              << ", excluding self)" << std::endl;

  bool this_isSeed = p->isInSeedState();
  for (auto it = range.first; it != range.second; ++it) {
    const Plant *p_ = it->plant;
    if (p_ == p) continue;

    if (debugCollision)
      std::cerr << "\t" << p_->id() << ": " << it->boundingRect << "\n";

    if (narrowPhaseCollision(p, p_) // Test for collision
        && (this_isSeed || !p_->isInSeedState())) // Ignore plants hitting seeds
      return false;
  }

  if (debugCollision) std::cerr << std::endl;

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
  Point c = o->globalCoordinates().center;
  return { c, (c.y + 1) * 5.f };
}

void CollisionData::addPistil(Organ *p) {
#ifdef NDEBUG
  _pistils.emplace(p, boundingDiskFor(p));
#else
  auto res = _pistils.emplace(p, boundingDiskFor(p));
  assert(p->globalCoordinates().center == boundingDiskFor(p).center);
  if (debug) {
    if (res.second)
      std::cerr << "Added " << *res.first << std::endl;
    else
      std::cerr << "Unable to insert pistil " << Pistil(p, boundingDiskFor(p))
                << " (equal to " << *res.first << ")" << std::endl;
  }
#endif
}

void CollisionData::delPistil(const Point &pos) {
  auto it = _pistils.find(pos);
  if (it != _pistils.end()) {
    if (debugReproduction)  std::cerr << "Deleting " << *it << std::endl;
    _pistils.erase(it);
  } else
    if (debugReproduction)  std::cerr << "Could not delete pistil at pos"
                                      << pos << ": not found" << std::endl;
}

void CollisionData::delPistil(const Pistil &s) {
  delPistil(s.boundingDisk.center);
}

void CollisionData::updatePistil (Organ *p, const Point &oldPos) {
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
CollisionData::Pistils_range CollisionData::sporesInRange(Organ *s) {
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
