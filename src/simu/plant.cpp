#include "../config/simuconfig.h"

#include "plant.h"
#include "environment.h"

using genotype::LSystemType;

namespace simu {

using GConfig = genotype::Plant::config_t;
using SConfig = config::Simulation;

using Rule_base = genotype::grammar::Rule_base;

static const auto A = GConfig::ls_rotationAngle();
static const auto L = GConfig::ls_segmentLength();
static const auto W = GConfig::ls_segmentWidth();

bool nonNullAngle (float a) {
  return fabs(a) > 1e-6;
}

#ifndef NDEBUG
#define MAYBE_ID uint id,
#define MAYBE_SET_ID , _id(id)
#else
#define MAYBE_ID
#define MAYBE_SET_ID
#endif

Organ::Organ (MAYBE_ID float w, float l, float r, char c, Layer t, Organ *p)
  : _localRotation(r), _global{}, _width(w), _length(l), _symbol(c), _layer(t),
    _parent(nullptr) MAYBE_SET_ID {
  updateParent(p);
}

#undef MAYBE_ID
#undef MAYBE_SET_ID

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
  _surface = 2 * _length;
  if (!_parent) _surface += _width;
  if (_children.empty())  _surface += _width;

  _biomass = _width * _length;

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

void Organ::updateRotation(float d_angle) {
  _localRotation += d_angle;
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
  : _genome(g), _pos{x, y}, _age(0), _derived(0), _killed(false) {

  auto A = genotype::grammar::toSuccessor(GConfig::ls_axiom());
  for (Layer t: {Layer::SHOOT, Layer::ROOT}) {
    Organs newOrgans;
    float a = initialAngle(t);
    turtleParse(nullptr, A, a, t, newOrgans, _genome.checkers(t));
  }

  updateGeometry();

#ifndef NDEBUG
  _nextOrganID = 0;
#endif
}

Plant::~Plant (void) {
  for (Organ *o: _organs)
    delete o;
}

float Plant::age (void) const {
  return _age / float(SConfig::stepsPerDay());
}

uint Plant::deriveRules(Environment &env) {
  auto nonTerminals = _nonTerminals;
  uint derivations = 0;
  for (Organ *o: nonTerminals) {
    LSystemType ltype = o->layer();
    auto succ = _genome.successor(ltype, o->symbol());

    bool enoughResources = true;  /// TODO Add resources test
    if (!enoughResources) continue;


//    std::cerr << "Applying " << o->symbol() << " -> " << succ << std::endl;

    auto size_before = _organs.size();

    // Create new organs
    Organs newOrgans;
    float angle = o->localRotation(); // will return the remaining rotation (e.g. A -> A++)
    Organ *o_ = turtleParse(o->parent(), succ, angle, ltype, newOrgans,
                            _genome.checkers(ltype));

    // Update physical data
    updateSubtree(o, o_, angle);
    updateGeometry();
    env.updateCollisionData(this);

    bool colliding = !env.isCollisionFree(this);
    if (colliding) {
      /// TODO rollback

      // Delete new organs at the end (eg. A[l], with A -> As[l])
      auto &o_children = o_->children();
      for (auto it = o_children.begin(); it != o_children.end();) {
        auto newOrgans_it = newOrgans.find(*it);
        if (newOrgans_it != newOrgans.end()) {
          delOrgan(*it);
          newOrgans.erase(newOrgans_it);
          it = o_children.erase(it);

        } else
          ++it;
      }

      // Revert reparenting
      updateSubtree(o_, o, -angle);

      // Delete newly created plant portion
      Organ *o__ = o_;
      while (o__ != o->parent()) {
        Organ *tmp = o__;
        o__ = o__->parent();
        delOrgan(tmp);
      }

      // Re-update physical data
      updateGeometry();
      env.updateCollisionData(this);

//      std::cerr << "\tCanceled due to collision with another plant\n"
//                << std::endl;

      auto size_after = _organs.size();
      assert(size_before == size_after);

    } else {
      derivations++;

      // Destroy
      delOrgan(o);
    }
  }

  updateInternals();
  env.updateCollisionDataFinal(this);
//  std::cerr << std::endl;

  return derivations;
}

void Plant::updateSubtree(Organ *oldParent, Organ *newParent, float angle_delta) {
  auto &children = oldParent->children();
  while (!children.empty()) {
    Organ *c = *children.begin();
    c->updateParent(newParent);

    if (nonNullAngle(angle_delta)) c->updateRotation(angle_delta);

    if (!newParent)
          _bases.insert(c);
    else  _bases.erase(c);
  }
}

void Plant::updateGeometry(void) {
  // update bounding rect
  _boundingRect = (*_organs.begin())->boundingRect();
  for (Organ *o: _organs)
    _boundingRect.uniteWith(o->boundingRect());
}

Organ* Plant::turtleParse (Organ *parent, const std::string &successor,
                           float &angle, Layer type, Organs &newOrgans,
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
                    a, type, newOrgans, checkers);
        break;
      }
    } else if (checkers.nonTerminal(c) || checkers.terminal(c)) {
      Organ *o = addOrgan(parent, angle, c, type);
      newOrgans.insert(o);
      parent = o;
      angle = 0;

    } else
      utils::doThrow<std::logic_error>(
            "Invalid character at position ", i, " in ", successor);
  }

  return parent;
}

Organ* Plant::addOrgan(Organ *parent, float angle, char symbol, Layer type) {

  float width = W, length = L;
  if (Rule_base::isValidNonTerminal(symbol))
    width = 0, length = 0;

#ifndef NDEBUG
#define MAYBE_ID _nextOrganID++,
#else
#define MAYBE_ID
#endif
  Organ *o = new Organ(MAYBE_ID width, length, angle, symbol, type, parent);
  _organs.insert(o); 
#undef MAYBE_ID


//  std::cerr << "Created " << *o;
//  if (parent) std::cerr << " under " << *parent;
//  std::cerr << std::endl;

  if (o->isNonTerminal()) _nonTerminals.insert(o);
  if (o->isLeaf())  _leaves.insert(o);
  if (o->isHair())  _hairs.insert(o);
  if (!o->parent()) _bases.insert(o);

  return o;
}

void Plant::delOrgan(Organ *o) {
  while (!o->children().empty())
    delOrgan(*o->children().begin());

//  std::cerr << "Destroying " << *o;
//  if (o->parent())  std::cerr << " under " << *o->parent();
//  std::cerr << std::endl;

  o->removeFromParent();
  _organs.erase(o);
  if (o->isNonTerminal()) _nonTerminals.erase(o);
  if (!o->parent()) _bases.erase(o);
  if (o->isLeaf())  _leaves.erase(o);
  if (o->isHair())  _hairs.erase(o);
  delete o;
}

void Plant::metabolicStep(Environment &env) {
  using genotype::Element;

  // Collect water
  static const auto &k = SConfig::assimilationRate();
  static const auto &J = SConfig::saturationRate();
  for (Organ *h: _hairs) {
    float w = env.waterAt(h->globalCoordinates());
    float dw = w * k * h->surface()
        / (1 + concentration(Layer::ROOT, Element::WATER) * J);
    _reserves[Layer::ROOT][Element::WATER] += dw;
  }
}

void Plant::updateInternals(void) {
  _biomasses.fill(0);
  for (Organ *o: _organs)
    _biomasses[o->layer()] += o->biomass();
}

void recurPrint (Organ *o, uint depth = 0) {
  std::cerr << std::string(depth, '\t') << *o << "\n";
  for (Organ *c: o->children())
    recurPrint(c, depth+1);
}

void Plant::step (Environment &env) {
  std::cerr << "## Plant " << id() << ", " << _age << " days old ##" << std::endl;

  if (_age % SConfig::stepsPerDay() == 0) {
    bool derived = bool(deriveRules(env));
    _derived += derived;

    // Remove non terminals when reaching maximal derivation for a given lsystem
    for (LSystemType t: {LSystemType::SHOOT, LSystemType::ROOT})
      if (_genome.maxDerivations(t) < _derived)
        for (Organ *o: _organs)
          if (o->layer() == t && o->isNonTerminal())
            _nonTerminals.erase(o);
  }

  _age++;

//  std::cerr << "State at end:\n";
//  std::cerr << *this;
//  std::cerr << std::endl;
}

float toPrimaryAngle (float a) {
  static const float PI_2 = 2. * M_PI;
  while (a <= -M_PI)  a += PI_2;
  while (M_PI < a)    a -= PI_2;
  return a;
}

void toString (const Organ *o, std::string &str) {
  double r = o->localRotation();
  if (!o->parent()) r += -Plant::initialAngle(o->layer());
  r = toPrimaryAngle(r);

  if (nonNullAngle(r)) {
    char c = r > 0 ? '+' : '-';
    uint n = std::round(fabs(r) / GConfig::ls_rotationAngle());
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

std::string Plant::toString(Layer type) const {
  uint n = 0;
  for (Organ *o: _bases)  n += (o->layer() == type);

  std::string str;
  for (Organ *o: _bases) {
    if (o->layer() != type)
      continue;

    if (n > 1)  str += "[";
    simu::toString(o, str);
    if (n > 1)  str += "]";
  }
  return str;
}

template <typename LS>
std::string control (const LS &ls, std::string axiom, uint derivations) {
  derivations = std::min(derivations, ls.recursivity+1);
  std::vector<std::string> phenotypes (derivations+1);
  phenotypes[0] = axiom;
  for (uint i=0; i<derivations; i++) {
    axiom = ls.stringPhenotype(axiom);
    phenotypes[i+1] = axiom;
  }

  std::ostringstream oss;
  oss << phenotypes.back();
  for (auto it = std::next(phenotypes.rbegin()); it != phenotypes.rend(); ++it)
    oss << " << " << *it;
  return oss.str();
}

std::ostream& operator<< (std::ostream &os, const Plant &p) {
  using LSType = Plant::Layer;
  static const auto A = genotype::grammar::toSuccessor(GConfig::ls_axiom());
  return os << "P{\n"
            << "\t  shoot: " << p.toString(LSType::SHOOT) << "\n"
            << "\tcontrol: " << control(p._genome.shoot, A, p._derived) << "\n"
            << "\t   root: " << p.toString(LSType::ROOT) << "\n"
            << "\tcontrol: " << control(p._genome.root, A, p._derived) << "\n}";
}


} // end of namespace simu
