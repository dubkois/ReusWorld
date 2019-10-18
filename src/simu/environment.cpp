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

void Environment::init (const Genome &genome) {
  _genomes = {genome};
  updateInternals();
  initInternal();
}

void Environment::initFrom(const std::vector<Environment *> &these) {
  // Prepare genomes container
  uint t = 0;
  for (const Environment *e: these) {
    assert(e->genomes().size() == 1);
    _genomes.push_back(e->genomes().front());
    t = std::max(t, e->time().toTimestamp());
  }

  // Prepare internal variables
  updateInternals();
  initInternal();

  _startTime = _currTime = _endTime = Time::fromTimestamp(t);

  // Copy contents (i.e. voxels)
  uint offset = 0;
  for (uint i=0; i<these.size(); i++) {
    const Environment &e = *these[i];

    bool first = (i==0), last = (i+1==these.size());
    uint stride = _genomes[i].voxels;

    const auto copyInto = [&offset, &stride, &first, &last]
      (const auto src, auto &dst) {
        auto beg = src.begin(), end = src.end();

        // Left-most voxel is shared, handle manually
        if (!first) beg = std::next(beg);

        // Right-most voxel is shared, handle manually
        if (!last)  end = std::prev(end);

        std::copy(beg, end, std::begin(dst)+offset+!first);

        if (!first) dst[offset] += .5 * src.front();
        if (!last)  dst[offset+stride] = .5 * src.back();
    };

    copyInto(e._topology, _topology);
    copyInto(e._temperature, _temperature);
    copyInto(e._hygrometry[SHALLOW], _hygrometry[SHALLOW]);
    copyInto(e._grazing, _grazing);

    offset += stride;
  }
}

void Environment::updateInternals(void) {
  _totalWidth = 0;
  _depth = 0;

  _minTemperature = std::numeric_limits<float>::max();
  _maxTemperature = -std::numeric_limits<float>::max();

  _voxels = 0;

  for (Genome &g: _genomes) {
    _totalWidth += g.width;
    _depth = std::max(_depth, g.depth);
    _minTemperature = std::min(_minTemperature, g.minT);
    _maxTemperature = std::max(_maxTemperature, g.maxT);

    _voxels += g.voxels;

    g.controller.prepare();
  }

  if (_genomes.size() > 1)
    _voxels = _voxels + 1 - _genomes.size();
}

void Environment::initInternal(void) {
  _topology.resize(_voxels+1, 0.f);
  _temperature.resize(_voxels+1, .5 * (_maxTemperature + _minTemperature));

  _hygrometry[UndergroundLayers::SHALLOW].resize(_voxels+1,
                                                 config::Simulation::baselineShallowWater());
  _hygrometry[UndergroundLayers::DEEP].resize(_voxels+1, 1.f);

  _grazing.resize(_voxels+1, 0.f);

  _dice.reset(_genomes.front().rngSeed); /// TODO Really okay?
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

  using I = genotype::cgp::Inputs;
  using Inputs = CGP::Inputs;
  Inputs inputs (Genome::config_t::activeInputs(), 0);

  using O = genotype::cgp::Outputs;
  using Outputs = CGP::Outputs;
  Outputs outputs (Genome::config_t::activeOutputs(), 0);

  const auto baselineWater = config::Simulation::baselineShallowWater();

  _updatedTopology =
    (_currTime.toTimestamp() % config::Simulation::updateTopologyEvery()) == 0;

  inputs[I::D] = sin(2 * M_PI * _currTime.timeOfYear());

  const auto S = _startTime.toTimestamp(), E = _endTime.toTimestamp();
  inputs[I::Y] = sin(2 * M_PI * (1. - (E - _currTime.toTimestamp()) / (E - S)));

  for (I i: {I::D, I::Y})
    utils::iclip(-1., inputs[i], 1.);

  uint offset = 0;
  std::array<float, 4> junctions;
  junctions.fill(NAN);

  for (uint i=0; i<_genomes.size(); i++) {
    Genome &g = _genomes[i];

    for (uint j=0; j<=g.voxels; j++) {
      uint j_ = j+offset;

      float &A = _topology[j_];
      float &T = _temperature[j_];
      float &H = _hygrometry[SHALLOW][j_];
      float &G = _grazing[j_];

      auto rangeT = g.maxT - g.minT;

      inputs[I::X] = 2 * float(j) / g.voxels - 1;
      inputs[I::T] = A / g.depth;
      inputs[I::H] = 2 * (T - g.minT) / rangeT - 1;
      inputs[I::W] = H / config::Simulation::baselineShallowWater() - 1;
      inputs[I::G] = G;

      for (I i: {I::X, I::T, I::H, I::W, I::G})
        utils::iclip(-1., inputs[i], 1.);

      g.controller.evaluate(inputs, outputs);

      float A_ = .9 * outputs[O::T] * g.depth,
            T_ = .5 * (outputs[O::H] + 1) * rangeT + g.minT,
            H_ = (1 + outputs[O::W]) * baselineWater,
            G_ = std::fabs(outputs[O::G]);

      bool isRight = (i+1 < _genomes.size() && j == g.voxels);
      if (isRight) {
        junctions[0] = .5 * A_;
        junctions[1] = .5 * T_;
        junctions[2] = .5 * H_;
        junctions[3] = .5 * G_;

      } else {
        bool isLeft = (i > 0 && j == 0);
        if (isLeft) {
          A_ = .5 * A_ + junctions[0];
          T_ = .5 * T_ + junctions[1];
          H_ = .5 * H_ + junctions[2];
          G_ = .5 * G_ + junctions[3];
        }

        // Keep 10% of space
        if (_updatedTopology)
          updateVoxel(g, A, A_);

        updateVoxel(g, T, T_);

        updateVoxel(g, H, H_);

        updateVoxel(g, G, G_);
      }
    }

    offset += g.voxels;
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

void Environment::updateVoxel (const Genome &g, float &voxel, float newValue) {
  voxel = g.inertia * voxel + (1.f - g.inertia) * newValue;
}

float Environment::interpolate(const Voxels &voxels, float x) const {
  float v = (x + xextent()) * float(_voxels) / width();
  uint v0 = uint(v);
  float d = v - v0;

  assert(v0 <= voxels.size());

  if (v0 == _voxels)
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
  _genomes = e._genomes;
  _dice = e._dice;

  _topology = e._topology;
  _temperature = e._temperature;
  _hygrometry = e._hygrometry;
  _grazing = e._grazing;

  _updatedTopology = false;

  _startTime = e._startTime;
  _currTime = e._currTime;
  _endTime = e._endTime;

  _physics->clone(*e._physics, plookup, olookups);
}

void Environment::save (nlohmann::json &j, const Environment &e) {
  if (e._genomes.size() > 1) { // Multigenomes save
    j= {

    };

  } else {
    nlohmann::json jd, jc;
    simu::save(jd, e._dice);
    Genome::CGP::save(jc, e._genomes.front().controller);
    j = {
      e._genomes.front(), jd,
      e._topology, e._temperature, e._hygrometry, e._grazing,
      e._startTime, e._currTime, e._endTime,
      jc
    };
  }
}

void Environment::postLoad(void) {  _physics->postLoad(); }

void Environment::load (const nlohmann::json &j, Environment &e) {
  if (j[0].is_array()) { // got multiple genomes to load

  } else {
    uint i=0;
    e._genomes.push_back(j[i++]);
    simu::load(j[i++], e._dice);
    e._topology = j[i++].get<Voxels>();
    e._temperature = j[i++].get<Voxels>();
    e._hygrometry = j[i++];
    e._grazing = j[i++].get<Voxels>();
    e._startTime = j[i++];
    e._currTime = j[i++];
    e._endTime = j[i++];

    Genome::CGP::load(j[i++], e._genomes.front().controller);
  }

  e.updateInternals();
  e._updatedTopology = false;

  if (debugEnvCTRL) e.showVoxelsContents();
}

void assertEqual (const Environment &lhs, const Environment &rhs,
                  bool deepcopy) {

  using utils::assertEqual;
  assertEqual(lhs._genomes, rhs._genomes, deepcopy);
  assertEqual(lhs._dice, rhs._dice, deepcopy);
  assertEqual(lhs._topology, rhs._topology, deepcopy);
  assertEqual(lhs._temperature, rhs._temperature, deepcopy);
  assertEqual(lhs._hygrometry, rhs._hygrometry, deepcopy);
  assertEqual(lhs._grazing, rhs._grazing, deepcopy);
  assertEqual(*lhs._physics, *rhs._physics, deepcopy);
}

void Environment::showVoxelsContents(void) const {
  std::cerr << "\tTopology:";
  for (uint i=0; i<=_voxels; i++)
    std::cerr << " " << _topology[i];
  std::cerr << "\n\tTemperature:";
  for (uint i=0; i<=_voxels; i++)
    std::cerr << " " << _temperature[i];
  std::cerr << "\n\tHygrometry:";
  for (uint i=0; i<=_voxels; i++)
    std::cerr << " " << _hygrometry[SHALLOW][i];
  std::cerr << "\n\tGrazing:";
  for (uint i=0; i<=_voxels; i++)
    std::cerr << " " << _grazing[i];
  std::cerr << std::endl;
}

} // end of namespace simu
