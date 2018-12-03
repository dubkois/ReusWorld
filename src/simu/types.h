#ifndef SIMU_TYPES_H
#define SIMU_TYPES_H

#include <cmath>

#ifndef NDEBUG
#include <ostream>
#endif

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

  Point& operator+= (const Point &that) {
    x += that.x;
    y += that.y;
    return *this;
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

  auto r (void) const { return br.x;  }
  auto t (void) const { return ul.y;  }
  auto l (void) const { return ul.x;  }
  auto b (void) const { return br.y;  }

  auto width (void) const {
    return br.x - ul.x;
  }

  auto height (void) const {
    return ul.y - br.y;
  }

  auto center (void) const {
    return Point{ .5f * (ul.x + br.x), .5f * (ul.y + br.y) };
  }

  Rect translated (const Point &p) const {
    return { { ul.x + p.x, ul.y + p.y }, { br.x + p.x, br.y + p.y } };
  }

#ifndef NDEBUG
  friend std::ostream& operator<< (std::ostream &os, const Rect &r) {
    return os << "{ " << r.ul << ", " << r.br << " }";
#endif
  }
};

struct Disk {
  Point center;
  float radius;
};

struct Position {
  Point start, end;
  float rotation;
};

} // end of namespace simu

#endif // SIMU_TYPES_H
