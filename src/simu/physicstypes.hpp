#ifndef PHYSICS_TYPES_HPP
#define PHYSICS_TYPES_HPP

#include <vector>
#include "types.h"

namespace genotype {
struct Plant;
} // end of namespace genotype

namespace simu {
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

  void update (const Plant *p);
};


struct Spore {
  Plant *const plant;
  Organ *const flower;

  Disk boundingDisk;

  Spore (void) : Spore(nullptr, nullptr, Disk()) {}

  Spore (Plant *p, Organ *f, const Disk &d)
    : plant(p), flower(f), boundingDisk(d) {}

  bool isValid (void) const {
    return plant && flower && boundingDisk.radius > 0;
  }
};

} // end of namespace physics
} // end of namespace simu

#endif // PHYSICS_TYPES_HPP
