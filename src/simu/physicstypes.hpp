#ifndef PHYSICS_TYPES_HPP
#define PHYSICS_TYPES_HPP

#include <vector>

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

} // end of namespace physics
} // end of namespace simu

#endif // PHYSICS_TYPES_HPP
