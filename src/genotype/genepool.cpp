#include "genepool.h"

#include "../simu/simulation.h"

namespace misc {

static constexpr bool debugGenepool = false;

using Simulation = simu::Simulation;

template <typename T>
std::string toKey (const T &v) {
  std::ostringstream oss;
  oss << v;
  return oss.str();
}

std::string toKey (float f) { return toKey(int(f)); }
std::string toKey (double d) { return toKey(int(d)); }

template <typename H, typename R>
void insertAllRules (H &histograms, const R &rules, const std::string &prefix) {

  using namespace genotype::grammar;

  for (Symbol s: Rule_base::nonTerminals()) {
    auto it = rules.find(s);

    std::string key ("");
    if (it != rules.end())  key = it->second.rhs;

    histograms[prefix + s][key]++;
  }
}

template <typename H, typename G>
void addGenome (H &histograms, const G &g) {
#define NINSERT(N,X) histograms[N][toKey(X)]++;
#define INSERT(X) NINSERT(#X, g.X)

  NINSERT("cdata.optimalDistance", 10*g.cdata.getOptimalDistance())
  NINSERT("cdata.inbreedTolerance", 10*g.cdata.getInbreedTolerance())
  NINSERT("cdata.outbreedTolerance", 10*g.cdata.getOutbreedTolerance())

  INSERT(shoot.recursivity)
  INSERT(root.recursivity)

  insertAllRules(histograms, g.shoot.rules, "shoot.");
  insertAllRules(histograms, g.root.rules, "root.");

//  for (const auto &p: g.shoot.rules)
//    NINSERT(std::string("shoot.") + p.second.lhs, p.second.rhs);

//  for (const auto &p: g.root.rules)
//    NINSERT(std::string("root.") + p.second.lhs, p.second.rhs);

  NINSERT("metabolism.resistors.G", 10*g.metabolism.resistors[0])
  NINSERT("metabolism.resistors.W", 10*g.metabolism.resistors[1])
  NINSERT("metabolism.growthSpeed", 10*g.metabolism.growthSpeed)
  NINSERT("metabolism.deltaWidth", 100*g.metabolism.deltaWidth)

  INSERT(dethklok)
  NINSERT("fruitOvershoot", 10*g.fruitOvershoot)
  INSERT(seedsPerFruit)
  NINSERT("temperatureOptimal", 10*g.temperatureOptimal)
  NINSERT("temperatureRange", 10*g.temperatureRange)
  NINSERT("structuralLength", 10*g.structuralLength)

#undef INSERT
#undef NINSERT
}

void GenePool::parse (const Simulation &s) {
  auto start = Simulation::clock::now();

  histograms.clear();
  for (const auto &p: s.plants())
    addGenome(histograms, p.second->genome());

  float N = s.plants().size();
  for (auto &hist: histograms) for (auto &bin: hist.second) bin.second /= N;

  if (debugGenepool)
    std::cout << *this << "\nParsed in " << Simulation::duration(start)
              << " ms" << std::endl;
}

struct Pad {
  size_t w;

  Pad (size_t w) : w(std::max(size_t(0), w)) {}

  friend std::ostream& operator<< (std::ostream &os, const Pad &p) {
    return os << std::setw(p.w);
  }
};

double divergence (const GenePool &lhs, const GenePool &rhs) {

  std::ostream &os = std::cout;

  size_t maxKeyWidth = 0;
  for (const auto &hist: lhs.histograms)
    for (const auto &bin: hist.second)
      maxKeyWidth = std::max(maxKeyWidth, bin.first.size());
  for (const auto &hist: rhs.histograms)
    for (const auto &bin: hist.second)
      maxKeyWidth = std::max(maxKeyWidth, bin.first.size());

  Pad kpad (maxKeyWidth+1), pad (30);

  const auto debugAlign =
    [&os, &kpad, &pad]
    (const std::string &k, float lhs, float rhs, float diff) {
      if (debugGenepool) {
      os << kpad << k;
      if (std::isnan(lhs))
            os << pad << "";
      else  os << pad << lhs;
      if (std::isnan(rhs))
            os << pad << "";
      else  os << pad << rhs;

      if (std::isnan(lhs))      diff = rhs;
      else if (std::isnan(rhs)) diff = lhs;

      os << pad << "+" << diff;

      os << "\n";
    }
  };

  if (debugGenepool)
    os << std::string(80, '-') << "\n-- Aligning --" << std::endl;

  double totalDiff = 0;

  // For every genetic field
  for (auto itFL = lhs.histograms.begin(), itFR = rhs.histograms.begin();
       itFL != lhs.histograms.end(); ++itFL, ++itFR) {

    if (debugGenepool)
      os << itFL->first << ":\n";

    double localDiff = 0;

    // For every bin -> align
    const auto &hL = itFL->second, hR = itFR->second;
    auto itL = hL.begin(), itR = hR.begin();
    while (itL != hL.end() && itR != hR.end()) {

      const auto &kL = itL->first, kR = itR->first;
      const auto &fL = itL->second, fR = itR->second;
      if (kL == kR) { // Match
        float diff = std::fabs(fL - fR);

        if (debugGenepool)  debugAlign(kL, fL, fR, diff);

        localDiff += diff;
        ++itL; ++itR;

      } else if (kL < kR) { // Left mismatch
        if (debugGenepool)  debugAlign(kL, fL, NAN, NAN);

        localDiff += fL;
        ++itL;

      } else {  // Right mismatch
        if (debugGenepool)  debugAlign(kR, NAN, fR, NAN);

        localDiff += fR;
        ++itR;
      }
    }

    while (itL != hL.end()) {
      debugAlign(itL->first, itL->second, NAN, NAN);
      localDiff += itL->second;
      ++itL;
    }

    while (itR != hR.end()) {
      debugAlign(itR->first, NAN, itR->second, NAN);
      localDiff += itR->second;
      ++itR;
    }

    if (debugGenepool)
      os << ">>>> Local diff: " << localDiff << std::endl;
    totalDiff += localDiff;
  }

  if (debugGenepool)
    os << ">> Total diff: " << totalDiff << std::endl;

  return totalDiff / 2.f;
}

std::ostream& operator<< (std::ostream &os, const GenePool &gp) {
//  os << std::setfill('-');
  size_t maxKeyWidth = 0;
  for (const auto &hist: gp.histograms)
    for (const auto &bin: hist.second)
      maxKeyWidth = std::max(maxKeyWidth, bin.first.size());
  maxKeyWidth++;

  for (const auto &hist: gp.histograms) {
    os << hist.first << ":\n";
    for (const auto &bin: hist.second)
      os << std::setw(maxKeyWidth) << bin.first << ": " << bin.second << "\n";
  }
  return os;
}

} // end of namespace misc
