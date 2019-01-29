#include "types.h"

#include "../config/simuconfig.h"

namespace simu {

const auto& Y (void) {  return config::Simulation::stopAtYear();  }
const auto& D (void) {  return config::Simulation::daysPerYear(); }
const auto& H (void) {  return config::Simulation::stepsPerDay(); }

Time::Time (void) : Time(0,0,0) {}

Time& Time::next (void) {
  _hour++;
  if (_hour == H()) _hour = 0,  _day++;
  if (_day == D())  _day = 0,   _year++;
  return *this;
}

std::string Time::pretty (void) const {
  static const int Y_digits = log10(Y());
  static const int D_digits = log10(D());
  static const int H_digits = log10(H());

  std::ostringstream oss;
  oss << std::setfill('0')
      << "y" << std::setw(Y_digits) << _year
      << "d" << std::setw(D_digits) << _day
      << "h" << std::setw(H_digits) << _hour;
  return oss.str();
}

const Time& Time::startOf (void) {
  static const Time FIRST = Time();
  return FIRST;
}

bool Time::isStartOf (void) const {
  return *this == startOf();
}

const Time& Time::endOf (void) {
  static const Time END { Y(), 0, 0 };
  return END;
}

bool Time::isEndOf (void) const {
  return *this == endOf();
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

bool operator== (const Time &lhs, const Time &rhs) {
  return lhs._year == rhs._year && lhs._day == rhs._day && lhs._hour == rhs._hour;
}

std::ostream& operator<< (std::ostream &os, const Time &t) {
  return os << "y" << t._year << "d" << t._day << "h" << t._hour;
}

std::istream& operator>> (std::istream& is, Time &t) {
  char c;
  is >> c >> t._year >> c >> t._day >> c >> t._hour;
  return is;
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

} // end of namespace simu
