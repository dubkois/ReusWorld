#include "environment.h"
#include "tiniestphysicsengine.h"

#include "../config/simuconfig.h"

namespace simu {

static constexpr int debugEnvCTRL = 0;

static constexpr auto DURATION_FORMAT = "Format is [+|=]<years>";

using UL_EU = EnumUtils<UndergroundLayers>;

// =============================================================================

Environment::Environment(void)
  : _physics(std::make_unique<physics::TinyPhysicsEngine>()) {}

Environment::~Environment(void) {}

void Environment::init (const Genome &g) {
  _genome = g;
  _genome.controller.prepare();

  _topology.resize(_genome.voxels+1, 0.f);
  _temperature.resize(_genome.voxels+1, .5 * (_genome.maxT + _genome.minT));

  _hygrometry[UndergroundLayers::SHALLOW].resize(_genome.voxels+1,
                                                 config::Simulation::baselineShallowWater());
  _hygrometry[UndergroundLayers::DEEP].resize(_genome.voxels+1, 1.f);

  _dice.reset(_genome.rngSeed);
  _updatedTopology = false;

  _startTime = _currTime = _endTime = Time();
}

void Environment::destroy (void) {
  _physics->reset();
}

void Environment::stepStart(void) {
#ifdef DEBUG_COLLISIONS
  for (auto &p: _physics->collisionsDebugData)
    p.second.clear();
#endif
}

void Environment::stepEnd (void) {
  cgpStep();

#if 0
#warning debugging physics
  _physics->debug();
#endif

  _currTime.next();
}

void Environment::setDuration(DurationSetType type, uint years) {
  _startTime = _currTime;
  switch (type) {
  case APPEND:  _endTime = _currTime + years; break;
  case SET:     _endTime.set(years, 0, 0);    break;
  default:
    utils::doThrow<std::invalid_argument>(
      "Invalid duration specifier '", type, "'.");
  }
}

void Environment::cgpStep (void) {
  using CGP = Genome::CGP;

  using I = genotype::env_controller::Inputs;
  using Inputs = CGP::Inputs;
  Inputs inputs;

  using O = genotype::env_controller::Outputs;
  using Outputs = CGP::Outputs;
  Outputs outputs;

  _updatedTopology = _currTime.isStartOfYear()
      && (_currTime.year() % config::Simulation::updateTopologyEvery()) == 0;

  inputs[I::D] = sin(2 * M_PI * _currTime.timeOfYear());

  const auto S = _startTime.toTimestamp(), E = _endTime.toTimestamp();
  inputs[I::Y] = sin(2 * M_PI * (1. - (E - _currTime.toTimestamp()) / (E - S)));

  for (uint i=0; i<=_genome.voxels; i++) {
    float &A = _topology[i];
    float &T = _temperature[i];
    float &H = _hygrometry[SHALLOW][i];

    inputs[I::X] = 2 * float(i) / _genome.voxels - 1;
    inputs[I::T] = A / _genome.depth;
    inputs[I::H] = 2 * (T - _genome.minT) / (_genome.maxT - _genome.minT) - 1;
    inputs[I::W] = H / config::Simulation::baselineShallowWater() - 1;

    _genome.controller.evaluate(inputs, outputs);

    // Keep 10% of space
    if (_updatedTopology)
      A = .9 * outputs[O::T_] * _genome.depth;

    T = .5 * (outputs[O::H_] + 1) * (_genome.maxT - _genome.minT) + _genome.minT;
    H = (1 + outputs[O::W_]) * config::Simulation::baselineShallowWater();
  }

  if (debugEnvCTRL) showVoxelsContents();
}

float Environment::heightAt(float x) const {
  if (!insideXRange(x)) return 0;
  return interpolate(_topology, x);
}

float Environment::temperatureAt(float x) const {
  if (!insideXRange(x)) return 0;
  return interpolate(_temperature, x);
}

float Environment::waterAt(const Point &p) const {
  if (!inside(p)) return 0; // No water outside world

  float h = heightAt(p.x);
  if (h < p.y)  return 0; // No water above ground

  float d = (h - p.y) / (h + yextent());
  assert(0 <= d && d <= 1);
  return interpolate(_hygrometry[SHALLOW], p.x) * (1.f - uint(d))
       + interpolate(_hygrometry[DEEP], p.x) * d;
}

float Environment::lightAt(float) const {
  return config::Simulation::baselineLight();
}

float Environment::interpolate(const Voxels &voxels, float x) const {
  float v = (x + xextent()) * float(_genome.voxels) / width();
  uint v0 = uint(v);
  float d = v - v0;

  assert(v0 <= _genome.voxels);

  if (v0 == _genome.voxels)
        return voxels[v0];
  else  return voxels[v0] * (1.f - d) + voxels[v0+1] * d;
}

const physics::UpperLayer::Items& Environment::canopy(const Plant *p) const {
  return _physics->canopy(p);
}

bool Environment::addCollisionData(Plant *p) {
  return _physics->addCollisionData(*this, p);
}

void Environment::updateCollisionData(Plant *p) {
  _physics->updateCollisions(p);
}

void Environment::updateCollisionDataFinal(Plant *p) {
  _physics->updateFinal(*this, p);
}

void Environment::removeCollisionData(Plant *p) {
  _physics->removeCollisionData(p);
}

physics::CollisionResult
Environment::initialCollisionTest (const Plant *plant) const {
  return _physics->initialCollisionTest(plant);
}

physics::CollisionResult
Environment::collisionTest (const Plant *plant,
                            const Branch &self, const Branch &branch,
                            const std::set<Organ*> &newOrgans) const {
  return _physics->collisionTest(plant, self, branch, newOrgans);
}

void Environment::disseminateGeneticMaterial(Organ *f) {
  _physics->addPistil(f);
}

void Environment::updateGeneticMaterial(Organ *f, const Point &oldPos) {
  _physics->updatePistil(f, oldPos);
}

void Environment::removeGeneticMaterial(Organ *f) {
  _physics->delPistil(f);
}

physics::Pistil Environment::collectGeneticMaterial(Organ *f) {
  auto itP = _physics->sporesInRange(f);

  if (std::distance(itP.first, itP.second) >= 1)
    return *dice()(itP.first, itP.second);

  else
    return physics::Pistil();
}

void Environment::processNewObjects(void) {
  _physics->processNewObjects();
}

// =============================================================================
// == Binary serialization

void Environment::clone (const Environment &e,
                         const std::map<const Plant *, Plant *> &plookup,
                         const std::map<const Plant*,
                                        std::map<const Organ*,
                                                 Organ*>> &olookups) {
  _genome = e._genome;
  _dice = e._dice;

  _topology = e._topology;
  _temperature = e._temperature;
  _hygrometry = e._hygrometry;

  _updatedTopology = false;

  _startTime = e._startTime;
  _currTime = e._currTime;
  _endTime = e._endTime;

  _physics->clone(*e._physics, plookup, olookups);
}

void Environment::save (nlohmann::json &j, const Environment &e) {
  nlohmann::json jd, jc;
  simu::save(jd, e._dice);
  Genome::CGP::save(jc, e._genome.controller);
  j = {
    e._genome, jd,
    e._topology, e._temperature, e._hygrometry,
    e._startTime, e._currTime, e._endTime,
    jc
  };
}

void Environment::postLoad(void) {  _physics->postLoad(); }

void Environment::load (const nlohmann::json &j, Environment &e) {
  uint i=0;
  e._genome = j[i++];
  simu::load(j[i++], e._dice);
  e._topology = j[i++].get<Voxels>();
  e._temperature = j[i++].get<Voxels>();
  e._hygrometry = j[i++];
  e._startTime = j[i++];
  e._currTime = j[i++];
  e._endTime = j[i++];

  Genome::CGP::load(j[i++], e._genome.controller);

  e._updatedTopology = false;

  if (debugEnvCTRL) e.showVoxelsContents();
}

void assertEqual (const Environment &lhs, const Environment &rhs,
                  bool deepcopy) {

  using utils::assertEqual;
  assertEqual(lhs._genome, rhs._genome, deepcopy);
  assertEqual(lhs._dice, rhs._dice, deepcopy);
  assertEqual(lhs._topology, rhs._topology, deepcopy);
  assertEqual(lhs._temperature, rhs._temperature, deepcopy);
  assertEqual(lhs._hygrometry, rhs._hygrometry, deepcopy);
  assertEqual(*lhs._physics, *rhs._physics, deepcopy);
}


void Environment::showVoxelsContents(void) const {
  std::cerr << "\tTopology:";
  for (uint i=0; i<=_genome.voxels; i++)
    std::cerr << " " << _topology[i];
  std::cerr << "\n\tTemperature:";
  for (uint i=0; i<=_genome.voxels; i++)
    std::cerr << " " << _temperature[i];
  std::cerr << "\n\tHygrometry:";
  for (uint i=0; i<=_genome.voxels; i++)
    std::cerr << " " << _hygrometry[SHALLOW][i];
  std::cerr << std::endl;
}

} // end of namespace simu
