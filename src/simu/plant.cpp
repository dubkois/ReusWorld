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
static constexpr int debugDerivation = 0;
static constexpr int debugMetabolism = false;
static constexpr bool debugReproduction = false;

static constexpr bool debug = false
  | debugInit | debugOrganManagement | debugDerivation | debugMetabolism
  | debugReproduction;


bool nonNullAngle (float a) {
  return fabs(a) > 1e-6;
}

Plant::Plant(const Genome &g, const Point &pos)
  : _genome(g), _pos(pos), _age(0), _derived(0), _killed(false),
    _pstats(nullptr), _pstatsWC(nullptr) {

  _nextOrganID = OID(0);

  _biomasses.fill(0);

  _boundingRect = {0,0,0,0};

  auto _r = _reserves[Layer::SHOOT];
  _r.fill(0);
  _reserves.fill(_r);
}

Plant::~Plant (void) {
  if (!_fruits.empty()) {
    std::ostringstream oss;
    oss << PlantID(this) << " destroyed with unprocessed seeds:";
    for (auto &fd: _fruits)
      for (auto &g: fd.second.genomes)
        oss << " " << g.id();
    autopsy();
    utils::doThrow<std::logic_error>(oss.str());
  }
  for (Organ *o: _organs)
    delete o;
  if (_pstats)  _pstats->plant = nullptr;
}

void Plant::init (Environment &env, float biomass, const PData &pdata) {
  auto A = genotype::grammar::toSuccessor(GConfig::ls_axiom());
  for (Layer l: {Layer::SHOOT, Layer::ROOT})
    addOrgan(turtleParse(nullptr, A, initialAngle(l), l, env, false), env);


  // Distribute initial resources to producer modules
  assert(isInSeedState());
  float totalBR = 0;
  for (Organ *o: _organs) totalBR += o->requiredBiomass();
  for (Organ *o: _organs)
    o->accumulate(biomass * o->requiredBiomass() / totalBR);

  updateMetabolicValues();
  updateGeometry();
  env.updateCollisionDataFinal(this);

  _genome.genealogy().setSID(pdata.sid);

  if (pdata.udata) {
    setPStatsPointer(pdata.udata);
    _pstats->born = true;
    _pstats->seed = isInSeedState();
    _pstats->pos = _pos;
    _pstats->birth = env.time();
    _pstats->plant = this;
  }

  if (debugInit)
    std::cerr << PlantID(this) << " Initialized. Genome is " << _genome
              << std::endl;
}

void Plant::destroy(void) {
  while (!_fruits.empty())
    collectSeedsFrom(_fruits.begin()->second.fruit);
}

void Plant::replaceWithFruit (Organ *o, const std::vector<Genome> &litter,
                              Environment &env) {

  using genotype::grammar::toSuccessor;
  using Rule = genotype::grammar::Rule_base;

  assert(o->isFlower());

  Organ *parent = o->parent();
  float rotation = o->localRotation();
  Layer l = o->layer();
  assert(l == Layer::SHOOT);

  auto p = _fruits.insert({_nextOrganID, {litter, nullptr}});
  assert(p.second);

  Organ *fruit = turtleParse(parent, toSuccessor(Rule::fruitSymbol()), rotation,
                             l, env, false);

  addOrgan(fruit, env);

  p.first->second.fruit = fruit;

  _dirty.set(DIRTY_METABOLISM, true);

  if (debugReproduction)
    std::cerr << OrganID(o) << " Replacing with fruit "
              << OrganID(fruit, true) << " at O"
              << o->globalCoordinates().origin << std::endl;

  // Clean up
  Point fpos = o->globalCoordinates().center;
  delOrgan(o, env);

  // Update other flowers in case a competitor can take the freed-up space
  for (Organ *f: _flowers)
    if (fabs(f->globalCoordinates().center.x - fpos.x) < 1e-3)
      env.updateGeneticMaterial(f, f->globalCoordinates().center);
}

void Plant::resetStamen(Organ *s) {
  s->accumulate(-s->biomass() + s->baseBiomass());
  updateMetabolicValues();
}

float Plant::age (void) const {
  return _age / float(SConfig::stepsPerDay());
}

bool Plant::starvedSeed(void) const {
  for (Organ *s: _nonTerminals)
    if (s->requiredBiomass() > 0)
      return true;
  return false;
}

bool Plant::spontaneousDeath(void) const {
  if (config::Simulation::DEBUG_NO_METABOLISM())  return false;

  if (_genome.dethklok <= age())  return true;
  if (_sinks.empty())  return true;
  for (Layer l: L_EU::iterator())
    for (Element e: E_EU::iterator())
      if (_reserves[l][e] < 0)
          return true;
  return false;
}

void Plant::autopsy(void) const {
  std::cerr << PlantID(this) << " Deleted. Death cause was ";
  if (starvedSeed()) {
    std::cerr << "starved seed\n"
              << "\tIncrimited non-terminals:";
    for (Organ *s: _nonTerminals)
      if (s->requiredBiomass() > 0)
        std::cerr << " " << OrganID(s);
    std::cerr << "\n" << std::endl;

  } else if (spontaneousDeath()) {
    std::cerr << "spontaneous\n"
              << "\n\tReserves:";

    using PL = EnumUtils<Plant::Layer>;
    using PE = EnumUtils<Plant::Element>;
    for (auto l: PL::iterator())
      for (auto e: PE::iterator())
        std::cerr << " {" << PL::getName(l)[0] << PE::getName(e)[0]
                  << ": " << 100 * concentration(l, e) << "%}";

    std::cerr << "\n\tAge: " << age() << " / " << _genome.dethklok
              << "\n\tOrgans: " << _organs.size()
              << " (" << _sinks.size() << " of which are sinks)"
              << "\n" << std::endl;
  } else
    std::cerr << " forced" << std::endl;
}

template <typename F = std::function<bool(const Organ*)>>
void printSubTree (const Organ *st, uint depth,
                   F doPrint = [] (const Organ*) { return true; }) {

  if (doPrint(st)) {
    std::cerr << std::string(depth, ' ') << OrganID(st) << "\n";
    for (Organ *sst: st->children())  printSubTree(sst, depth+2, doPrint);
  }
}

uint Plant::deriveRules(Environment &env) {
  // Store current pistils location in case of an update
  std::map<OID, Point> oldPistilsPositions;
  if (sex() == Sex::FEMALE)
    for (Organ *f: _flowers)
      oldPistilsPositions.emplace(f->id(), f->globalCoordinates().center);

  uint derivations = 0;
  std::set<OID> unprocessed;
  for (Organ *o: _nonTerminals) unprocessed.insert(o->id());

  for (OID id: unprocessed) {
    auto it = _nonTerminals.find(id);
    if (it == _nonTerminals.end())
      utils::doThrow<std::logic_error>("Lost track of non terminal ", id, "!");

    Organ *apex = *it;
    Layer layer = apex->layer();
    auto succ = _genome.successor(layer, apex->symbol());

    // Did the apex accumulate enough resources ?
    if (apex->requiredBiomass() > 0) {
      if (debugDerivation > 1)
        std::cerr << OrganID(apex) << " Apex is not ready (" << 100*apex->fullness()
                  << "%)" << std::endl;
      continue;

    } else if (debugDerivation)
      std::cerr << OrganID(apex) << " Applying " << apex->symbol() << " -> "
                << succ << std::endl;

    // Create new organs
    Organs newOrgans; // Collection of all newly created organs
    Organ *newApex;   // Last organ of the new subtree
    float angle = apex->localRotation();  // Remaining rotation after the last
    newApex = turtleParse(apex->parent(), succ, angle, layer, newOrgans,
                          env, true);

    Organs stBases; // Roots of the new subtree
    for (Organ *o: newOrgans) if (o->parent() == apex->parent())  stBases.insert(o);

    // Create clones of subtree under 'apex'
    for (Organ *c: apex->children()) {
      Organ *c_ = c->cloneAndUpdate(newApex, angle);
      if (newApex)  newApex->children().insert(c_);
      if (apex->parent() == newApex)  stBases.insert(c_);
    }

    // Create temporary collision data
    const auto inSelf = [apex] (const Organ *o) {
      return (o != apex) && !o->isUncommitted();
    };
    Branch self (_bases, inSelf);

    Branch branch (stBases);

    if (debugDerivation > 1) {
      std::cerr << "Rule:\n";
      for (Organ *st: stBases)
        printSubTree(st, 13, [&newOrgans] (const Organ *o) {
          return newOrgans.find(const_cast<Organ*>(o)) != newOrgans.end();
        });
      std::cerr << "Branch:\n";
      for (Organ *st: stBases)  printSubTree(st, 13);
      std::cerr << "Self:\n";
      for (const Organ *o: _bases)  printSubTree(o, 13, inSelf);
      std::cerr << std::endl;
    }

    // Perform collision detection
    using CR = physics::CollisionResult;
    CR cres = env.collisionTest(this, self, branch, newOrgans);

    if (cres != CR::NO_COLLISION) {
      if (debugDerivation) {
        std::cerr << "\tCanceled due to collision of class ";
        switch (cres) {
        case CR::AUTO_COLLISION:
          std::cerr << "auto. Deleting apex " << OrganID(apex); break;
        case CR::BRANCH_COLLISION:  std::cerr << "branch";  break;
        case CR::INTRA_COLLISION:   std::cerr << "intra";  break;
        case CR::INTER_COLLISION:   std::cerr << "inter";  break;
        default:  std::cerr << "ERROR";
        }
        std::cerr << "\n" << std::endl;
      }

      for (Organ *st: stBases)  delOrgan(st, env);
      for (Organ *st: apex->children()) commit(st);

      if (cres == CR::AUTO_COLLISION) {
        // Also delete apex
        updateSubtree(apex, apex->parent(), apex->localRotation());
        delOrgan(apex, env);
      }

      continue;
    }


    float leftOverBiomass = apex->biomass()
                          - ruleBiomassCost(apex->layer(), apex->symbol());

    if (debugDerivation)
      std::cerr << "\tDerivation validated. Commiting." << std::endl;

    if (apex->parent()) {
      Organ *ap = apex->parent();
      for (Organ *b: stBases) ap->children().insert(b);
    }

    // Destroy
    delOrgan(apex, env);

    for (Organ *st: stBases) {
      commit(st);
      addSubtree(st, env);
    }

    updateGeometry();
//      env.updateCollisionData(this);

    // Divide remaining biomass into newly created sinks
    if (leftOverBiomass > 0) {
      if (debugMetabolism || (debugDerivation > 1))
        std::cerr << PlantID(this) << " Dispatching " << leftOverBiomass
                  << " left over biomass" << std::endl;
      distributeBiomass(leftOverBiomass, newOrgans,
                        [] (Organ*) { return true; });
    }

    derivations++;

    // Update pistils positions cache
    if (sex() == Sex::FEMALE)
      for (Organ *p: _flowers)
        if (oldPistilsPositions.find(p->id()) == oldPistilsPositions.end())
          oldPistilsPositions.emplace(p->id(), p->globalCoordinates().center);

    if (debugDerivation > 1)
      std::cerr << "\t\t>>> " << toString(layer) << std::endl;
  }

  // Update pistils as needed
  for (auto &p: oldPistilsPositions) {
    auto it = _flowers.find(p.first);
    if (it == _flowers.end())
      utils::doThrow<std::logic_error>("Lost track of pistil ", p.first, "!");

    Organ *pistil = *it;
    if (p.second != pistil->globalCoordinates().center) {
      env.updateGeneticMaterial(pistil, p.second); // If position changed

      // Check if this freed up some space for others
      for (Organ *f: _flowers)
        if (f->id() != p.first
            && fabs(f->globalCoordinates().center.x - p.second.x) < 1e-3)
          env.updateGeneticMaterial(f, f->globalCoordinates().center);
    }
  }

  // Update all depths (wasteful but accurate)
  updateDepths();

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
  _boundingRect = Rect{{0,0},{0,0}};
  for (Organ *o: _organs)
    _boundingRect.uniteWith(o->inPlantCoordinates().boundingRect);
}

Organ* Plant::turtleParse (Organ *parent, const std::string &successor,
                           float &angle, Layer type, Organs &newOrgans,
                           Environment &env, bool checkMedium) {

  using R = Rule_base;

  for (size_t i=0; i<successor.size(); i++) {
    char c = successor[i];
    if (R::isValidControl(c)) {
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
                    a, type, newOrgans, env, checkMedium);
        break;
      }
    } else if (R::isValidNonTerminal(c) || R::isTerminal(c)
               || c == genotype::grammar::Rule_base::fruitSymbol()) {
      Organ *o = makeOrgan(parent, angle, c, type);

      bool correctMedium = false;
      if (checkMedium) {
        Point pos = o->globalCoordinates().center;
        float h = env.heightAt(pos.x);
        if (type == Layer::SHOOT) correctMedium = (pos.y >= h);
        if (type == Layer::ROOT)  correctMedium = (pos.y <= h);

      } else
        correctMedium = true;

      if (!correctMedium)
        delOrgan(o, env);

      else {
        newOrgans.insert(o);
        parent = o;
        angle = 0;
      }

    } else
      utils::doThrow<std::logic_error>(
            "Invalid character at position ", i, " in ", successor);
  }

  return parent;
}


Organ* Plant::makeOrgan(Organ *parent, float angle, char symbol, Layer type) {
  auto size = _genome.sizeOf(symbol);
  Organ *o = new Organ(this, size.width, size.length, angle,
                       symbol, type, parent);

  if (debugOrganManagement) {
    std::cerr << PlantID(this) << " Created " << *o;
    if (parent) std::cerr << " under " << *parent;
    std::cerr << std::endl;
  }

  o->updateDimensions(true);

  return o;
}

void Plant::addOrgan(Organ *o, Environment &env) {
  assert(!o->isCloned()); // Must be commited

  if (o->id() == OID::INVALID) {
    o->setID(_nextOrganID);
    _nextOrganID = OID(uint(_nextOrganID)+1);
  }

  _organs.insert(o);

  if (debugOrganManagement) {
    std::cerr << PlantID(this) << " Inserted " << *o;
    if (o->parent())  std::cerr << " under " << *o->parent();
    std::cerr << std::endl;
  }

  assignToViews(o);
  o->setRequiredBiomass(isSink(o) ? sinkRequirement(o) : 0);

  if (o->isFruit()) {
    FruitData &fd = _fruits.at(o->id());
    fd.fruit = o;
  }

  if (o->isFlower() && sex() == Sex::FEMALE)
    env.disseminateGeneticMaterial(o);
}

void Plant::assignToViews(Organ *o) {
  if (!o->parent()) _bases.insert(o);
  if (o->isNonTerminal()) _nonTerminals.insert(o);
  if (o->isHair())  _hairs.insert(o);
  if (o->isFlower())  _flowers.insert(o);

  if (isSink(o))  _sinks.insert(o);
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
    if (sex() == Sex::FEMALE) env.removeGeneticMaterial(o);
  }

  if (o->isFruit() && !o->isCloned()) {
    if (debugOrganManagement || debugReproduction)
      std::cerr << PlantID(this) << " Collecting seeds from fruit "
                << OrganID(o) << std::endl;
    collectSeedsFrom(o);
  }

  _dirty.set(DIRTY_COLLISION, true);
  delete o;
}

bool Plant::destroyDeadSubtree(Organ *o, Environment &env) {
  if (o->isDead()) {
    delOrgan(o, env);
    return true;

  } else {
    bool deleted = false;
    auto subtrees = o->children();
    for (Organ *st: subtrees)
      deleted |= destroyDeadSubtree(st, env);
    return deleted;
  }
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
    for (const Genome &g: _fruits.at(o->id()).genomes)
      required += seedBiomassRequirements(_genome, g);

  } else if (o->isStructural()) {
    const auto &size = _genome.sizeOf(o->symbol());
    required = size.area()
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
  static const auto& ntcost = [] {
    const auto ssize = GConfig::sizeOf('s');
    return ssize.area() * GConfig::ls_nonTerminalCost();
  }();

  const std::string &succ = g.successor(l, symbol);
  float cost = 0;
  for (char c: succ) {
    if (Rule_base::isTerminal(c))
      cost += g.sizeOf(c).area();

    else if (Rule_base::isValidNonTerminal(c))
      cost += ntcost;

//  else no cost
  }
  return cost;
}

void Plant::biomassRequirements (Masses &wastes, Masses &growth) {
  static const auto &lifeCosts = SConfig::lifeCosts();

  wastes.fill(0);

  // New, per-organ, wastes production
  for (Organ *o: _organs)
    if (!o->isNonTerminal())
      wastes[o->layer()] += lifeCosts.at(o->symbol()) * o->biomass();

//  Old, single-value, wastes production
//  for (Layer l: EnumUtils<Layer>::iterator())
//    wastes[l] = SConfig::lifeCost() * _biomasses[l];

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

float Plant::heatEfficiency(float T) const {
  const auto &mu_r = SConfig::temperatureMaxRange();
  const auto &sigma_r = SConfig::temperatureRangePenalty();

  const auto mu = _genome.temperatureOptimal;
  const auto sigma = _genome.temperatureRange;
  return utils::gauss(T, mu, sigma) * utils::gauss(sigma, mu_r, sigma_r);
}

void Plant::metabolicStep(Environment &env) {
  using genotype::Element;

  if (SConfig::DEBUG_NO_METABOLISM()) // Fill sinks
    for (Organ *o: _sinks)
      o->accumulate(o->requiredBiomass());

  const auto &k_E = SConfig::assimilationRate();
  const auto &J_E = SConfig::saturationRate();
  const auto &f_p = SConfig::photosynthesisCost();
  const auto &f_E = SConfig::resourceCost();

  float T = env.temperatureAt(_pos.x);
  if (_pstats)  _pstatsWC->sumTemperature += T;
  float T_eff = heatEfficiency(T);
  float T_dir = utils::sgn(T - _genome.temperatureOptimal);

  if (debugMetabolism) {
    std::cerr << PlantID(this) << " state at start:\n\t"
              << _organs.size() << " organs"
              << "\n\tTemperature = " << T << " (" << 100 * T_eff << "% efficiency)"
              << "\n\t  biomasses:";
    for (decimal b: _biomasses) std::cerr << " " << b;
    std::cerr << "\n\t   reserves:";
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

  if (_pstats)  _pstatsWC->tmpSum = 0;

  for (Organ *h: _hairs) {
    decimal w = env.waterAt(h->globalCoordinates().center);

    if (_pstats)  _pstatsWC->tmpSum += w;

    U_w += w * k_E * h->surface() / uw_k;
    if (debugMetabolism)
      std::cerr << OrganID(h) << " Drawing "
                << w * k_E * h->surface() / uw_k << " = "
                << w << " * " << k_E << " * " << h->surface() << " / " << uw_k
                << std::endl;
  }
  if (_pstats && _pstatsWC->tmpSum > 0)
    _pstatsWC->sumHygrometry += _pstatsWC->tmpSum / _hairs.size();

  if (debugMetabolism)
    std::cerr << PlantID(this) << " Total water uptake of " << U_w;

  if (T_dir < 0)  // Too cold reduce water intake
    U_w *= T_eff;

  if (debugMetabolism && T_dir < 0)
    std::cerr << " reduced by low temperatures to " << U_w;

  utils::iclip_max(U_w, _biomasses[Layer::ROOT] - _reserves[Layer::ROOT][Element::WATER]);

  if (debugMetabolism && std::fabs(_biomasses[Layer::ROOT] - _reserves[Layer::ROOT][Element::WATER] - U_w) < 1e-3)
    std::cerr << " clipped to " << U_w;

  if (debugMetabolism)  std::cerr << std::endl;

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
  decimal light = env.lightAt(_pos.x);

  if (_pstats)  _pstatsWC->tmpSum = 0;

  const auto &canopy = env.canopy(this);
  for (const physics::UpperLayer::Item &i: canopy) {
    if (!i.organ->isLeaf()) continue;

    float d_g = light * k_E * (i.r - i.l) / ug_k;
    U_g += d_g;

    if (_pstats)  _pstatsWC->tmpSum += light * (i.r - i.l)
                      / i.organ->inPlantCoordinates().boundingRect.width();

    if (debugMetabolism)
      std::cerr << OrganID(i.organ) << " photosynthizing "
                << d_g << " = " << light << " * " << k_E << " * (" << i.r
                << " - " << i.l << ") / " << ug_k << std::endl;
  }

  if (_pstats && _pstatsWC->tmpSum > 0)
    _pstatsWC->sumLight += _pstatsWC->tmpSum / canopy.size();

  if (debugMetabolism)
    std::cerr << PlantID(this) << " Total glucose production of " << U_g;
  utils::iclip_max(U_g, f_p * _reserves[Layer::SHOOT][Element::WATER]);
  utils::iclip_max(U_g, _biomasses[Layer::SHOOT] - _reserves[Layer::SHOOT][Element::GLUCOSE]);
  if (debugMetabolism)
    std::cerr << " clipped to " << U_g << std::endl;
  assert(U_g >= 0);
  _reserves[Layer::SHOOT][Element::WATER] -= f_p * U_g;
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

  // Evaporate water
  if (T_dir > 0) {
    decimal W_sh = _reserves[Layer::SHOOT][Element::WATER];

    if (debugMetabolism)
      std::cerr << PlantID(this) << " Evaporating "
                << (1.f - T_eff) * W_sh / SConfig::stepsPerDay()
                << " = (1 - " << T_eff << ") * " << W_sh << " / " << SConfig::stepsPerDay()
                << std::endl;

    _reserves[Layer::SHOOT][Element::WATER] -= (1.f - T_eff) * W_sh / SConfig::stepsPerDay();
   }

  // Compute biomass needs
  Masses X, W, G;
  biomassRequirements(W, G);

  // Increase wastes when outside comfortable temperature range
  for (decimal &f: W) f *= 2 - T_eff;

  // Transform resources into biomass
  if (debugMetabolism)
    std::cerr << PlantID(this) << " Transforming resources into biomass" << std::endl;
  for (Layer l: L_EU::iterator()) {
    X[l] = _genome.metabolism.growthSpeed * _biomasses[l]
         * concentration(l, Element::GLUCOSE)
         * concentration(l, Element::WATER);

    if (debugMetabolism)
      std::cerr << "\tX[" << L_EU::getName(l) << "] = " << X[l] << " = "
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
    std::cerr << " (" << 100 * (2 - T_eff) << "%)" << std::endl;
  }

  for (Layer l: L_EU::iterator()) X[l] -= W[l];

  // Distribute to consumers (non terminals, structurals, fruits)
  if (debugMetabolism) {
    std::cerr << PlantID(this) << " Available biomass:";
    for (Layer l: L_EU::iterator())
      std::cerr << " " << X[l];
    std::cerr << std::endl;
    std::cerr << PlantID(this) << " Requirements:";
    for (decimal f: G) std::cerr << " " << f;
    std::cerr << std::endl;
  }

  for (Layer l: L_EU::iterator())
    distributeBiomass(X[l], _sinks, G[l],
                      [&l] (Organ *o) {
      return o->layer() == l;
    });

  // Update biomass (if needed)
  bool update = false;
  for (Layer l: L_EU::iterator())
    update |= (X[l] != 0);
  if (update) updateMetabolicValues();

  // Clip resources to new biomass
  for (Layer l: L_EU::iterator())
    for (Element e: E_EU::iterator())
      utils::iclip_max(_reserves[l][e], _biomasses[l]);

  if (debugMetabolism) {
    std::cerr << PlantID(this) << " State at end:\n\t"
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
}

void Plant::updateMetabolicValues(void) {
  auto oldBiomasses = _biomasses;
  _biomasses.fill(0);

  static constexpr bool d = debugMetabolism
      || (debugDerivation > 1)
      || debugReproduction;

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
        std::cerr << PlantID(this) << " Clipping "
                  << L_EU::getName(l)<< " reserves to " << _biomasses[l] << std::endl;
      for (Element e: E_EU::iterator())
        utils::iclip_max(_reserves[l][e], _biomasses[l]);
    }
  }
}

float Plant::initialBiomassFor (const Genome &g) {
  return ruleBiomassCost(g, Layer::SHOOT, GConfig::ls_axiom())
       + ruleBiomassCost(g, Layer::ROOT, GConfig::ls_axiom());
}

void Plant::collectSeedsFrom(Organ *fruit) {
  if (fruit->isUncommitted()) return;

  FruitData fd = utils::take(_fruits, fruit->id());
  uint S = fd.genomes.size();
  float biomass = fruit->biomass();
  Point pos = fruit->globalCoordinates().center;
  for (uint i=0; i<S; i++) {
    const Genome &g = fd.genomes[i];
    float requestedBiomass = seedBiomassRequirements(_genome, g);
    float obtainedBiomass = std::min(biomass, requestedBiomass);

    _currentStepSeeds.push_back({obtainedBiomass, g, pos});
    biomass -= obtainedBiomass;

    if (debugReproduction)
      std::cerr << PlantID(this) << " Created seed from " << OrganID(fruit)
                << " at " << 100 * obtainedBiomass / requestedBiomass
                << "% capacity" << std::endl;
  }
}

void Plant::processFruits(Environment &env) {
  std::vector<Organ*> collectedFruits;
  for (auto &p: _fruits) {
    Organ *fruit = p.second.fruit;

    if (debugReproduction)
      std::cerr << PlantID(this) << " Pending fruit " << OrganID(fruit)
                << " at " << 100 * fruit->fullness() << "% capacity"
                << std::endl;

    if (fabs(fruit->fullness() - 1) < 1e-3)
      collectedFruits.push_back(fruit);
  }

  bool needUpdate = !collectedFruits.empty();
  while (!collectedFruits.empty()) {
    Organ *fruit = collectedFruits.back();
    collectedFruits.pop_back();
    delOrgan(fruit, env);
  }

  if (needUpdate) {
    updateGeometry();
    updateDepths();
    _dirty.set(DIRTY_COLLISION, true);
    _dirty.set(DIRTY_METABOLISM, true);
  }
}

void Plant::updateRequirements(void) {
  for (Organ *o: _organs) {
    if (o->isStructural()) {
      float db = sinkRequirement(o) - o->accumulatedBiomass();
      assert(db >= 0);
      o->setRequiredBiomass(db);
    }
  }
}

void Plant::updatePosition (float newx) {
  _pos.x = newx;
  for (Organ *o: _organs) o->updateGlobalTransformation();
  updateGeometry();
}

void Plant::updateAltitude(Environment &env, float h) {
  _pos.y = h;

  // Store current pistils location
  std::map<Organ*, Point> oldPistilsPositions;
  if (sex() == Sex::FEMALE)
    for (Organ *f: _flowers)
      oldPistilsPositions.emplace(f, f->globalCoordinates().center);

  for (Organ *o: _organs)
    o->updateGlobalTransformation();

  // Update pistils with new altitude
  for (auto &p: oldPistilsPositions)
    env.updateGeneticMaterial(p.first, p.second);

  updateGeometry();
  env.updateCollisionData(this);
  env.updateCollisionDataFinal(this);
}

void Plant::update (Environment &env) {
  if (isDirty(DIRTY_COLLISION)) {
    updateGeometry();
    env.updateCollisionDataFinal(this);
    _dirty.set(DIRTY_COLLISION, false);
  }
  if (isDirty(DIRTY_METABOLISM)) {
    updateMetabolicValues();
    _dirty.set(DIRTY_METABOLISM, false);
  }
}

uint Plant::step(Environment &env) {
  if (debug)
    std::cerr << "## Plant " << id() << ", " << age() << " days old ##"
              << std::endl;

  // Check if there is still enough resources to grow the first rules
  if (isInSeedState() && starvedSeed()) kill();

  uint derived = 0;
  if ((SConfig::DEBUG_NO_METABOLISM() || _age % SConfig::stepsPerDay() == 0)
    && !_nonTerminals.empty()) {

    derived += deriveRules(env);
    _derived += bool(derived);

    if (!_nonTerminals.empty()) {
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
    }

    if (derived)  updateRequirements();
    update(env);
  }

  metabolicStep(env);

  processFruits(env);

  /// Recursively delete dead organs
  // First take note of the current flowers
  std::map<OID, Point> oldPistilsPositions;
  if (sex() == Sex::FEMALE)
    for (Organ *f: _flowers)
      oldPistilsPositions.emplace(f->id(), f->globalCoordinates().center);

  // Perform suppressions
  bool deleted = false;
  auto bases = _bases;
  for (Organ *o: bases)  deleted |= destroyDeadSubtree(o, env);
  if (deleted) {
    updateDepths();
    updateMetabolicValues();
  }

  // Update remaining flowers (check if some space was freed)
  for (auto &pair: oldPistilsPositions)
    if (_flowers.find(pair.first) == _flowers.end())
      for (Organ *f: _flowers)
        if (f->id() != pair.first
            && fabs(f->globalCoordinates().center.x - pair.second.x) < 1e-3)
          env.updateGeneticMaterial(f, f->globalCoordinates().center);

  update(env);

  _age++;

  if (_pstats)  updatePStats(env);

  if (debug)  std::cerr << std::endl;

//  std::cerr << "State at end:\n";
//  std::cerr << *this;
//  std::cerr << std::endl;

  return derived;
}

void Plant::updatePStats(Environment &env) {
  auto &ps = *_pstats;
  auto &wc = *_pstatsWC;

  float b = biomass();
  if (wc.largestBiomass <= b) {
    wc.largestBiomass = b;
    ps.shoot = toString(Layer::SHOOT);
    ps.root = toString(Layer::ROOT);
  }

  ps.seed = isInSeedState();

  ps.death = env.time();
  ps.lifespan = age();

  ps.avgTemperature = wc.sumTemperature / _age;
  ps.avgHygrometry = wc.sumHygrometry / _age;
  ps.avgLight = wc.sumLight / _age;

  assert(!std::isnan(wc.largestBiomass));
  assert(!std::isnan(wc.sumTemperature));
  assert(!std::isnan(wc.sumHygrometry));
  assert(!std::isnan(wc.sumLight));

  assert(!std::isnan(ps.avgHygrometry));
  assert(!std::isnan(ps.avgLight));
}

void PStats::removedFromEnveloppe(void) {
  if (plant)  plant->setPStatsPointer(nullptr);
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

  using SortedOrgans = Organ::SortedCollection;
  SortedOrgans sortedChildren (o->children().begin(), o->children().end());
  bool children = sortedChildren.size() > 1;
  str += o->symbol();
  for (Organ *c: sortedChildren) {
    if (children) str += "[";
    toString(c, str);
    if (children) str += "]";
  }
}

std::string Plant::toString(Layer type) const {
  uint n = 0;
  Organ::SortedCollection sortedBases;
  for (Organ *o: _bases) {
    n += (o->layer() == type);
    sortedBases.insert(o);
  }

  std::string str;
  for (Organ *o: sortedBases) {
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

Plant* Plant::clone (const Plant &that_p,
                     std::map<const Plant *,
                              std::map<const Organ*, Organ*>> &olookups) {
  Plant *this_p = new Plant(that_p._genome, that_p._pos);

  this_p->_age = that_p._age;

  auto &olookup = olookups[&that_p];
  for (const Organ *that_o: that_p._organs) {
    Organ *this_o = Organ::clone(that_o, this_p);
    this_p->_organs.insert(this_o);
    this_p->assignToViews(this_o);
    olookup[that_o] = this_o;
  }

  // Update with cloned parent organ (if any)
  for (Organ *this_o: this_p->_organs)
    this_o->updatePointers(olookup);

  this_p->_derived = that_p._derived;

  this_p->_boundingRect = that_p._boundingRect;

  this_p->_biomasses = that_p._biomasses;
  this_p->_reserves = that_p._reserves;

  for (const auto &p: that_p._fruits) {
    FruitData fd = p.second;
    fd.fruit = olookup.at(fd.fruit);
    this_p->_fruits[p.first] = fd;
  }

  assert(that_p._currentStepSeeds.empty());

  this_p->_nextOrganID = that_p._nextOrganID;

  assert(!that_p._killed);
  assert(that_p._dirty.none());

  return this_p;
}

void Plant::save (nlohmann::json &j, const Plant &p) {
  assert(p._currentStepSeeds.empty());
  assert(p._dirty.none());
  assert(!p._killed);

  nlohmann::json jo, jf;
  for (const Organ *o: p._bases) {
    nlohmann::json jo_;
    Organ::save(jo_, *o);
    jo.push_back(jo_);
  }
  for (const auto &p: p._fruits)
    jf.push_back({  p.second.fruit->id(), p.second.genomes  });

  j = {
    p._genome, {p._pos.x, p._pos.y}, p._age,
    jo,
    p._derived, p._reserves,
    jf,
    p._nextOrganID
  };
}

Plant* Plant::load (const nlohmann::json &j) {
  Point pos {j[1][0], j[1][1]};
  Plant *p = new Plant (j[0].get<Genome>(), pos);

  uint i=2;
  p->_age = j[i++];

  nlohmann::json jo = j[i++];
  for (const nlohmann::json &jo_: jo)
    Organ::load(jo_, nullptr, p, p->_organs);

  std::map<OID, Organ*> fruits;
  for (Organ *o: p->_organs) {
    if (o->isFruit()) fruits[o->id()] = o;
    p->assignToViews(o);
  }

  p->_derived = j[i++];
  p->_reserves = j[i++];

  nlohmann::json jf = j[i++];
  for (const nlohmann::json &jf_: jf) {
    Organ *f = fruits.at(jf_[0]);
    p->_fruits.emplace(f->id(), FruitData{jf_[1], f});
  }

  p->_nextOrganID = j[i++];
  p->_killed = false;
  p->_dirty.reset();

  p->updateGeometry();
  p->updateMetabolicValues();

  p->updateDepths();

  return p;
}

void assertEqual (const Plant &lhs, const Plant &rhs, bool deepcopy) {
  using utils::assertEqual;

  assertEqual(lhs._genome, rhs._genome, deepcopy);
  assertEqual(lhs._pos, rhs._pos, deepcopy);
  assertEqual(lhs._age, rhs._age, deepcopy);

  Organ::OID_CMP sorter;

  using P = Organ::OID_CMP;
  using V = Plant::Organs::value_type;
  static_assert(std::is_nothrow_invocable_r<bool, P, const V&, const V&>::value, "No");

  assertEqual(lhs._organs, rhs._organs, sorter, deepcopy);
  assertEqual(lhs._bases, rhs._bases, sorter, deepcopy);
  assertEqual(lhs._hairs, rhs._hairs, sorter, deepcopy);
  assertEqual(lhs._sinks, rhs._sinks, sorter, deepcopy);

  assertEqual(lhs._nonTerminals, rhs._nonTerminals, deepcopy);
  assertEqual(lhs._flowers, rhs._flowers, deepcopy);
  assertEqual(lhs._derived, rhs._derived, deepcopy);
  assertEqual(lhs._boundingRect, rhs._boundingRect, deepcopy);
  assertEqual(lhs._biomasses, rhs._biomasses, deepcopy);
  assertEqual(lhs._reserves, rhs._reserves, deepcopy);
  assertEqual(lhs._fruits, rhs._fruits, deepcopy);
  assertEqual(lhs._currentStepSeeds, rhs._currentStepSeeds, deepcopy);
  assertEqual(lhs._nextOrganID, rhs._nextOrganID, deepcopy);
  assertEqual(lhs._killed, rhs._killed, deepcopy);
}

} // end of namespace simu
