#include "../config/simuconfig.h"

#include "plant.h"

using genotype::grammar::LSystemType;

namespace simu {

using GConfig = genotype::Plant::config_t;
using SConfig = config::Simulation;

using Rule_base = genotype::grammar::Rule_base;

static const auto A = GConfig::ls_rotationAngle();
static const auto L = GConfig::ls_segmentLength();
static const auto W = GConfig::ls_segmentWidth();

Organ::Organ (float w, float l, float r, char c, LSType t, Organ *p)
  : _localRotation(r), _global{}, _width(w), _length(l), _symbol(c), _type(t),
    _parent(nullptr) {
  updateParent(p);
}

void Organ::removeFromParent(void) {
  if (_parent)  _parent->_children.erase(this);
}

Point rotate_point(float angle, Point p) {
  float s = std::sin(angle);
  float c = std::cos(angle);
  p.x = p.x * c - p.y * s;
  p.y = p.x * s + p.y * c;
  return p;
}

void Organ::updateCache(void) {
  _global.rotation = _localRotation;
  if (_parent)  _global.rotation += _parent->_global.rotation;

  _global.start = _parent ? _parent->_global.end : Point{0,0};

  _global.end = _global.start + Point::fromPolar(_global.rotation, _length);

  Point v = rotate_point(_global.rotation, {0, .5f * width()});

  Point corners [] {
    _global.start + v, _global.start - v, _global.end + v, _global.end - v
  };

  _boundingRect = Rect::invalid();
  for (Point p: corners) {
    _boundingRect.ul.x = std::min(_boundingRect.ul.x, p.x);
    _boundingRect.ul.y = std::max(_boundingRect.ul.y, p.y);
    _boundingRect.br.x = std::max(_boundingRect.br.x, p.x);
    _boundingRect.br.y = std::min(_boundingRect.br.y, p.y);
  }

  for (Organ *c: _children) c->updateCache();
}

void Organ::updateParent (Organ *newParent) {
  removeFromParent();
  if (newParent)  newParent->_children.insert(this);
  _parent = newParent;
  updateCache();
}

#ifndef NDEBUG
std::ostream& operator<< (std::ostream &os, const Organ &o) {
  return os << std::setprecision(2) << std::fixed
            << "{ " << o._global.start << "->" << o._global.end << ", "
            << o._global.rotation << ", " << o._symbol << "}";
}
#endif


Plant::Plant(const Genome &g, float x, float y)
  : _genome(g), _pos{x, y}, _derived(0) {

  auto A = genotype::grammar::toSuccessor(GConfig::ls_axiom());
  for (LSType t: {LSType::SHOOT, LSType::ROOT}) {
    float a = initialAngle(t);
    turtleParse(nullptr, A, a, t, _genome.checkers(t));
  }
//    addOrgan(nullptr, W, L, 0, GConfig::ls_axiom(), t);


//  deriveRules();
}

Plant::~Plant (void) {
  for (Organ *o: _organs)
    delete o;
}

float Plant::age (void) const {
  return _age / float(SConfig::stepsPerDay());
}

uint Plant::deriveRules(void) {
  auto nonTerminals = _nonTerminals;
  uint derivations = 0;
  for (auto it = nonTerminals.begin(); it != nonTerminals.end();) {
    Organ* o = *it;
    LSystemType ltype = o->type();
    auto succ = _genome.successor(ltype, o->symbol());

    /// TODO Validity check (resources, collision)
    bool valid = true;
    if (valid) {
      std::cerr << "Applying " << o->symbol() << " -> " << succ << std::endl;

      // Update physical phenotype
      float angle = o->localRotation();
      Organ *o_ = turtleParse(o->parent(), succ, angle, ltype,
                              _genome.checkers(ltype));

      if (o_ == o->parent()) { // Nothing append (except maybe some rotations)
//        assert(false);
      }

      // Update connections
      for (Organ *c: o->children())
        c->updateParent(o_);

      derivations++;

      // Destroy
      it = nonTerminals.erase(it);
      delOrgan(o);

    } else
      ++it;
  }

  std::cerr << std::endl;
  if (derivations > 0) {
    // update bounding rect
    _boundingRect = (*_organs.begin())->boundingRect();
    for (Organ *o: _organs)
      _boundingRect.uniteWith(o->boundingRect());
  }

  return derivations;
}

Organ* Plant::turtleParse (Organ *parent, const std::string &successor,
                           float &angle, LSType type,
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
        float a = angle;
        turtleParse(parent, genotype::grammar::extractBranch(successor, i, i),
                    a, type, checkers);
        break;
      }
    } else if (checkers.nonTerminal(c) || checkers.terminal(c)) {
      parent = addOrgan(parent, W, L, angle, c, type);
      angle = 0;

    } else
      utils::doThrow<std::logic_error>(
            "Invalid character at position ", i, " in ", successor);
  }

  return parent;
}

Organ* Plant::addOrgan(Organ *parent, float width, float length, float angle,
                       char symbol, LSType type) {

  Organ *o = new Organ(width, length, angle, symbol, type, parent);
  _organs.insert(o);

  std::cerr << "Created " << *o;
  if (parent) std::cerr << " under " << *parent;
  std::cerr << std::endl;

  if (o->isNonTerminal()) {
    _nonTerminals.insert(o);
//    _ntpos[o] = i;
  }

  if (o->isLeaf())  _leaves.insert(o);
  if (o->isHair())  _hairs.insert(o);
  if (!o->parent()) _bases.insert(o);

  return o;
}

void Plant::delOrgan(Organ *o) {
  o->removeFromParent();
  _organs.erase(o);
  if (o->isNonTerminal()) _nonTerminals.erase(o);
  if (!o->parent()) _bases.erase(o);
  if (o->isLeaf())  _leaves.erase(o);
  if (o->isHair())  _hairs.erase(o);
  delete o;
}

void recurPrint (Organ *o, uint depth = 0) {
  std::cerr << std::string(depth, '\t') << *o << "\n";
  for (Organ *c: o->children())
    recurPrint(c, depth+1);
}

void Plant::step (void) {
  if (_age % SConfig::stepsPerDay() == 0) {
    bool derived = bool(deriveRules());
    _derived += derived;

    // Remove non terminals when reaching maximal derivation for a given lsystem
    for (LSystemType t: {LSystemType::SHOOT, LSystemType::ROOT})
      if (_genome.maxDerivations(t) <= _derived)
        for (Organ *o: _organs)
          if (o->type() == t && o->isNonTerminal())
            _nonTerminals.erase(o);
  }

  _age++;

  std::cerr << "State at end:\n";
  for (Organ *o: _bases)
    recurPrint(o);
  std::cerr << std::endl;
}

void toString (const Organ *o, std::string &str) {

  double r = o->localRotation();
  if (!o->parent()) r += -utils::sgn(r) * M_PI / 2.;

  if (fabs(r) > 1e-6) {
    char c = r > 0 ? '+' : '-';
    uint n = fabs(r) / GConfig::ls_rotationAngle();
    str += std::string(n, c);
  }

  bool children = o->children().size() > 1;
  str += o->symbol();
  for (Organ *c: o->children()) {
    if (children) str += "[";
    toString(c, str);
    if (children) str += "]";
  }
}

std::string Plant::toString(LSType type) const {
  std::string str;
  for (Organ *o: _bases) {
    if (o->type() != type)
      continue;

    simu::toString(o, str);
  }
  return str;
}

} // end of namespace simu
