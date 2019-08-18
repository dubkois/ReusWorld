#include "minicgp.h"

namespace simu {

void save (nlohmann::json &j, const rng::FastDice &d) {
  std::ostringstream oss;
  serialize(oss, d);
  j = oss.str();
}

void load (const nlohmann::json &j, rng::FastDice &d) {
  std::istringstream iss (j.get<std::string>());
  deserialize(iss, d);
}

} // end of namespace simu

namespace cgp::functions {
std::ostream& operator<< (std::ostream &os, const ID &id) {
  for (auto c: id)  os << c;
  return os;
}

std::istream& operator>> (std::istream &is, ID &id) {
  std::string str;
  is >> str;
  id = FID(str);
  return is;
}

} // end of namespace cgp::functions

template <>
struct PrettyWriter<cgp::FuncID> {
  void operator() (std::ostream &os, const cgp::FuncID &fid) {
    using cgp::functions::operator<<;
    os << fid;
  }
};

template <>
struct PrettyReader<cgp::FuncID> {
  bool operator() (std::istream &is, cgp::FuncID &fid) {
    using cgp::functions::operator>>;
    return bool(is >> fid);
  }
};

namespace config {
#define CFILE CGP
using cgp::functions::FID;

DEFINE_CONTAINER_PARAMETER(CGP::FunctionSet, functionSet, {
  // 0-ary
  FID("mone"),  FID("zero"), FID("one"), FID("rand"),

  // unary
  FID("id"),   FID("abs"),  FID("sq"),   FID("sqrt"), FID("exp"),
  FID("sin"),  FID("cos"),  FID("tanh"), FID("asin"), FID("acos"),
  FID("step"), FID("inv"),  FID("rond"), FID("flor"), FID("ceil"),

  // binary
  FID("del"),  FID("gt"),   FID("lt"),   FID("eq"),   FID("hgss"),

  // n-ary
  FID("add"),  FID("mult"), FID("min"),  FID("max")
})

#undef CFILE
} // end of namespace config
