#include "visuconfig.h"

std::ostream& operator<< (std::ostream &os, const QSize &s) {
  return os << s.width() << "x" << s.height();
}

std::istream& operator>> (std::istream &is, QSize &s) {
  char junk;
  int width, height;
  is >> width >> junk >> height;
  s.setWidth(width), s.setHeight(height);
  return is;
}

void to_json (nlohmann::json &j, const QSize &s) {
  j = { s.width(), s.height() };
}

void from_json (const nlohmann::json &j, QSize &s) {
  s = QSize(j[0], j[1]);
}

namespace config {
#define CFILE Visualization
DEFINE_SUBCONFIG(Simulation, simuConfig)

DEFINE_PARAMETER(bool, withScreenshots, false)
DEFINE_PARAMETER(QSize, screenshotResolution, QSize(0,0))
#undef CFILE

} // end of namespace config
