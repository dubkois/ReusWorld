#ifndef SIMU_PLANT_H
#define SIMU_PLANT_H

#include "../genotype/plant.h"

namespace simu {

struct Point {
  float x, y;

#ifndef NDEBUG
  friend std::ostream& operator<< (std::ostream &os, const Point &p) {
    return os << "{" << p.x << "," << p.y << "}";
  }
#endif

  static Point fromPolar (float a, float r) {
    return {  float(r * cos(a)), float(r * sin(a))  };
  }

  friend Point operator* (const Point &p, float f) {
    return Point{ f * p.x, f * p.y };
  }

  friend Point operator* (float f, const Point &p) {
    return p * f;
  }

  friend Point operator+ (const Point &lhs, const Point &rhs) {
    return { lhs.x + rhs.x, lhs.y + rhs.y };
  }

  friend Point operator- (const Point &lhs, const Point &rhs) {
    return Point { lhs.x - rhs.x, lhs.y - rhs.y };
  }
};

struct Rect {
  Point ul, br;

  static Rect invalid (void) {
    using L = std::numeric_limits<float>;
    return { { L::max(), -L::max() }, { -L::max(), L::max() } };
  }

  Rect& uniteWith (const Rect &that) {
    ul.x = std::min(ul.x, that.ul.x);
    ul.y = std::max(ul.y, that.ul.y);
    br.x = std::max(br.x, that.br.x);
    br.y = std::min(br.y, that.br.y);
    return *this;
  }
};

struct Position {
  Point start, end;
  float rotation;
};

class Organ {
public:
  using LSType = genotype::grammar::LSystemType;

private:
  float _localRotation;   // Parent coordinate
  Position _global;       // Plant coordinates
  float _width, _length;  // Constant for now
  char _symbol;           // To choose color and shape
  LSType _type;           // To check altitude and symbol

  Organ *_parent;
  std::set<Organ*> _children;

  Rect _boundingRect;

public:
  Organ (float w, float l, float r, char c, LSType t, Organ *p = nullptr);

  void updateCache (void);
  void updateParent (Organ *newParent);

  void removeFromParent(void);

  float localRotation (void) const {  return _localRotation;  }
  const auto& globalCoordinates (void) const {  return _global; }

  const auto& boundingRect (void) const { return _boundingRect; }

  auto width (void) const {   return _width;  }
  auto length (void) const {  return _length; }

  auto symbol (void) const {  return _symbol; }
  auto type (void) const {    return _type;   }

  auto parent (void) {  return _parent; }
  const auto parent (void) const {  return _parent; }

  const auto& children (void) const { return _children; }

  bool isNonTerminal (void) const {
    return genotype::grammar::Rule_base::isValidNonTerminal(_symbol);
  }
  bool isLeaf (void) const {  return _symbol == 'l'; }
  bool isHair (void) const {  return _symbol == 'h'; }

#ifndef NDEBUG
  friend std::ostream& operator<< (std::ostream &os, const Organ &o);
#endif
};

class Plant {
  using Genome = genotype::Plant;
  using LSType = Organ::LSType;

  Genome _genome;
  Point _pos;

  uint _age;

  using Organs = std::set<Organ*>;
  Organs _organs;

  using OrgansView = std::set<Organ*>;
  OrgansView _bases, _nonTerminals, _leaves, _hairs;

  uint _derived;  ///< Number of times derivation rules were applied

  Rect _boundingRect;

public:
  Plant(const Genome &g, float x, float y);
  ~Plant (void);

  const Point& pos (void) const {
    return _pos;
  }

  const auto& organs (void) const {
    return _organs;
  }

  float age (void) const;

  const auto& genome (void) const {
    return _genome;
  }

  const auto& boundingRect (void) const {
    return _boundingRect;
  }

  void step (void);

  bool isDead (void) const {
    return false;
  }

  uint deriveRules(void);

  std::string toString (LSType type) const;

  friend std::ostream& operator<< (std::ostream &os, const Plant &p) {
    return os << "P{\n"
              << "\tshoot: " << p.toString(LSType::SHOOT) << "\n"
              << "\t root: " << p.toString(LSType::ROOT) << "\n}";
  }

  static float initialAngle (LSType t) {
    switch (t) {
    case LSType::SHOOT: return  M_PI/2.;
    case LSType::ROOT:  return -M_PI/2.;
    }
    utils::doThrow<std::logic_error>("Invalid lsystem type", t);
    return 0;
  }

private:
  Organ *addOrgan (Organ *parent, float width, float length, float angle,
                   char symbol, LSType type);

  void delOrgan (Organ *o);

  Organ* turtleParse (Organ *parent, const std::string &successor, float &angle,
                      LSType type,
                      const genotype::grammar::Checkers &checkers);
};

} // end of namespace simu

#endif // SIMU_PLANT_H
