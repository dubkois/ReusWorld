#include "organ.h"

#include "plant.h"  /// TODO Remove (for printing plant id)

namespace simu {

using GConfig = genotype::Plant::config_t;

static constexpr bool debugGrowth = false;

Organ::Organ (Plant *plant, float w, float l, float r, char c, Layer t,
              Organ *parent)
  : _id(OID::INVALID), _plant(plant), _width(w), _length(l),
    _symbol(c), _layer(t), _cloned(false), _parent(nullptr) {

  _baseBiomass = -1;
  _accumulatedBiomass = 0;

  _parentCoordinates.rotation = r;
  _plantCoordinates = {};
  _globalCoordinates = {};

  _depth = 0;
  updateParent(parent);
}

Organ* Organ::cloneAndUpdate(Organ *newParent, float rotation) {
  Organ *clone = new Organ(_plant, _width, _length,
                           _parentCoordinates.rotation + rotation,
                           _symbol,  _layer, newParent);

  _cloned = true;

  clone->_cloned = true;
  clone->_id = _id;
  clone->_surface = _surface;
  clone->_baseBiomass = _baseBiomass;
  clone->_accumulatedBiomass = _accumulatedBiomass;
  clone->_requiredBiomass = _requiredBiomass;

  clone->updateTransformation();

  for (Organ *c: _children)
    clone->_children.insert(c->cloneAndUpdate(clone, 0));

  return clone;
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
  if (_parent)  _parent->_children.erase(this);
}

void Organ::updateDepth(void) {
  _depth = 0;
  for (Organ *c: _children) {
    c->updateDepth();
    _depth = std::max(_depth, c->_depth + (c->isStructural() ? 1 : 0));
  }

  if (debugGrowth) {
    std::cerr << OrganID(this);
    for (uint d = 0; d<_depth; d++) std::cerr << "  ";
    std::cerr << " Updated depth to " << _depth << std::endl;
  }

//    if (_plant->id() == Plant::ID(9732) && id() == OID(2))
//      std::cerr << "Stop right there!" << std::endl;
}

void Organ::updateParent (Organ *newParent) {
  assert(newParent != this);
  if (_parent != newParent) {
    removeFromParent();
    _parent = newParent;
    if (_parent)  _parent->_children.insert(this);

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

Organ* Organ::clone (const Organ *that_o, Plant *const this_p) {
  Organ *this_o = new Organ (this_p);

  this_o->_id = that_o->_id;

  this_o->_parentCoordinates = that_o->_parentCoordinates;
  this_o->_plantCoordinates = that_o->_plantCoordinates;
  this_o->_globalCoordinates = that_o->_globalCoordinates;

  this_o->_width = that_o->_width;
  this_o->_length = that_o->_length;
  this_o->_symbol = that_o->_symbol;
  this_o->_layer = that_o->_layer;

  assert(!that_o->_cloned);
  this_o->_cloned = false;

  this_o->_parent = that_o->_parent;
  this_o->_children = that_o->_children;

  this_o->_depth = that_o->_depth;

  this_o->_surface = that_o->_surface;
  this_o->_baseBiomass = that_o->_baseBiomass;
  this_o->_accumulatedBiomass = that_o->_accumulatedBiomass;
  this_o->_requiredBiomass = that_o->_requiredBiomass;

  return this_o;
}

void Organ::updatePointers(const std::map<const Organ *, Organ *> &olookup) {
  if (_parent)  _parent = olookup.at(_parent);
  Collection updatedChildren;
  for (Organ *o: _children) updatedChildren.insert(olookup.at(o));
  _children = updatedChildren;
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
                    Collection &organs) {

  Organ *o = new Organ(plant, j[3], j[4], j[1], j[5].get<char>(), j[6], parent);
  o->setID(j[0]);

  simu::load(j[2], o->_plantCoordinates);

  uint i=7;
  o->_surface = j[i++];
  o->_baseBiomass = j[i++];
  o->_accumulatedBiomass = j[i++];
  o->_requiredBiomass = j[i++];

  o->updateBoundingBox();
  o->updateGlobalTransformation();

  organs.insert(o);
  for (const nlohmann::json &jc: j[i++]) {
    Organ *c = load(jc, o, plant, organs);
    o->_children.insert(c);
  }

  return o;
}

void assertEqual (const Organ::ParentCoordinates &lhs,
                  const Organ::ParentCoordinates &rhs, bool deepcopy) {
  utils::assertEqual(lhs.rotation, rhs.rotation, deepcopy);
}

void assertEqual (const Organ::PlantCoordinates &lhs,
                  const Organ::PlantCoordinates &rhs, bool deepcopy) {
  using utils::assertEqual;
  assertEqual(lhs.origin, rhs.origin, deepcopy);
  assertEqual(lhs.end, rhs.end, deepcopy);
  assertEqual(lhs.center, rhs.center, deepcopy);
  assertEqual(lhs.rotation, rhs.rotation, deepcopy);
  assertEqual(lhs.boundingRect, rhs.boundingRect, deepcopy);
  assertEqual(lhs.corners, rhs.corners, deepcopy);
}

void assertEqual (const Organ::GlobalCoordinates &lhs,
                  const Organ::GlobalCoordinates &rhs, bool deepcopy) {
  using utils::assertEqual;
  assertEqual(lhs.origin, rhs.origin, deepcopy);
  assertEqual(lhs.center, rhs.center, deepcopy);
  assertEqual(lhs.boundingRect, rhs.boundingRect, deepcopy);
  assertEqual(lhs.corners, rhs.corners, deepcopy);
}
void assertEqual (const Organ &lhs, const Organ &rhs, bool deepcopy) {
  using utils::assertEqual;

  assertEqual(lhs._id, rhs._id, deepcopy);
  assertEqual(lhs._plant->id(), rhs._plant->id(), deepcopy);
  assertEqual(lhs._parentCoordinates, rhs._parentCoordinates, deepcopy);
  assertEqual(lhs._plantCoordinates, rhs._plantCoordinates, deepcopy);
  assertEqual(lhs._globalCoordinates, rhs._globalCoordinates, deepcopy);
  assertEqual(lhs._width, rhs._width, deepcopy);
  assertEqual(lhs._length, rhs._length, deepcopy);
  assertEqual(lhs._symbol, rhs._symbol, deepcopy);
  assertEqual(lhs._layer, rhs._layer, deepcopy);
  assertEqual(lhs._cloned, rhs._cloned, deepcopy);
  assertEqual(lhs._depth, rhs._depth, deepcopy);
  assertEqual(lhs._surface, rhs._surface, deepcopy);
  assertEqual(lhs._baseBiomass, rhs._baseBiomass, deepcopy);
  assertEqual(lhs._accumulatedBiomass, rhs._accumulatedBiomass, deepcopy);
  assertEqual(lhs._requiredBiomass, rhs._requiredBiomass, deepcopy);

  assertEqual(bool(lhs._parent), bool(rhs._parent), deepcopy);
  if (lhs._parent && rhs._parent)
    assertEqual(lhs._parent->id(), rhs._parent->id(), deepcopy);

  Organ::OID_CMP sorter;
  assertEqual(lhs._children, rhs._children, sorter, deepcopy);
}

} // end of namespace simu
