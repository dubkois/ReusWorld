#include "minicgp.h"

/*constexpr*/ cgp::FuncID FID (const std::string_view &s) {
  if (std::tuple_size<cgp::FuncID>::value < s.size())
    utils::doThrow<std::invalid_argument>("Function name '", s, "' is too long");

  cgp::FuncID fid {};
  uint i = 0;
  for (; i<fid.size() - s.size(); i++) fid[i] = '_';
  for (; i<fid.size(); i++) fid[i] = s[i-fid.size()+s.size()];

  std::cerr << s << " >> ";
  for (auto c : fid)  std::cerr << c;
  std::cerr << " >> "
            << std::string(fid.begin(), fid.end())
            << std::endl;

  return fid;
}

template <>
struct PrettyWriter<cgp::FuncID> {
  void operator() (std::ostream &os, const cgp::FuncID &fid) {
    os << std::string(fid.begin(), fid.end());
  }
};

template <>
struct PrettyReader<cgp::FuncID> {
  bool operator() (std::istream &is, cgp::FuncID &fid) {
    std::string str;
    is >> str;
    fid = FID(str);
    return bool(is);
  }
};

namespace config {
#define CFILE CGP

DEFINE_CONTAINER_PARAMETER(CGP::FunctionSet, functionSet, {
  // 0-ary
  FID("rand"), FID("one"),  FID("zero"), FID("pi"),

  // unary
  FID("id"),   FID("abs"),  FID("sq"),   FID("sqrt"), FID("exp"),  FID("sin"),
  FID("cos"),  FID("tan"),  FID("step"),

  // binary
  FID("del"),  FID("div"),

  // ternary
  FID("gss"),

  // n-ary
  FID("add"),  FID("mult")
})

#undef CFILE
} // end of namespace config
