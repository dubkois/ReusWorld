#include "kgd/utils/utils.h"

#include "types.h"

#include "../config/simuconfig.h"

namespace simu {

const auto& D (void) {  return config::Simulation::daysPerYear(); }
const auto& H (void) {  return config::Simulation::stepsPerDay(); }

Time::Time (void) : Time(0,0,0) {}

float Time::timeOfDay (void) const {  return float(_hour) / H(); }
float Time::timeOfYear (void) const {  return float(_day) / D(); }

Time& Time::next (void) {
  _hour++;
  if (_hour == H()) _hour = 0,  _day++;
  if (_day == D())  _day = 0,   _year++;
  return *this;
}

std::string Time::pretty (void) const {
  static const int D_digits = std::ceil(log10(D()));
  static const int H_digits = std::ceil(log10(H()));

  std::ostringstream oss;
  oss << std::setfill('0')
      << "y" << _year
      << "d" << std::setw(D_digits) << _day
      << "h" << std::setw(H_digits) << _hour;
  return oss.str();
}

Time Time::fromTimestamp(uint step) {
  Time t;
  t._hour = step % H(); step /= H();
  t._day = step % D();  step /= D();
  t._year = step;
  return t;
}

uint Time::toTimestamp(void) const {
  return _hour + H() * (_day + _year * D());
}

void to_json (nlohmann::json &j, const Time &t) {
  std::ostringstream oss;
  oss << t;
  j = oss.str();
}

void from_json (const nlohmann::json &j, Time &t) {
  std::istringstream iss (j.get<std::string>());
  iss >> t;
}

void assertEqual (const Time &lhs, const Time &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.hour(), rhs.hour());
  assertEqual(lhs.day(), rhs.day());
  assertEqual(lhs.year(), rhs.year());
}

void assertEqual (const Point &lhs, const Point &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.x, rhs.x);
  assertEqual(lhs.y, rhs.y);
}

void assertEqual (const Rect &lhs, const Rect &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.br, rhs.br);
  assertEqual(lhs.ul, rhs.ul);
}


void assertEqual (const Disk &lhs, const Disk &rhs) {
  using utils::assertEqual;
  assertEqual(lhs.center, rhs.center);
  assertEqual(lhs.radius, rhs.radius);
}

} // end of namespace simu
