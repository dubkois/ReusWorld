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
  _baseBiomass = -1;
  _accumulatedBiomass = 0;
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

void Organ::accumulate (float biomass) {
  if (isStructural()) {
    std::cerr << "[O" << _id << _symbol << "] old width: " << _width
              << ", adding " << biomass;
    _baseBiomass += biomass;
    _width = _baseBiomass / _length;
    std::cerr << ", new width: " << _baseBiomass << " / " << _length
              << " = " << _width << std::endl;
    updateDimensions(false);

  } else
    _accumulatedBiomass += biomass;

  _requiredBiomass -= biomass;
}

void Organ::updateDimensions(bool andTransformations) {
  _surface = 2 * _length;
  if (_children.empty())  _surface += _width;

  _baseBiomass = _width * _length;

  if (andTransformations)
    updateTransformation();
}

void Organ::updateTransformation(void) {
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

  for (Organ *c: _children) c->updateTransformation();
}

void Organ::updateParent (Organ *newParent) {
  if (_parent != newParent) {
    removeFromParent();
    if (newParent)  newParent->_children.insert(this);
    _parent = newParent;

    updateTransformation();
  }

  _depth = 0;
  for (Organ *c: _children) {
    c->updateParent(c->_parent);
    _depth = std::max(_depth, c->_depth);
  }
  _depth++;
}

void Organ::updateRotation(float d_angle) {
  _localRotation += d_angle;
  updateTransformation();
}

#ifndef NDEBUG
std::ostream& operator<< (std::ostream &os, const Organ &o) {
  return os << std::setprecision(2) << std::fixed
            << "{ " << o._global.start << "->" << o._global.end << ", "
            << o._global.rotation << ", " << o._symbol << "}";
}
#endif


Plant::Plant(const Genome &g, float x, float y, float biomass)
  : _genome(g), _pos{x, y}, _age(0), _derived(0), _killed(false) {

#ifndef NDEBUG
  _nextOrganID = 0;
#endif

  auto _r = _reserves[Layer::SHOOT];
  _r.fill(0);
  _reserves.fill(_r);

  auto A = genotype::grammar::toSuccessor(GConfig::ls_axiom());
  for (Layer t: {Layer::SHOOT, Layer::ROOT}) {
    Organs newOrgans;
    float a = initialAngle(t);
    turtleParse(nullptr, A, a, t, newOrgans, _genome.checkers(t));
  }

  updateGeometry();
  updateInternals();

  // Distribute initial resources to producer modules
  assert(isSeed());
  float totalBR = 0;
  for (Organ *o: _organs) totalBR += o->requiredBiomass();
  for (Organ *o: _organs)
    o->accumulate(biomass * o->requiredBiomass() / totalBR);
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

    // Did the apex accumulate enough resources ?
    if (o->requiredBiomass() > 0) continue;

    std::cerr << "Applying " << o->symbol() << " -> " << succ << std::endl;

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

  if (derivations > 0)  updateInternals();
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

  if (!o->parent()) _bases.insert(o);
  if (o->isNonTerminal()) _nonTerminals.insert(o);
  if (o->isHair())  _hairs.insert(o);
  if (o->isFlower())  _flowers.insert(o);

  if (isSink(o))  _sinks.insert(o);

  o->updateDimensions(true);
  o->setRequiredBiomass(isSink(o) ? sinkRequirement(o) : 0);

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
  if (!o->parent()) _bases.erase(o);
  if (o->isNonTerminal()) _nonTerminals.erase(o);
  if (o->isHair())  _hairs.erase(o);
  if (isSink(o))  _sinks.erase(o);
  if (o->isFlower())  _flowers.erase(o);
  delete o;
}

bool Plant::isSink(Organ *o) const {
  if (o->isNonTerminal()) return true;
  if (o->isStructural())  return true;
  if (o->isFlower())  return _fruits.find(o) != _fruits.end();
  return false;
}

float Plant::sinkRequirement (Organ *o) const {
  assert(isSink(o));

  float required;
  if (o->isNonTerminal()) {
    required = ruleBiomassCost(o->layer(), o->symbol());

  } else if (o->isFlower()) {
    required = initialBiomassFor(_fruits.at(o)) * _genome.seedsPerFruit;

  } else if (o->isStructural()) {
//    std::cerr << "[O" << o->id() << o->symbol() << "] requests a width of "
//              << W << " * (1 + " << _genome.metabolism.deltaWidth << ")^"
//              << std::max(0, int(o->depth()) -1)
//              << ") = " << W * std::pow(1 + _genome.metabolism.deltaWidth,
//                                    std::max(0, int(o->depth()) -1))
//              << std::endl;
    required = L * W * std::pow(1 + _genome.metabolism.deltaWidth, std::max(0, int(o->depth()) -1));
  }

  required = std::max(0.f, required - o->biomass());
//  std::cerr << "[O" << o->id() << o->symbol() << "] requires "
//            << required << std::endl;
  return required;
//  return std::max(0.f, required - o->biomass());
}

float Plant::ruleBiomassCost (const Genome &g, Layer l, char symbol) {
  const std::string &succ = g.successor(l, symbol);
  const auto &checkers = g.checkers(l);
  float cost = 0;
  for (char c: succ)
    if (checkers.terminal(c))
      cost += W * L;
  return cost;
}

void Plant::biomassRequirements (Masses &wastes, Masses &growth) {
  static const auto &k_lit = SConfig::lifeCost();
  for (Layer l: EnumUtils<Layer>::iterator())
    wastes[l] = k_lit * _biomasses[l];

  growth.fill(0);
  for (Organ *o: _sinks)
    growth[o->layer()] += o->requiredBiomass();
}

void Plant::metabolicStep(Environment &env) {
  using genotype::Element;
  using L_EU = EnumUtils<Layer>;

  static const auto &k_E = SConfig::assimilationRate();
  static const auto &J_E = SConfig::saturationRate();
  static const auto &f_E = SConfig::resourceCost();

  // Collect water
  decimal U_w = 0;
  decimal uw_k = (1 + concentration(Layer::ROOT, Element::WATER) * J_E); assert(uw_k >= 1);
  for (Organ *h: _hairs) {
    decimal w = env.waterAt(h->globalCoordinates());
    U_w += w * k_E * h->surface() / uw_k;
    std::cerr << "[O" << h->id() << h->symbol() << "] drawing "
              << U_w << " = " << w << " * " << k_E << " * " << h->surface()
              << " / " << uw_k << std::endl;
  }
  utils::iclip_max(U_w, _biomasses[Layer::ROOT] - _reserves[Layer::ROOT][Element::WATER]);
  _reserves[Layer::ROOT][Element::WATER] += U_w;
  assert(_reserves[Layer::ROOT][Element::WATER] <= _biomasses[Layer::ROOT]);


  // Transport water
  decimal T_w = (
      concentration(Layer::ROOT, Element::WATER)
    - concentration(Layer::SHOOT, Element::WATER)
  ) / (
      (_genome.metabolism.resistors[Element::WATER] / _biomasses[Layer::ROOT])
    + (_genome.metabolism.resistors[Element::WATER] / _biomasses[Layer::SHOOT])
  );
  _reserves[Layer::ROOT][Element::WATER] -= T_w;
  _reserves[Layer::SHOOT][Element::WATER] += T_w;
  std::cerr << "[P" << id() << "] transfering " << T_w << " from ROOT to SHOOT"
            << std::endl;


  // Produce glucose
  decimal U_g = 0;
  decimal ug_k = 1 + concentration(Layer::SHOOT, Element::GLUCOSE) * J_E; assert(ug_k >= 1);
  for (const physics::UpperLayer::Item &i: env.canopy(this)) {
    if (!i.organ->isLeaf()) continue;

    static constexpr decimal light = 1;
    U_g += light * k_E * (i.r - i.l) / ug_k;
    std::cerr << "[O" << i.organ->id() << i.organ->symbol() << "] photosynthizing "
              << U_g << " = " << light << " * " << k_E << " * (" << i.r << " - "
              << i.l << ") / " << ug_k << std::endl;
  }
  utils::iclip_max(U_g, _reserves[Layer::SHOOT][Element::WATER]);
  utils::iclip_max(U_g, _biomasses[Layer::SHOOT] - _reserves[Layer::SHOOT][Element::GLUCOSE]);
  _reserves[Layer::SHOOT][Element::WATER] -= U_g;
  _reserves[Layer::SHOOT][Element::GLUCOSE] += U_g;
  assert(_reserves[Layer::SHOOT][Element::GLUCOSE] <= _biomasses[Layer::SHOOT]);


  // Transport glucose
  decimal T_g = (
      concentration(Layer::SHOOT, Element::GLUCOSE)
    - concentration(Layer::ROOT, Element::GLUCOSE)
  ) / (
      (_genome.metabolism.resistors[Element::GLUCOSE] / _biomasses[Layer::SHOOT])
    + (_genome.metabolism.resistors[Element::GLUCOSE] / _biomasses[Layer::ROOT])
  );
  _reserves[Layer::SHOOT][Element::GLUCOSE] -= T_g;
  _reserves[Layer::ROOT][Element::GLUCOSE] += T_g;
  std::cerr << "[P" << id() << "] transfering " << T_g << " from SHOOT to ROOT"
            << std::endl;


  // Transform resources into biomass
  std::cerr << "Transforming resources into biomass" << std::endl;
  Masses X, W, G;
  biomassRequirements(W, G);
  for (Layer l: L_EU::iterator()) {
    X[l] = _genome.metabolism.growthSpeed * _biomasses[l]
         * concentration(l, Element::GLUCOSE)
         * concentration(l, Element::WATER);

    std::cerr << "X[" << L_EU::getName(l) << "] = " << X[l] << " = "
              << _genome.metabolism.growthSpeed << " * " << _biomasses[l]
              << " * " << concentration(l, Element::GLUCOSE)
              << " * " << concentration(l, Element::WATER);

    utils::iclip_max(X[l], W[l] + G[l]);

    std::cerr << ", clipped to " << X[l] << std::endl;

    for (Element e: EnumUtils<Element>::iterator())
      _reserves[l][e] -= f_E * X[l];
  }


  // Transform biomass into wastes
  std::cerr << "Wastes:";
  for (decimal f: W) std::cerr << " " << f;
  std::cerr << std::endl;

  for (Layer l: L_EU::iterator()) X[l] -= W[l];

  // Distribute to consumers (non terminals, structurals, fruits)
  std::cerr << "Available biomass:";
  for (Layer l: L_EU::iterator())
    std::cerr << " " << X[l];
  std::cerr << std::endl;

  std::cerr << "Requirements:";
  for (decimal f: G) std::cerr << " " << f;
  std::cerr << std::endl;
  for (Organ *s: _sinks) {
    decimal r = s->requiredBiomass();
    assert(r >= 0);
    if (r <= 0) continue;
    decimal b = X[s->layer()] * r / G[s->layer()];
    std::cerr << "[O" << s->id() << s->symbol() << "] b = "
              << "X[" << s->layer() << "] * " << r << " / G[" << s->layer() << "] = "
              << X[s->layer()] << " * " << r << " / " << G[s->layer()] << " = "
              << X[s->layer()] * r / G[s->layer()] << std::endl;
    s->accumulate(std::min(r, b));
  }

  std::cerr << "[P" << id() << "] state at end:\n\t"
            << _organs.size() << " organs"
            << "\n\tbiomasses:";
  for (decimal b: _biomasses) std::cerr << " " << b;
  std::cerr << "\n\treserves:";
  for (auto &rs: _reserves) {
    std::cerr << " [";
    for (decimal r: rs) std::cerr << " " << r;
    std::cerr << " ]";
  }
  std::cerr << std::endl;

  if (SConfig::logIndividualStats()) {
    std::ostringstream oss;
    oss << "p" << id() << ".dat";

    std::ofstream ofs;
    std::ios_base::openmode mode = std::fstream::out;
    if (_age > 0) mode |= std::fstream::app;
    else          mode |= std::fstream::trunc;
    ofs.open(oss.str(), mode);

    if (_age == 0)
      ofs << R"("Organs" "Apexes" "M_{sh}" "M_{rt}" "M_{shW}" "M_{shG}" )"
             R"("M_{rtW}" "M_{rtG}" "U_W" "T_W" "U_G" "T_G" "W_{sh}" "W_{rt}" )"
             R"("X_{sh}" "X_{rt}")" << "\n";
    ofs << _organs.size() << " " << _nonTerminals.size() << " "
        << _biomasses[Layer::SHOOT] << " " << _biomasses[Layer::ROOT] << " "
        << _reserves[Layer::SHOOT][Element::WATER] << " "
        << _reserves[Layer::SHOOT][Element::GLUCOSE] << " "
        << _reserves[Layer::ROOT][Element::WATER] << " "
        << _reserves[Layer::ROOT][Element::GLUCOSE] << " "
        << U_w << " " << T_w << " " << U_g << " " << T_g << " "
        << W[Layer::SHOOT] << " " << W[Layer::ROOT] << " "
        << X[Layer::SHOOT] << " " << X[Layer::ROOT] << std::endl;
  }
}

void Plant::updateInternals(void) {
  _biomasses.fill(0);
  for (Organ *o: _organs)
    _biomasses[o->layer()] += o->biomass();
}

float Plant::initialBiomassFor (const Genome &g) {
  return ruleBiomassCost(g, Layer::SHOOT, GConfig::ls_axiom())
       + ruleBiomassCost(g, Layer::ROOT, GConfig::ls_axiom());
}

void Plant::attemptReproduction(Environment &env) {
  std::cerr << "No reproduction (yet)" << std::endl;
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

  metabolicStep(env);

  attemptReproduction(env);

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
