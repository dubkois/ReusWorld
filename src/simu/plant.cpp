#include "../config/simuconfig.h"

#include "plant.h"
#include "environment.h"

using genotype::LSystemType;

namespace simu {

using GConfig = genotype::Plant::config_t;
using SConfig = config::Simulation;

using Rule_base = genotype::grammar::Rule_base;
using L_EU = EnumUtils<Plant::Layer>;
using E_EU = EnumUtils<Plant::Element>;

static const auto A = GConfig::ls_rotationAngle();

static constexpr bool debugInit = false;
static constexpr bool debugOrganManagement = false;
static constexpr bool debugDerivation = false;
static constexpr int debugMetabolism = false;
static constexpr bool debugGrowth = false;
static constexpr bool debugReproduction = false;

static constexpr bool debug = false
  | debugInit   | debugOrganManagement | debugDerivation | debugMetabolism
  | debugGrowth | debugReproduction;

bool nonNullAngle (float a) {
  return fabs(a) > 1e-6;
}

Organ::Organ (OID id, Plant *plant, float w, float l, float r, char c, Layer t,
              Organ *p)
  : _id(id), _plant(plant), _width(w), _length(l),
    _symbol(c), _layer(t), _parent(nullptr) {

  _baseBiomass = -1;
  _accumulatedBiomass = 0;

  _parentCoordinates.rotation = r;
  _plantCoordinates = {};
  _globalCoordinates = {};

  _depth = 0;
  updateParent(p);
}

float Organ::fullness(void) const {
  if (_requiredBiomass == 0)  return 1;
  float f = 1 - _requiredBiomass / (biomass() + _requiredBiomass);
  assert(biomass() > 0);
  assert(_requiredBiomass != 0 || f == 1);
  assert(isSeed() || (0 <= f && f <= 1));
  utils::iclip_max(f, 1.f);
  return f;
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
    if (debugGrowth)
      std::cerr << OrganID(this) << " old width: " << _width
                << ", adding " << biomass;

    _baseBiomass += biomass;
    _width = _baseBiomass / _length;

    if (debugGrowth)
      std::cerr << ", new width: " << _baseBiomass << " / " << _length
                << " = " << _width << " (" << 100 * fullness() << "%)"
                << std::endl;

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
  _plantCoordinates.rotation = _parentCoordinates.rotation;
  if (_parent)  _plantCoordinates.rotation += _parent->_plantCoordinates.rotation;

  _plantCoordinates.origin =
      _parent ? _parent->_plantCoordinates.end : Point{0,0};

  _plantCoordinates.end = _plantCoordinates.origin
      + Point::fromPolar(_plantCoordinates.rotation, _length);

  Point pend = _plantCoordinates.end;
  auto width = _width;
  if (_length == 0 && _width == 0) {
    static const auto S = GConfig::ls_terminalsSizes().at('s').width;
    width = S;
    pend = _plantCoordinates.origin
        + Point::fromPolar(_plantCoordinates.rotation, S);
  }

  Point v = rotate_point(_plantCoordinates.rotation, {0, .5f * width});

  Point corners [] {
    _plantCoordinates.origin + v, pend + v,
    _plantCoordinates.origin - v, pend - v
  };

  Rect &pr = _plantCoordinates.boundingRect;
  pr = Rect::invalid();
  for (Point p: corners) {
    pr.ul.x = std::min(pr.ul.x, p.x);
    pr.ul.y = std::max(pr.ul.y, p.y);
    pr.br.x = std::max(pr.br.x, p.x);
    pr.br.y = std::min(pr.br.y, p.y);
  }

  _plantCoordinates.center = pr.center();


  const Point &pp = _plant->pos();
  _globalCoordinates.origin = _plantCoordinates.origin + pp;
  _globalCoordinates.center = _plantCoordinates.center + pp;
  _globalCoordinates.boundingRect = _plantCoordinates.boundingRect.translated(pp);

  for (Organ *c: _children) c->updateTransformation();
}

void Organ::removeFromParent(void) {
  if (_parent) {
    _parent->_children.erase(this);
    uint depth = 0;
    for (Organ *c: _parent->_children)
      depth = std::max(depth, c->_depth);
    _parent->updateDepth(depth);
  }
}

void Organ::restoreInParent(void) {
  if (_parent) {
    _parent->_children.insert(this);
    uint depth = 0;
    for (Organ *c: _parent->_children)
      depth = std::max(depth, c->_depth);
    _parent->updateDepth(depth);
  }
}

void Organ::updateParentDepth(void) {
  if (_parent)  _parent->updateDepth(_depth + (isNonTerminal() ? 0 : 1));
}

void Organ::updateDepth(uint newDepth) {
  _depth = std::max(_depth, newDepth);

  if (debugGrowth) {
    std::cerr << OrganID(this);
    for (uint d = 0; d<_depth; d++) std::cerr << "  ";
    std::cerr << " Updated depth to " << _depth << std::endl;
  }

  updateParentDepth();
}

void Organ::updateParent (Organ *newParent) {
  assert(newParent != this);
  if (_parent != newParent) {
    removeFromParent();
    if (newParent)  newParent->_children.insert(this);
    _parent = newParent;
    updateParentDepth();

    updateTransformation();
  }
}

void Organ::updateRotation(float d_angle) {
  _parentCoordinates.rotation += d_angle;
  updateTransformation();
}

std::ostream& operator<< (std::ostream &os, const Organ &o) {
  std::stringstream tmpos;
  tmpos << std::setprecision(2) << std::fixed
     << "[" << OrganID(&o) << "]{ " << o._plantCoordinates.origin << "->"
     << o._plantCoordinates.end << ", "
     << o._plantCoordinates.rotation << "}";
  return os << tmpos.rdbuf();
}


Plant::Plant(const Genome &g, float x, float y)
  : _genome(g), _pos{x, y}, _age(0), _derived(0), _killed(false) {

  _nextOrganID = OID(0);

  _biomasses.fill(0);

  auto _r = _reserves[Layer::SHOOT];
  _r.fill(0);
  _reserves.fill(_r);
}

Plant::~Plant (void) {
  for (Organ *o: _organs)
    delete o;
}

void Plant::init (Environment &env, float biomass) {
  auto A = genotype::grammar::toSuccessor(GConfig::ls_axiom());
  for (Layer l: {Layer::SHOOT, Layer::ROOT})
    turtleParse(nullptr, A, initialAngle(l), l, _genome.checkers(l), env);


  // Distribute initial resources to producer modules
  assert(isInSeedState());
  float totalBR = 0;
  for (Organ *o: _organs) totalBR += o->requiredBiomass();
  for (Organ *o: _organs)
    o->accumulate(biomass * o->requiredBiomass() / totalBR);

  updateMetabolicValues();
  updateGeometry();

  if (debugInit)
    std::cerr << PlantID(this) << " Initialized. Genome is " << _genome
              << std::endl;
}

void Plant::replaceWithFruit (Organ *o, const Genome &g, Environment &env) {
  using genotype::grammar::toSuccessor;
  using Rule = genotype::grammar::Rule_base;

  assert(o->isFlower());

  Organ *parent = o->parent();
  float rotation = o->localRotation();
  Layer l = o->layer();
  assert(l == Layer::SHOOT);

  auto p = _fruits.insert({_nextOrganID, {g, nullptr}});
  assert(p.second);

  Organ *fruit = turtleParse(parent, toSuccessor(Rule::fruitSymbol()), rotation,
                             l, _genome.checkers(l), env);

  p.first->second.fruit = fruit;

  _dirty.set(DIRTY_METABOLISM, true);

  // Clean up
  Point fpos = o->globalCoordinates().center;
  delOrgan(o, env);

  // Update other flowers in case a competitor can take the freed-up space
  for (Organ *f: _flowers)
    if (fabs(f->globalCoordinates().center.x - fpos.x) < 1e-3)
      env.updateGeneticMaterial(f, f->globalCoordinates().center);
}

float Plant::age (void) const {
  return _age / float(SConfig::stepsPerDay());
}

bool Plant::spontaneousDeath(void) const {
  if (_genome.dethklok <= age())  return true;
  if (_sinks.empty())  return true;
  for (Layer l: L_EU::iterator())
    for (Element e: E_EU::iterator())
      if (_reserves[l][e] < 0)
          return true;
  return false;
}

void Plant::autopsy(void) const {
  bool spontaneous = spontaneousDeath();
  std::cerr << PlantID(this) << " Deleted."
            << " Death was " << (spontaneous ? "spontaneous" : "forced");
  if (spontaneous) {
    std::cerr << "\n\tReserves:";

    using PL = EnumUtils<Plant::Layer>;
    using PE = EnumUtils<Plant::Element>;
    for (auto l: PL::iterator())
      for (auto e: PE::iterator())
        std::cerr << " {" << PL::getName(l)[0] << PE::getName(e)[0]
                  << ": " << 100 * concentration(l, e) << "%}";

    std::cerr << "\n\tAge: " << age() << " / " << _genome.dethklok
              << "\n\tOrgans: " << _organs.size()
              << " (" << _sinks.size() << " of which are sinks)"
              << std::endl;
  }
}

uint Plant::deriveRules(Environment &env) {
  /// TODO Is this really okay ?
  // Store current pistils location in case of an update
  std::map<Organ*, Point> oldPistilsPositions;
  if (sex() == Sex::FEMALE)
    for (Organ *f: _flowers)
      oldPistilsPositions.emplace(f, f->globalCoordinates().center);

  auto nonTerminals = _nonTerminals;
  uint derivations = 0;
  for (Organ *apex: nonTerminals) {
    Layer layer = apex->layer();
    auto succ = _genome.successor(layer, apex->symbol());

    // Did the apex accumulate enough resources ?
    if (apex->requiredBiomass() > 0) {
      if (debugDerivation)
        std::cerr << OrganID(apex) << " Apex is not ready (" << 100*apex->fullness()
                  << "%)" << std::endl;
      continue;

    } else if (debugDerivation)
      std::cerr << "Applying " << apex->symbol() << " -> " << succ << std::endl;

    auto size_before = _organs.size();

    // Create new organs
    Organs newOrgans;
    float angle = apex->localRotation(); // will return the remaining rotation (e.g. A -> A++)
    Organ *end = turtleParse(apex->parent(), succ, angle, layer, newOrgans,
                             _genome.checkers(layer), env);

    // Update physical data
    apex->removeFromParent();
    updateSubtree(apex, end, angle);
    updateGeometry();
    env.updateCollisionData(this);

    /// FIXME This is a smelly piece of
    float sizeIncrease = 0;
    for (auto c: succ)
      sizeIncrease += GConfig::sizeOf(c).length;

    bool colliding = !env.isCollisionFree(this);

    if (colliding && sizeIncrease > 0) {
      const auto deleter = [&newOrgans, &env, this] (auto collection) {
        for (auto it = collection.begin(); it != collection.end();) {
          auto newOrgans_it = newOrgans.find(*it);
          if (newOrgans_it != newOrgans.end()) {
            delOrgan(*it, env);
            newOrgans.erase(newOrgans_it);
            it = collection.erase(it);

          } else
            ++it;
        }
      };

      // Delete new organs at the end (eg. A[l], with A -> As[l])
      if (end) deleter(end->children());

      // Revert reparenting
      if (end) updateSubtree(end, apex, -angle);
      apex->restoreInParent();

      // Delete newly created plant portion
      Organ *o__ = end;
      while (o__ != apex->parent()) {
        Organ *tmp = o__;
        o__ = o__->parent();
        delOrgan(tmp, env);
      }

      // Delete remaining unparented organs
      if (!apex->parent()) deleter(_bases);

      // Re-update physical data
      updateGeometry();
      env.updateCollisionData(this);

      if (debugDerivation)
        std::cerr << "\tCanceled due to collision with another plant\n"
                  << std::endl;

      auto size_after = _organs.size();
      assert(size_before == size_after);

    } else { // All's good

      // Divide remaining biomass into newly created sinks
      float leftOverBiomass = apex->biomass() - ruleBiomassCost(apex->layer(), apex->symbol());
      if (leftOverBiomass > 0) {
        if (debugMetabolism || debugDerivation)
          std::cerr << PlantID(this) << "Dispatching " << leftOverBiomass
                    << " left over biomass" << std::endl;
        distributeBiomass(leftOverBiomass, newOrgans,
                          [] (Organ*) { return true; });
      }

      derivations++;

      // Destroy
      delOrgan(apex, env);

      // Update pistils positions cache
      if (sex() == Sex::FEMALE)
        for (Organ *p: _flowers)
          if (oldPistilsPositions.find(p) == oldPistilsPositions.end())
            oldPistilsPositions.emplace(p, p->globalCoordinates().center);
    }
  }

  // Update pistils as needed
  for (auto &p: oldPistilsPositions) {
    if (p.second != p.first->globalCoordinates().center) {
      env.updateGeneticMaterial(p.first, p.second); // If position changed

      // Check if this freed up some space for others
      for (Organ *f: _flowers)
        if (f != p.first && fabs(f->globalCoordinates().center.x - p.second.x) < 1e-3)
          env.updateGeneticMaterial(f, f->globalCoordinates().center);
    }
  }

  if (derivations > 0) {
    _dirty.set(DIRTY_METABOLISM);
    _dirty.set(DIRTY_COLLISION);
  }

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
  _boundingRect = (*_organs.begin())->inPlantCoordinates().boundingRect;
  for (Organ *o: _organs)
    _boundingRect.uniteWith(o->inPlantCoordinates().boundingRect);
}

Organ* Plant::turtleParse (Organ *parent, const std::string &successor,
                           float &angle, Layer type, Organs &newOrgans,
                           const genotype::grammar::Checkers &checkers,
                           Environment &env) {

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
                    a, type, newOrgans, checkers, env);
        break;
      }
    } else if (checkers.nonTerminal(c) || checkers.terminal(c)
               || c == genotype::grammar::Rule_base::fruitSymbol()) {
      Organ *o = addOrgan(parent, angle, c, type, env);
      newOrgans.insert(o);
      parent = o;
      angle = 0;

    } else
      utils::doThrow<std::logic_error>(
            "Invalid character at position ", i, " in ", successor);
  }

  return parent;
}

Organ* Plant::addOrgan(Organ *parent, float angle, char symbol, Layer type,
                       Environment &env) {

  auto size = GConfig::sizeOf(symbol);
  Organ *o = new Organ(_nextOrganID, this, size.width, size.length, angle,
                       symbol, type, parent);
  _nextOrganID = OID(uint(_nextOrganID)+1);
  _organs.insert(o); 

  if (debugOrganManagement) {
    std::cerr << "Created " << *o;
    if (parent) std::cerr << " under " << *parent;
    std::cerr << std::endl;
  }

  if (!o->parent()) _bases.insert(o);
  if (o->isNonTerminal()) _nonTerminals.insert(o);
  if (o->isHair())  _hairs.insert(o);
  if (o->isFlower())  _flowers.insert(o);

  if (isSink(o))  _sinks.insert(o);

  o->updateDimensions(true);
  o->setRequiredBiomass(isSink(o) ? sinkRequirement(o) : 0);

  if (o->isFlower() && sex() == Sex::FEMALE)
    env.disseminateGeneticMaterial(o);

  return o;
}

void Plant::delOrgan(Organ *o, Environment &env) {
  while (!o->children().empty())
    delOrgan(*o->children().begin(), env);

  if (debugOrganManagement) {
    std::cerr << "Destroying " << *o;
    if (o->parent())  std::cerr << " under " << *o->parent();
    std::cerr << std::endl;
  }

  o->removeFromParent();
  _organs.erase(o);
  if (!o->parent()) _bases.erase(o);
  if (o->isNonTerminal()) _nonTerminals.erase(o);
  if (o->isHair())  _hairs.erase(o);
  if (isSink(o))  _sinks.erase(o);

  if (o->isFlower()) {
    _flowers.erase(o);
    if (sex() == Sex::FEMALE)
      env.removeGeneticMaterial(o->globalCoordinates().center);
  }

  if (o->isFruit()) _fruits.erase(o->id());

  _dirty.set(DIRTY_COLLISION, true);
  delete o;
}

bool Plant::isSink(Organ *o) const {
  return o->isNonTerminal() || o->isStructural()
      || o->isFlower() || o->isFruit();
}

float Plant::sinkRequirement (Organ *o) const {
  assert(isSink(o));

  float required = 0;
  if (o->isNonTerminal()) {
    required = ruleBiomassCost(o->layer(), o->symbol());

  } else if (o->isFlower()) {
    required = (1 + SConfig::floweringCost()) * o->biomass();

  } else if (o->isFruit()) {
    required = seedBiomassRequirements(_genome, _fruits.at(o->id()).genome)
      * _genome.seedsPerFruit;

  } else if (o->isStructural()) {
    const auto &size = GConfig::sizeOf(o->symbol());
    if (debugGrowth)
      std::cerr << OrganID(o) << " requests a width of "
                << size.width << " * (1 + " << _genome.metabolism.deltaWidth
                << ")^" << std::max(0, int(o->depth()) -1)
                << " = " << size.width * std::pow(1 + _genome.metabolism.deltaWidth,
                                                   std::max(0, int(o->depth()) -1))
                << std::endl;

    required = size.length * size.width
        * std::pow(1 + _genome.metabolism.deltaWidth,
                   std::max(0, int(o->depth()) -1));
  }

  required = std::max(0.f, required - o->biomass());

  if (debugMetabolism)
    std::cerr << OrganID(o) << " Requires "
              << required << std::endl;

  return required;
}

float Plant::ruleBiomassCost (const Genome &g, Layer l, char symbol) {
  const std::string &succ = g.successor(l, symbol);
  float cost = 0;
  for (char c: succ) {
    const auto &size = GConfig::sizeOf(c);
    cost += size.width * size.length;
  }
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

template <typename F>
void Plant::distributeBiomass (decimal  amount, Organs &organs, F match) {
  float totalRequirements = 0;
  for (Organ *o: organs)  if (match(o)) totalRequirements += o->requiredBiomass();
  distributeBiomass(amount, organs, totalRequirements, match);
}

template <typename F>
void Plant::distributeBiomass (decimal amount, Organs &organs, decimal total, F match) {
  for (Organ *o: organs) {
    if (!match(o))  continue;
    decimal r = o->requiredBiomass();
    if (r <= 0) continue;
    decimal b = o->requiredBiomass() * amount / total;

    if (debugMetabolism)
      std::cerr << OrganID(o) << " b = "
                << o->requiredBiomass() << " * " << amount << " / " << total
                << " = " << b << std::endl;

    o->accumulate(std::min(r, b));
  }
}

float Plant::seedBiomassRequirements(const Genome &mother, const Genome &child) {
  return initialBiomassFor(child) * mother.fruitOvershoot;
}

void Plant::metabolicStep(Environment &env) {
  using genotype::Element;

  static const auto &k_E = SConfig::assimilationRate();
  static const auto &J_E = SConfig::saturationRate();
  static const auto &f_E = SConfig::resourceCost();

  if (debugMetabolism > 1) {
    std::cerr << PlantID(this) << " state at start:\n\t"
              << _organs.size() << " organs"
              << "\n\tbiomasses:";
    for (decimal b: _biomasses) std::cerr << " " << b;
    std::cerr << "\n\treserves:";
    for (Layer l: L_EU::iterator())
      for (Element e: E_EU::iterator())
        std::cerr << "\n\t\t"
                  << L_EU::getName(l)[0]
                  << E_EU::getName(e)[0]
                  << ": " << _reserves[l][e] << " ("
                  << 100 * concentration(l, e) << " %)";
    std::cerr << "\n" << std::endl;
  }

  // Collect water
  decimal U_w = 0;
  decimal uw_k = (1 + concentration(Layer::ROOT, Element::WATER) * J_E); assert(uw_k >= 1);
  for (Organ *h: _hairs) {
    decimal w = env.waterAt(h->globalCoordinates().center);
    U_w += w * k_E * h->surface() / uw_k;
    if (debugMetabolism)
      std::cerr << OrganID(h) << " Drawing "
                << w * k_E * h->surface() / uw_k << " = "
                << w << " * " << k_E << " * " << h->surface() << " / " << uw_k
                << std::endl;
  }
  if (debugMetabolism)
    std::cerr << PlantID(this) << " Total water uptake of " << U_w;
  utils::iclip_max(U_w, _biomasses[Layer::ROOT] - _reserves[Layer::ROOT][Element::WATER]);
  if (debugMetabolism)
    std::cerr << " clipped to " << U_w << std::endl;
  _reserves[Layer::ROOT][Element::WATER] += U_w;
  assert(U_w >= 0);
  assert(_reserves[Layer::ROOT][Element::WATER] <= _biomasses[Layer::ROOT]);


  // Transport water
  decimal T_w = (
      concentration(Layer::ROOT, Element::WATER)
    - concentration(Layer::SHOOT, Element::WATER)
  ) / (
      (_genome.metabolism.resistors[Element::WATER] / _biomasses[Layer::ROOT])
    + (_genome.metabolism.resistors[Element::WATER] / _biomasses[Layer::SHOOT])
  );
  if (debugMetabolism)
    std::cerr << PlantID(this) << " transfering " << T_w
              << " = ( "
              << concentration(Layer::ROOT, Element::WATER)
              << " - " << concentration(Layer::SHOOT, Element::WATER)
              << " ) / ( ("
              << _genome.metabolism.resistors[Element::WATER] << " / " << _biomasses[Layer::ROOT]
              << ") + (" << _genome.metabolism.resistors[Element::WATER] << " / " << _biomasses[Layer::SHOOT]
              << ") ) from ROOT to SHOOT" << std::endl;
  _reserves[Layer::ROOT][Element::WATER] -= T_w;
  _reserves[Layer::SHOOT][Element::WATER] += T_w;


  // Produce glucose
  decimal U_g = 0;
  decimal ug_k = 1 + concentration(Layer::SHOOT, Element::GLUCOSE) * J_E; assert(ug_k >= 1);
  for (const physics::UpperLayer::Item &i: env.canopy(this)) {
    if (!i.organ->isLeaf()) continue;

    static constexpr decimal light = 1;
    U_g += light * k_E * (i.r - i.l) / ug_k;

    if (debugMetabolism)
      std::cerr << OrganID(i.organ) << " photosynthizing "
                << U_g << " = " << light << " * " << k_E << " * (" << i.r
                << " - " << i.l << ") / " << ug_k << std::endl;
  }
  if (debugMetabolism)
    std::cerr << PlantID(this) << " Total glucose production of " << U_g;
  utils::iclip_max(U_g, _reserves[Layer::SHOOT][Element::WATER]);
  utils::iclip_max(U_g, _biomasses[Layer::SHOOT] - _reserves[Layer::SHOOT][Element::GLUCOSE]);
  if (debugMetabolism)
    std::cerr << " clipped to " << U_g << std::endl;
  assert(U_g >= 0);
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
  if (debugMetabolism)
    std::cerr << PlantID(this) << " Transfering " << T_g
              << " = ( "
              << concentration(Layer::SHOOT, Element::GLUCOSE)
              << " - " << concentration(Layer::ROOT, Element::GLUCOSE)
              << " ) / ( ("
              << _genome.metabolism.resistors[Element::GLUCOSE] << " / " << _biomasses[Layer::SHOOT]
              << ") + (" << _genome.metabolism.resistors[Element::GLUCOSE] << " / " << _biomasses[Layer::ROOT]
              << ") ) from SHOOT to ROOT" << std::endl;
  _reserves[Layer::SHOOT][Element::GLUCOSE] -= T_g;
  _reserves[Layer::ROOT][Element::GLUCOSE] += T_g;


  // Transform resources into biomass
  if (debugMetabolism)
    std::cerr << PlantID(this) << " Transforming resources into biomass" << std::endl;
  Masses X, W, G;
  biomassRequirements(W, G);
  for (Layer l: L_EU::iterator()) {
    X[l] = _genome.metabolism.growthSpeed * _biomasses[l]
         * concentration(l, Element::GLUCOSE)
         * concentration(l, Element::WATER);

    if (debugMetabolism)
      std::cerr << "X[" << L_EU::getName(l) << "] = " << X[l] << " = "
                << _genome.metabolism.growthSpeed << " * " << _biomasses[l]
                << " * " << concentration(l, Element::GLUCOSE)
                << " * " << concentration(l, Element::WATER);

    utils::iclip_max(X[l], W[l] + G[l]);

    if (debugMetabolism)
      std::cerr << ", clipped to " << X[l] << std::endl;

    for (Element e: E_EU::iterator())
      _reserves[l][e] -= f_E * X[l];
  }


  // Transform biomass into wastes
  if (debugMetabolism) {
    std::cerr << PlantID(this) << " Wastes:";
    for (decimal f: W) std::cerr << " " << f;
    std::cerr << std::endl;
  }

  for (Layer l: L_EU::iterator()) X[l] -= W[l];

  // Distribute to consumers (non terminals, structurals, fruits)
  if (debugMetabolism) {
    std::cerr << PlantID(this) << " Available biomass:";
    for (Layer l: L_EU::iterator())
      std::cerr << " " << X[l];
    std::cerr << std::endl;
    std::cerr << "Requirements:";
    for (decimal f: G) std::cerr << " " << f;
    std::cerr << std::endl;
  }

  for (Layer l: L_EU::iterator())
    distributeBiomass(X[l], _sinks, G[l],
                      [&l] (Organ *o) {
      return o->layer() == l;
    });

//  for (Organ *s: _sinks) {
//    decimal r = s->requiredBiomass();
//    assert(s->isSeed() || r >= 0);
//    if (r <= 0) continue;
//    decimal b = X[s->layer()] * r / G[s->layer()];

//    if (debugMetabolism)
//      std::cerr << OrganID(s) << " b = "
//                << "X[" << s->layer() << "] * " << r << " / G[" << s->layer()
//                << "] = " << X[s->layer()] << " * " << r << " / "
//                << G[s->layer()] << " = " << X[s->layer()] * r / G[s->layer()]
//                << std::endl;

//    s->accumulate(std::min(r, b));
//  }

  // Update biomass (if needed)
  bool update = false;
  for (Layer l: L_EU::iterator())
    update |= (X[l] > 0);
  if (update) updateMetabolicValues();

  // Clip resources to new biomass
  for (Layer l: L_EU::iterator())
    for (Element e: E_EU::iterator())
      utils::iclip_max(_reserves[l][e], _biomasses[l]);

  if (debugMetabolism) {
    std::cerr << PlantID(this) << " state at end:\n\t"
              << _organs.size() << " organs"
              << "\n\tbiomasses:";
    for (decimal b: _biomasses) std::cerr << " " << b;
    std::cerr << "\n\treserves:";
    for (Layer l: L_EU::iterator())
      for (Element e: E_EU::iterator())
        std::cerr << "\n\t\t"
                  << L_EU::getName(l)[0]
                  << E_EU::getName(e)[0]
                  << ": " << _reserves[l][e] << " ("
                  << 100 * concentration(l, e) << " %)";
    std::cerr << "\n" << std::endl;
  }

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
             R"("G_{sh}" "G_{rt}")" << "\n";
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

void Plant::updateMetabolicValues(void) {
  auto oldBiomasses = _biomasses;
  _biomasses.fill(0);

  static constexpr bool d = debugMetabolism || debugDerivation || debugReproduction;
  if (d)  std::cerr << PlantID(this) << " Updating biomasses:\n";

  for (Organ *o: _organs) {
    if (d)  std::cerr << OrganID(o) << " \tM[" << L_EU::getName(o->layer())
                      << "] += " << o->biomass() << "\n";
    _biomasses[o->layer()] += o->biomass();
  }

  if (d) {
    std::cerr << PlantID(this) << " Updated biomasses:";
    for (Layer l: L_EU::iterator()) {
      std::cerr << " [" << L_EU::getName(l) << ":" << _biomasses[l]
                << " (dM=" << _biomasses[l] - oldBiomasses[l] << ")]";
    }
    std::cerr << std::endl;
  }

  for (Layer l: L_EU::iterator()) {
    if (_biomasses[l] < oldBiomasses[l]) {
      if (d)
        std::cerr << PlantID(this) << " Clipped to " << _biomasses[l] << std::endl;
      for (Element e: E_EU::iterator())
        utils::iclip_max(_reserves[l][e], _biomasses[l]);
    }
  }
}

float Plant::initialBiomassFor (const Genome &g) {
  return ruleBiomassCost(g, Layer::SHOOT, GConfig::ls_axiom())
       + ruleBiomassCost(g, Layer::ROOT, GConfig::ls_axiom());
}

void Plant::collectFruits(Seeds &seeds, Environment &env) {
  bool dead = this->isDead();
  if (debugReproduction && dead)
    std::cerr << PlantID(this) << " Dead. Force collecting fruits" << std::endl;

  std::vector<Organ*> collectedFruits;
  for (auto &p: _fruits) {
    Organ *fruit = p.second.fruit;

    if (fruit->biomass() <= 0) {
      if (debugReproduction)
        std::cerr << PlantID(this) << " Pending fruit " << OrganID(fruit)
                  << " rotted on its branch (" << fruit->biomass()
                  << " available biomass)" << std::endl;
      continue;
    }

    if (debugReproduction)
      std::cerr << PlantID(this) << " Pending fruit " << OrganID(fruit) << " at "
                << 100 * fruit->fullness() << "% capacity" << std::endl;

    if (dead || fabs(fruit->fullness() - 1) < 1e-3) {
      uint S = _genome.seedsPerFruit;
      float biomass = fruit->biomass();
      float requestedBiomass = seedBiomassRequirements(_genome, p.second.genome);
      float biomassPerSeed = std::min(biomass, requestedBiomass);
      for (uint i=0; i<S && biomass > 0; i++) {
        Genome g = p.second.genome;
        Point pos = fruit->globalCoordinates().center;

        g.cdata.id = genotype::BOCData::nextID();
        seeds.push_back({biomassPerSeed, g, pos});
        biomass -= biomassPerSeed;

        if (debugReproduction)
          std::cerr << PlantID(this) << " Created seed from " << OrganID(fruit)
                    << " at " << 100 * biomassPerSeed / requestedBiomass
                    << "% capacity" << std::endl;
      }
      collectedFruits.push_back(fruit);
    }
  }

  while (!collectedFruits.empty()) {
    Organ *fruit = collectedFruits.back();
    collectedFruits.pop_back();
    delOrgan(fruit, env);
  }
}

void Plant::updateRequirements(void) {
  for (Organ *o: _organs) {
    if (o->isStructural()) {
      float db = sinkRequirement(o) - o->accumulatedBiomass();
      if (debugGrowth)
        std::cerr << OrganID(o) << " Updated biomass requirement (" << db
                  << ")" << std::endl;
      assert(db >= 0);
      o->setRequiredBiomass(db);
    }
  }
}

void Plant::update (Environment &env) {
  if (_dirty.test(DIRTY_COLLISION)) {
    env.updateCollisionDataFinal(this);
    _dirty.set(DIRTY_COLLISION, false);
  }
  if (_dirty.test(DIRTY_METABOLISM)) {
    updateMetabolicValues();
    _dirty.set(DIRTY_METABOLISM, false);
  }
}

void Plant::step (Environment &env, Seeds &seeds) {
  if (debug)
    std::cerr << "## Plant " << id() << ", " << age() << " days old ##"
              << std::endl;

  if (_age % SConfig::stepsPerDay() == 0) {

    bool derived = bool(deriveRules(env));
    _derived += derived;

    // Remove non terminals when reaching maximal derivation for a given lsystem
    for (Layer l: {Layer::SHOOT, Layer::ROOT}) {
      if (_genome.maxDerivations(l) >= _derived)  continue;
      if (debugDerivation)
        std::cerr << PlantID(this) << " Trimming non terminals for "
                  << L_EU::getName(l) << std::endl;
      auto nonTerminals = _nonTerminals;
      for (Organ *o: nonTerminals) {
        if (o->layer() == l) {
          updateSubtree(o, o->parent(), o->localRotation());
          delOrgan(o, env);
        }
      }
    }

    if (derived)  updateRequirements();
    update(env);
  }

  metabolicStep(env);
  collectFruits(seeds, env);

  /// Recursively delete dead organs
  // First take note of the current flowers
  std::map<Organ*, Point> oldPistilsPositions;
  if (sex() == Sex::FEMALE)
    for (Organ *f: _flowers)
      oldPistilsPositions.emplace(f, f->globalCoordinates().center);

  // Perform suppressions
  auto bases = _bases;
  std::function<void(Organ*)> destroyDeadSubtrees =
    [&destroyDeadSubtrees, &env, this] (Organ *o) {
      if (o->biomass() < 0) delOrgan(o, env);
      else  for (Organ *c: o->children()) destroyDeadSubtrees(c);
  };
  for (Organ *o: bases)  destroyDeadSubtrees(o);

  // Update remaining flowers (check if some space was freed)
  for (auto &pair: oldPistilsPositions)
    if (_flowers.find(pair.first) == _flowers.end())
      for (Organ *f: _flowers)
        if (fabs(f->globalCoordinates().center.x - pair.second.x) < 1e-3)
          env.updateGeneticMaterial(f, f->globalCoordinates().center);

  update(env);

  _age++;

  if (debug)  std::cerr << std::endl;

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
