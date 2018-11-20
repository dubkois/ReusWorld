#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "../genotype/plant.h"

namespace simu {

struct Point {
  float x, y;
};

struct Organ {
  Point start, end;     // Plant coordinates
  float width, length;  // Constant for now
  float rotation;       // Plant coordinate
  char symbol;          // To choose color and shape
  uint index;           // Index in the string phenotype
};

class Plant {
  using Genome = genotype::Plant;

  Genome _genome;
  Point _pos;

  using Organs = std::vector<Organ>;
  Organs _organs;

  using OrgansView = std::set<Organ*>;
  OrgansView _nonTerminals, _leaves, _hairs;

  uint _derived;  ///< Number of times derivation rules were applied
  std::string _shoot, _root;

public:
  Plant(const Genome &g, float x, float y);

  const Point& pos (void) const {
    return _pos;
  }

  const auto& organs (void) const {
    return _organs;
  }

  void deriveRules (void);

  friend std::ostream& operator<< (std::ostream &os, const Plant &p) {
    return os << "P{\n"
              << "\tshoot: " << p._shoot << "\n"
              << "\t root: " << p._root << "\n}";
  }
};

} // end of namespace simu

#endif // SIMU_PLANT_H
