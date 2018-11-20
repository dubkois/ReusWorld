#include "plant.h"

namespace simu {

using LSConfig = genotype::Plant::config_t;

Plant::Plant(const Genome &g, float x, float y)
  : _genome(g), _pos{x, y}, _derived(0) {

  const float l = LSConfig::ls_segmentLength();

  // shoot
  _organs.push_back({
    {0,0}, {0, l}, LSConfig::ls_segmentWidth(), l, M_PI/2.,
    g.shoot.axiom, 0
  });
  _nonTerminals.insert(&_organs[0]);
  _shoot += _organs[0].symbol;

  // root
  _organs.push_back({
    {0,0}, {0, -l}, LSConfig::ls_segmentWidth(), l, -M_PI/2.,
    g.root.axiom, 0
  });
  _nonTerminals.insert(&_organs[1]);
  _root += _organs[1].symbol;

  deriveRules();
}

void Plant::deriveRules(void) {
  uint derivations = 0;
  for (auto it = _nonTerminals.begin(); it != _nonTerminals.end();) {
    Organ &o = **it;

    auto succ = o.end.y > 0 ?
          _genome.shoot.successor(o.symbol)
        : _genome.root.successor(o.symbol);

    bool valid = true; /// TODO Validity check (resources, collision)
    if (valid) {
      it = _nonTerminals.erase(it);
      derivations++;

    } else
      ++it;
  }
}

} // end of namespace simu
