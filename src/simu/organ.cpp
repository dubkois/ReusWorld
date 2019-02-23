#include "organ.h"

#include "plant.h"  /// TODO Remove (for printing plant id)

namespace simu {

using GConfig = genotype::Plant::config_t;

static constexpr bool debugGrowth = false;

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
//  assert(biomass() > 0);
  assert(_requiredBiomass != 0 || f == 1);
  assert(isSeed() || isFruit() || (0 <= f && f <= 1));
  utils::iclip_max(f, 1.f);
  return f;
}

Point rotate_point(float angle, const Point &p) {
  float s = std::sin(angle);
  float c = std::cos(angle);
  return {
    p.x * c - p.y * s,
    p.x * s + p.y * c
  };
}

void Organ::accumulate (float biomass) {
  if (fabs(biomass) < 1e-9) return;

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
    updateBoundingBox();

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

  updateBoundingBox();
  updateGlobalTransformation();

  for (Organ *c: _children) c->updateTransformation();
}

void Organ::updateBoundingBox(void) {
  Point pend = _plantCoordinates.end;
  auto width = _width;
  if (_length == 0 && _width == 0) {
    static const auto S = GConfig::ls_terminalsSizes().at('s').width;
    width = S;
    pend = _plantCoordinates.origin
        + Point::fromPolar(_plantCoordinates.rotation, S);
  }

  Point v = rotate_point(_plantCoordinates.rotation, {0, .5f * width});

  _plantCoordinates.corners = {
    _plantCoordinates.origin + v, pend + v,
    pend - v, _plantCoordinates.origin - v
  };

  Rect &pr = _plantCoordinates.boundingRect;
  pr = Rect::invalid();
  for (const Point &p: _plantCoordinates.corners) {
    pr.ul.x = std::min(pr.ul.x, p.x);
    pr.ul.y = std::max(pr.ul.y, p.y);
    pr.br.x = std::max(pr.br.x, p.x);
    pr.br.y = std::min(pr.br.y, p.y);
  }

  _plantCoordinates.center = pr.center();

  updateGlobalTransformation();
}

void Organ::updateGlobalTransformation(void) {
  const Point &pp = _plant->pos();
  _globalCoordinates.origin = _plantCoordinates.origin + pp;
  _globalCoordinates.center = _plantCoordinates.center + pp;
  _globalCoordinates.boundingRect = _plantCoordinates.boundingRect.translated(pp);

  for (uint i=0; i<4; i++)
    _globalCoordinates.corners[i] = _plantCoordinates.corners[i] + pp;
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
     << OrganID(&o) << "{ " << o._plantCoordinates.origin << "->"
     << o._plantCoordinates.end << ", "
     << o._plantCoordinates.rotation << "}";
  return os << tmpos.rdbuf();
}

void save (nlohmann::json &j, const Organ::PlantCoordinates &c) {
  j = { c.origin, c.end, c.center, c.rotation };
}

void load (const nlohmann::json &j, Organ::PlantCoordinates &c) {
  uint i=0;
  c.origin = j[i++];
  c.end = j[i++];
  c.center = j[i++];
  c.rotation = j[i++];
}

void Organ::save (nlohmann::json &j, const Organ &o) {
  nlohmann::json jpc;
  simu::save(jpc, o._plantCoordinates);

  nlohmann::json jc;
  for (Organ *c: o._children) {
    nlohmann::json jc_;
    save(jc_, *c);
    jc.push_back(jc_);
  }

  j = {
    o._id,
    o._parentCoordinates.rotation,
    jpc,
    o._width, o._length,
    o._symbol, o._layer,
    o._surface,
    o._baseBiomass, o._accumulatedBiomass, o._requiredBiomass,
    jc
  };
}

Organ* Organ::load (const nlohmann::json &j, Organ *parent, Plant *plant,
                    std::set<Organ*> &organs) {

  Organ *o = new Organ(j[0], plant, j[3], j[4], j[1], j[5].get<char>(), j[6], parent);

  simu::load(j[2], o->_plantCoordinates);

  uint i=7;
  o->_surface = j[i++];
  o->_baseBiomass = j[i++];
  o->_accumulatedBiomass = j[i++];
  o->_requiredBiomass = j[i++];

  o->updateBoundingBox();
  o->updateGlobalTransformation();

  organs.insert(o);
  for (const nlohmann::json &jc: j[i++])
    o->_children.insert(load(jc, o, plant, organs));

  return o;
}

} // end of namespace simu
