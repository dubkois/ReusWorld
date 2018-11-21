#include "plant.h"

namespace simu {

using GConfig = genotype::Plant::config_t;

static const auto A = GConfig::ls_rotationAngle();
static const auto L = GConfig::ls_segmentLength();
static const auto W = GConfig::ls_segmentWidth();

Plant::Plant(const Genome &g, float x, float y)
  : _genome(g), _pos{x, y}, _derived(0) {

  // shoot
  Organ *sseed = new Organ { {0,0}, {0, L}, W, L, M_PI/2., g.shoot.axiom };
  _organs.insert(Organ_ptr(sseed));
  _nonTerminals.insert(sseed);
  _shoot += sseed->symbol;
  _ntpos[sseed] = 0;

  // root
  Organ *rseed = new Organ { {0,0}, {0, -L}, W, L, -M_PI/2., g.root.axiom };
  _organs.insert(Organ_ptr(rseed));
  _nonTerminals.insert(rseed);
  _root += rseed->symbol;
  _ntpos[rseed] = 0;

  deriveRules();
}

void Plant::deriveRules(void) {
  using genotype::grammar::LSystemType;

  uint derivations = 0;
  for (auto it = _nonTerminals.begin(); it != _nonTerminals.end();) {
    Organ* o = *it;
    bool shoot = o->end.y > 0;
    LSystemType ltype = shoot? LSystemType::SHOOT : LSystemType::ROOT;
    auto succ = _genome.successor(ltype, o->symbol);

    bool valid = true; /// TODO Validity check (resources, collision)
    if (valid) {
      // Update physical phenotype
      turtleParse(succ, o->start, o->rotation, _genome.checkers(ltype));
      derivations++;

      // Update string phenotype
      std::string &phenotype = shoot ? _shoot : _root;
      uint index = _ntpos.at(*it);
      phenotype.replace(index, 1, succ);

      // Update indices map
      _ntpos.erase(*it);
      for (auto &p: _ntpos) if (p.second > index) p.second += succ.size();

      // Destroy
      it = _nonTerminals.erase(it);
      _organs.erase(_organs.find(o));

    } else
      ++it;
  }
}

void Plant::turtleParse (const std::string &successor,
                         Point start, float angle,
                         const genotype::grammar::Checkers &checkers) {

  for (size_t i=0; i<successor.size(); i++) {
    char c = successor[i];
    if (checkers.control(c)) {
      switch (c) {
      case ']':
        utils::doThrow<std::logic_error>(
              "Dandling closing bracket a position ", i, " in ", successor);
        break;
      case '+': angle += A; break;
      case '-': angle -= A; break;
      case '[':
        turtleParse(genotype::grammar::extractBranch(successor, i, i), start, angle, checkers);
        break;
      }
    } else if (checkers.nonTerminal(c) || checkers.terminal(c)) {
      Point end {
        float(start.x + L * cos(angle)),
        float(start.y + L * sin(angle))
      };
      Organ *o = new Organ { start, end, W, L, angle, c };
      _organs.insert(Organ_ptr(o));

      if (checkers.nonTerminal(c)) {
        _nonTerminals.insert(o);
        _ntpos[o] = i;
      }

    } else
      utils::doThrow<std::logic_error>(
            "Invalid character at position ", i, " in ", successor);
  }
}


} // end of namespace simu
