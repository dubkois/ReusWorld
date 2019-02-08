#ifndef PHYSICS_TYPES_HPP
#define PHYSICS_TYPES_HPP

#include <vector>
#include "types.h"

namespace genotype {
struct Plant;
} // end of namespace genotype

namespace simu {
struct Environment;
struct Plant;
struct Organ;

namespace physics {

struct UpperLayer {
  struct Item {
    float l, r, y;
    Organ *organ;

    Item (void);
  };

  using Items = std::vector<Item>;
  Items items;

  void update (const Environment &env, const Plant *p);
};


struct Pistil {
  Organ *const organ;

  Disk boundingDisk;

  Pistil (void) : Pistil(nullptr, Disk()) {}

  Pistil (Organ *f, const Disk &d) : organ(f), boundingDisk(d) {}

  bool isValid (void) const {
    return organ && boundingDisk.radius > 0;
  }
};

} // end of namespace physics
} // end of namespace simu

#endif // PHYSICS_TYPES_HPP
