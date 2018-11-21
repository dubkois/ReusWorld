#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "../genotype/plant.h"

namespace simu {

struct Point {
  float x, y;

#ifndef NDEBUG
  friend std::ostream& operator<< (std::ostream &os, const Point &p) {
    return os << "{" << p.x << "," << p.y << "}";
  }
#endif
};

struct Organ {
  Point start, end;     // Plant coordinates
  float width, length;  // Constant for now
  float rotation;       // Plant coordinate
  char symbol;          // To choose color and shape

  using Ptr = std::unique_ptr<Organ>;

  friend bool operator< (const Ptr &lhs, Organ *rhs) {
    return lhs.get() < rhs;
  }
  friend bool operator< (Organ *lhs, const Ptr &rhs) {
    return lhs < rhs.get();
  }

#ifndef NDEBUG
  friend std::ostream& operator<< (std::ostream &os, const Organ &o) {
    return os << "{ " << o.start << "->" << o.end << ", "
              << o.rotation << ", " << o.symbol << "}";
  }
#endif
};

class Plant {
  using Genome = genotype::Plant;

  Genome _genome;
  Point _pos;

  uint _age;

  using Organ_ptr = Organ::Ptr;
  using Organs = std::set<Organ_ptr, std::less<>>;
  Organs _organs;

  using OrgansView = std::set<Organ*>;
  OrgansView _nonTerminals, _leaves, _hairs;

  uint _derived;  ///< Number of times derivation rules were applied

  // ===========================================
  // == String representation
  using NonTerminalsPos = std::map<Organ*,uint>;
  NonTerminalsPos _ntpos;
  std::string _shoot, _root;
  // ===========================================

public:
  Plant(const Genome &g, float x, float y);

  const Point& pos (void) const {
    return _pos;
  }

  const auto& organs (void) const {
    return _organs;
  }

  float age (void) const;

  void step (void);

  bool isDead (void) const {
    return false;
  }

  void deriveRules (void);

  friend std::ostream& operator<< (std::ostream &os, const Plant &p) {
    return os << "P{\n"
              << "\tshoot: " << p._shoot << "\n"
              << "\t root: " << p._root << "\n}";
  }

private:
  void turtleParse (const std::string &successor, Point start, float angle,
                    const genotype::grammar::Checkers &checkers);
};

} // end of namespace simu

#endif // SIMU_PLANT_H
