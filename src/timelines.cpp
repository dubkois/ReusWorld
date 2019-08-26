#include <csignal>

#include <numeric>

#include <omp.h>

#include "kgd/external/cxxopts.hpp"

#include "simu/simulation.h"
#include "config/dependencies.h"

//#define TEST 1
#ifdef TEST
#warning Test mode activated for timelines executable
#endif

static constexpr bool debugGenepool = false;

bool isValidSeed(const std::string& s) {
  return !s.empty()
    && std::all_of(s.begin(),
                   s.end(),
                   [](char c) { return std::isdigit(c); }
    );
}

using Plant = simu::Plant;
using Simulation = simu::Simulation;

struct GenePool {
  static constexpr int W = 30;
  using G = Plant::Genome;

  void parse (const Simulation &s) {
    auto start = Simulation::clock::now();

    values.clear();
    svalues.clear();
    for (const auto &p: s.plants())
      addGenome(p.second->genome());

    if (debugGenepool)
      std::cout << *this << "\nParsed in " << Simulation::duration(start)
                << " ms" << std::endl;
  }

  friend double matching (const GenePool &lhs, const GenePool &rhs) {
    auto start = Simulation::clock::now();
    uint match = 0, miss = 0;

    if (debugGenepool)
    std::cout << "\n" << std::string(80, '=') << "\n";

    for (auto lhsMIt = lhs.values.begin(), rhsMIt = rhs.values.begin();
         lhsMIt != lhs.values.end(); ++lhsMIt, ++rhsMIt) {
      const auto lhsS = lhsMIt->second, rhsS = rhsMIt->second;
      uint lmatch = 0, lmiss = 0;

      // Debug
      std::ostringstream lhsOss, rhsOss;
      //

      auto lhsSIt = lhsS.begin(), rhsSIt = rhsS.begin();
      while (lhsSIt != lhsS.end() && rhsSIt != rhsS.end()) {
        auto lhsV = *lhsSIt, rhsV = *rhsSIt;
        if (lhsV == rhsV) {
          lmatch++;
          ++lhsSIt;
          ++rhsSIt;

          if (debugGenepool) {
            lhsOss << " " << lhsV;
            rhsOss << " " << rhsV;
          }

        } else if (lhsV < rhsV) {
          lmiss++;
          ++lhsSIt;

          if (debugGenepool) {
            lhsOss << " " << lhsV;
            rhsOss << " " << std::setw(std::ceil(std::log10(lhsV+1))) << ' ';
          }

        } else {
          lmiss++;
          ++rhsSIt;

          if (debugGenepool) {
            lhsOss << " " << std::setw(std::ceil(std::log10(rhsV+1))) << ' ';
            rhsOss << " " << rhsV;
          }
        }
      }

      lmiss += std::distance(lhsSIt, lhsS.end());
      lmiss += std::distance(rhsSIt, rhsS.end());

      if (debugGenepool) {
        std::cout << " " << lmatch << " lmatches\n";
        std::cout << " " << lmiss << " lmisses\n";
      }

      match += lmatch;
      miss += lmiss;

      // Debug
      if (debugGenepool) {
        std::cout << std::setw(W) << lhsMIt->first << ":";
        std::cout << lhsOss.str();
        for (auto it = lhsSIt; it!=lhsS.end(); ++it)
          std::cout << " " << *it;
        std::cout << "\n";
        std::cout << std::setw(W) << " " << " ";
        std::cout << rhsOss.str();
        for (auto it = rhsSIt; it!=rhsS.end(); ++it)
          std::cout << " " << *it;
        std::cout << "\n";
      }
      //
    }

    auto lhsMIt = lhs.svalues.begin(), rhsMIt = rhs.svalues.begin();
    while (lhsMIt != lhs.svalues.end() && rhsMIt != rhs.svalues.end()) {
      uint lmatch = 0, lmiss = 0;

      const auto &lhsK = lhsMIt->first, rhsK = rhsMIt->first;
      if (lhsK == rhsK) { // Same key. Compare contents

        // Debug
        std::ostringstream lhsOss, rhsOss;
        //

        const auto lhsS = lhsMIt->second, rhsS = rhsMIt->second;
        auto lhsSIt = lhsS.begin(), rhsSIt = rhsS.begin();
        while (lhsSIt != lhsS.end() && rhsSIt != rhsS.end()) {
          auto lhsV = *lhsSIt, rhsV = *rhsSIt;
          if (lhsV == rhsV) {
            lmatch++;
            ++lhsSIt;
            ++rhsSIt;

            if (debugGenepool) {
              lhsOss << " " << lhsV;
              rhsOss << " " << rhsV;
            }

          } else if (lhsV < rhsV) {
            lmiss++;
            ++lhsSIt;

            if (debugGenepool) {
              lhsOss << " " << lhsV;
              rhsOss << " " << std::setw(lhsV.length()) << ' ';
            }

          } else {
            lmiss++;
            ++rhsSIt;

            if (debugGenepool) {
              lhsOss << " " << std::setw(rhsV.length()) << ' ';
              rhsOss << " " << rhsV;
            }
          }
        }

        lmiss += std::distance(lhsSIt, lhsS.end());
        lmiss += std::distance(rhsSIt, rhsS.end());

        // Debug
        if (debugGenepool) {
          std::cout << std::setw(W) << lhsMIt->first << ":";
          std::cout << lhsOss.str();
          for (auto it = lhsSIt; it!=lhsS.end(); ++it)
            std::cout << " " << *it;
          std::cout << "\n";
          std::cout << std::setw(W) << " " << " ";
          std::cout << rhsOss.str();
          for (auto it = rhsSIt; it!=rhsS.end(); ++it)
            std::cout << " " << *it;
          std::cout << "\n";
        }
        //

        ++lhsMIt;
        ++rhsMIt;

      } else if (lhsK < rhsK) {
        if (debugGenepool) {
          std::cout << std::setw(W) << lhsMIt->first << ":";
          for (auto v: lhsMIt->second)  std::cout << " " << v;
          std::cout << "\n";
          std::cout << std::setw(W) << " " << "   " << "\n";
        }

        lmiss += lhsMIt->second.size();
        ++lhsMIt;

      } else {
        if (debugGenepool) {
          std::cout << std::setw(W) << rhsMIt->first << ":  \n";
          std::cout << std::setw(W) << " " << " ";
          for (auto v: rhsMIt->second)  std::cout << " " << v;
          std::cout << "\n";
        }

        lmiss += rhsMIt->second.size();
        ++rhsMIt;
      }

      if (debugGenepool) {
        std::cout << " " << lmatch << " lmatches\n";
        std::cout << " " << lmiss << " lmisses\n";
      }

      match += lmatch;
      miss += lmiss;
    }

    for (auto it=lhsMIt; it!=lhs.svalues.end(); ++it) {
      miss += it->second.size();

      if (debugGenepool) {
        std::cout << std::setw(W) << it->first << ":";
        for (auto v: it->second)  std::cout << " " << v;
        std::cout << "\n";
        std::cout << std::setw(W) << " " << "   " << "\n";
      }
    }
    for (auto it=rhsMIt; it!=rhs.svalues.end(); ++it) {
      miss += it->second.size();

      if (debugGenepool) {
        std::cout << std::setw(W) << rhsMIt->first << ":  \n";
        std::cout << std::setw(W) << " " << " ";
        for (auto v: it->second)  std::cout << " " << v;
        std::cout << "\n";
      }
    }

    if (debugGenepool) {
      std::cout << match << " matches\n";
      std::cout << miss << " misses\n";
      std::cout << "Matching = " << match / double(match + miss) << "\n";
      std::cout << " computed in " << Simulation::duration(start) << " ms"
                << std::endl;
      std::cout << "\n" << std::string(80, '=') << "\n";
    }

    return match / double(match + miss);
  }

  friend std::ostream& operator<< (std::ostream &os, const GenePool &gp) {
    os << std::setfill(' ');
    for (const auto &p: gp.values) {
      os << std::setw(W) << p.first << ":";
      for (const auto v: p.second)
        os << " " << v;
      os << "\n";
    }
    for (const auto &p: gp.svalues) {
      os << std::setw(W) << p.first << ":";
      for (const auto v: p.second)
        os << " " << v;
      os << "\n";
    }
    return os;
  }

private:
  std::map<std::string, std::set<int>> values;
  std::map<std::string, std::set<std::string>> svalues;

  void addGenome (const G &g) {
#define NINSERT(N,X) values[N].insert(X);
#define INSERT(X) NINSERT(#X, g.X)

    NINSERT("cdata.optimalDistance", 10*g.cdata.getOptimalDistance())
    NINSERT("cdata.inbreedTolerance", 10*g.cdata.getInbreedTolerance())
    NINSERT("cdata.outbreedTolerance", 10*g.cdata.getOutbreedTolerance())

    INSERT(shoot.recursivity)
    for (const auto &p: g.shoot.rules)
      svalues[std::string("shoot.") + p.second.lhs].insert(p.second.rhs);

    INSERT(root.recursivity)
    for (const auto &p: g.root.rules)
      svalues[std::string("root.") + p.second.lhs].insert(p.second.rhs);

    NINSERT("metabolism.resistors.G", 10*g.metabolism.resistors[0])
    NINSERT("metabolism.resistors.W", 10*g.metabolism.resistors[1])
    NINSERT("metabolism.growthSpeed", 10*g.metabolism.growthSpeed)
    NINSERT("metabolism.deltaWidth", 100*g.metabolism.deltaWidth)

    INSERT(dethklok)
    NINSERT("fruitOvershoot", 10*g.fruitOvershoot)
    INSERT(seedsPerFruit)
    NINSERT("temperatureOptimal", 10*g.temperatureOptimal)
    NINSERT("temperatureRange", 10*g.temperatureRange)

#undef INSERT
#undef NINSERT
  }
};

struct Parameters {
  genotype::Environment envGenome;
  genotype::Plant plantGenome;

  stdfs::path subfolder = "tmp/test_timelines/";
  Simulation::Overwrite overwrite = Simulation::ABORT;

  uint epoch = 0;
  uint epochs = 100;
  uint epochDuration = 1;
  uint branching = 3;

  decltype(genotype::Environment::rngSeed) gaseed;
};

DEFINE_UNSCOPED_PRETTY_ENUMERATION(Fitnesses, EDST, STGN, DENS, TIME)
using FUtils = EnumUtils<Fitnesses>;
using Fitnesses_t = std::array<double, FUtils::size()>;

struct Format {
  int width, precision;

  using Flags = std::ios_base::fmtflags;
  Flags flags;

  Format (int w, int p, Flags f = Flags())
    : width(w), precision(p), flags(f) {}

  static Format integer (int width) {
    return Format (width, 0);
  }

  static Format decimal (int precision, bool neg = false) {
    return Format (5+precision+neg, precision, std::ios_base::showpoint);
  }

  static Format save (std::ostream &os) {
    return Format (0, os.precision(), os.flags());
  }

  template <typename T>
  std::ostream& operator() (std::ostream &os, T v) const {
    auto f = os.flags();
    auto p = os.precision();
    os << *this << v;
    os.flags(f);
    os.precision(p);
    return os;
  }

  friend std::ostream& operator<< (std::ostream &os, const Format &f) {
    os.setf(f.flags);
    return os << std::setprecision(f.precision) << std::setw(f.width);
  }
};

const std::map<Fitnesses, Format> formatters {
//  { CMPT, Format::decimal(6) },
  { EDST, Format::integer(4) },
  { STGN, Format::decimal(6, true) },
  { DENS, Format::integer(4) },
  { TIME, Format::decimal(4, true) },
};

struct Alternative {
  static constexpr double MAGNITUDE = 1;

  uint index;
  Fitnesses_t fitnesses;

  Simulation simulation;

  /// Compare with a margin
  friend bool operator< (const Alternative &lhs, const Alternative &rhs) {
    for (Fitnesses f: FUtils::iterator()) {
      double lhsF = lhs.fitnesses[f],
             rhsF = rhs.fitnesses[f];
      bool invert = lhsF < 0 && rhsF < 0;
      if (invert) lhsF *= -1, rhsF *= -1;
      lhsF = log10(lhsF);
      rhsF = log10(rhsF);
      if (std::fabs(lhsF - rhsF) >= MAGNITUDE)
        return invert ? rhsF < lhsF : lhsF < rhsF;
    }
    return false;
  }
};

using Population = std::vector<Alternative>;

using ParetoFront = std::vector<uint>;

// == Inter-species compatibility
/* Exploited by plants through massive divergence in the compatibility function
   Induced insanely huge phylogenetic trees and computation times
 */
double interspeciesCompatibility (const Simulation &s) {
  float compat = 0;
  float ccount = 0;
  for (const auto &lhs_p: s.plants()) {
    const Plant &lhs = *lhs_p.second;
    if (lhs.sex() != Plant::Sex::FEMALE)  continue;

    for (const auto &rhs_p: s.plants()) {
      const Plant &rhs = *rhs_p.second;
      if (rhs.sex() != Plant::Sex::MALE)  continue;
      if (lhs.genealogy().self.sid == rhs.genealogy().self.sid) continue;

      compat += lhs.genome().compatibility(distance(lhs.genome(), rhs.genome()));
      ccount ++;
    }
  }
  return (ccount > 0) ? -compat / ccount : -1;
}

// == Inter-species compatibility (std-dev)
/* ?
 */
double interspeciesCompatibilitySTD(const Simulation &s) {
  std::vector<float> compats;
  float avgCompat = 0, stdCompat = 0;

  for (const auto &lhs_p: s.plants()) {
    const Plant &lhs = *lhs_p.second;
    if (lhs.sex() != Plant::Sex::FEMALE)  continue;

    for (const auto &rhs_p: s.plants()) {
      const Plant &rhs = *rhs_p.second;
      if (rhs.sex() != Plant::Sex::MALE)  continue;
      if (lhs.genealogy().self.sid == rhs.genealogy().self.sid) continue;

      float compat = lhs.genome().compatibility(distance(lhs.genome(),
                                                         rhs.genome()));
      avgCompat += compat;
      compats.push_back(avgCompat);
    }
  }

  if (!compats.empty()) {
    avgCompat /= compats.size();
    for (float c: compats)  stdCompat += std::pow(avgCompat - c, 2);
    stdCompat = std::sqrt(stdCompat / compats.size());
  }

  return stdCompat;
}

/// TODO put this back to its place in APOGeT@Node.hpp
double fullness (const Simulation::PTree::Node *n) {
  static const auto &K = config::PTree::rsetSize();
  return n->rset.size() / float(K);
}

// == Inter-species evolutionary distance
/* Not Tested */
double interspeciesDistance (const Simulation &s, uint index) {
  using N = Simulation::PTree::Node const*;
  const auto &phylogeny = s.phylogeny();
  const auto &aliveSpecies = phylogeny.aliveSpecies();

  uint maxD = 0;
  for (auto itL = aliveSpecies.begin(); itL != aliveSpecies.end(); ++itL) {
    N lhsN = phylogeny.nodeAt(*itL).get();
    if (fullness(lhsN) < 1) continue;

    std::vector<N> lineage;
    N itN = lhsN;
    while (itN) {
      lineage.push_back(itN);
      itN = itN->parent();
    }

    for (auto itR = std::next(itL); itR != aliveSpecies.end(); ++itR) {
      N rhsN = phylogeny.nodeAt(*itR).get();
      if (fullness(rhsN) < 1) continue;

      uint d = 0;
      itN = rhsN;

      auto ca = std::find(lineage.begin(), lineage.end(), itN);
      while (ca == lineage.end()) {
        itN = itN->parent();
        ca = std::find(lineage.begin(), lineage.end(), itN);
        d++;
      }

      d += std::distance(lineage.begin(), ca);
      if (maxD < d) maxD = d;
    }
  }

  return maxD;
}

void computeFitnesses(Alternative &a, const GenePool &atstart) {
  static constexpr auto M_INF = -std::numeric_limits<double>::infinity();

  const Simulation &s = a.simulation;
  a.fitnesses.fill(M_INF);

  if (s.plants().size() >= config::Simulation::initSeeds()) {
//    a.fitnesses[CMPT] = interspeciesCompatibility(s);
//    a.fitnesses[CMPT] = interspeciesCompatibilitySTD(s);

    a.fitnesses[EDST] = interspeciesDistance(s, a.index);

// == Genepool diversity
/* Can cause extinction */
    GenePool atend;
    atend.parse(s);
    a.fitnesses[STGN] = -matching(atstart, atend);

// ** Control fitness
// == Population size (keep in range)
    double p = s.plants().size();
    static constexpr double L = 500, H = 2500;
    a.fitnesses[DENS] = (p < L ? -1 : (p > H ? 0 : 1));

// ** Control fitness
// == Population size (keep ~constant)
//  a.fitnesses[CNST] = -std::fabs(startpop - int(s.plants().size()));

// ** Control fitness
// == Computation time (minimize)
    a.fitnesses[TIME] = -s.wallTimeDuration();
  }

  // log to local file
  std::ofstream ofs (a.simulation.dataFolder() / "fitnesses.dat");
  for (auto f: FUtils::iterator())
    ofs << " " << FUtils::getName(f);
  ofs << "\n";
  for (auto f: FUtils::iterator())
    ofs << " " << a.fitnesses[f];
  ofs << "\n";
}

/// Implements strong pareto domination:
///  a dominates b iff forall i, a_i >= b_i and exists i / a_i > b_i
bool paretoDominates (const Alternative &lhs, const Alternative &rhs) {
  bool better = false;
  const auto &lhsF = lhs.fitnesses, rhsF = rhs.fitnesses;
  for (auto itLhs = lhsF.begin(), itRhs = rhsF.begin();
       itLhs != lhsF.end();
       ++itLhs, ++itRhs) {
    double vLhs = *itLhs, vRhs = *itRhs;
    if (vLhs < vRhs)  return false;
    better |= (vRhs < vLhs);
  }
  return better;
}

/// Code inspired by gaga/gaga.hpp @ https://github.com/jdisset/gaga.git
void paretoFront (const Population &alternatives,
                  ParetoFront &front) {

  for (uint i=0; i<alternatives.size(); i++) {
    const Alternative &a = alternatives[i];
    bool dominated = false;

    for (uint j=0; j<front.size() && !dominated; j++)
      dominated = (paretoDominates(alternatives[front[j]], a));

    if (!dominated)
      for (uint j=i+1; j<alternatives.size() && !dominated; j++)
        dominated = paretoDominates(alternatives[j], a);

    if (!dominated)
      front.push_back(i);
  }
}

void printSummary (const Parameters &parameters,
                   const Population &alternatives,
                   const ParetoFront &pFront,
                   uint winner) {

  const auto oldFormat = Format::save(std::cout);
  static const auto iformatter =
    Format::integer(std::ceil(std::log10(parameters.branching)));

  static const auto fitnessHeader = [] {
    for (auto f: FUtils::iterator()) {
      std::cout << "   ";
      formatters.at(f)(std::cout, FUtils::getName(f));
    }
    std::cout << "\n";
  };

  static const auto printFitnesses = [] (const Alternative &a) {
    for (Fitnesses f: FUtils::iterator()) {
      std::cout << "   ";
      formatters.at(f)(std::cout, a.fitnesses[f]);
    }
  };

  if (parameters.epoch == 0) {
    std::cout << "# Seed epoch:\n"
                 "# " << iformatter << " ";
    fitnessHeader();
    std::cout << "# " << iformatter << 0;
    printFitnesses(alternatives[0]);
    std::cout << "\n";

  } else {
    std::cout << "# Alternatives:\n"
                 "# " << iformatter << " ";
    fitnessHeader();
    uint i=0;
    for (const Alternative &a: alternatives) {
      std::cout << "# " << iformatter << i++;
      printFitnesses(a);
      std::cout << "\n";
    }

    std::cout << "#\n# " << pFront.size() << " alternatives in pareto front"
                 "\n#  " << iformatter;
    fitnessHeader();
    for (uint i: pFront) {
      std::cout << "# " << ((i == winner) ? '*' : ' ');
      printFitnesses(alternatives[i]);
      std::cout << "\n";
    }
    std::cout << "#" << std::endl;
  }

  std::cout << oldFormat;
}

void controlGroup (const Parameters &parameters) {

  Alternative a;
  Simulation &s = a.simulation;
  auto start = Simulation::clock::now();
  GenePool genepool;

  s.init(parameters.envGenome, parameters.plantGenome);
  s.setDataFolder(parameters.subfolder / "results", parameters.overwrite);
  s.setDuration(simu::Environment::DurationSetType::SET,
                parameters.epochs * parameters.epochDuration);

  genepool.parse(s);

  while (!s.finished()) {
    if (s.time().isStartOfYear())
      s.printStepHeader();
    s.step();
  }

  computeFitnesses(a, genepool);

  std::cout << "Final fitnesses\n";
  for (auto f: FUtils::iterator())
    std::cout << "\t" << FUtils::getName(f)
              << ": " << a.fitnesses[f] << "\n";

  if (s.extinct())
    std::cout << "\nControl group suffered extinction at "
              << s.environment().time().pretty() << std::endl;
  else
    std::cout << "\nControl group completed in "
              << Simulation::duration(start) / 1000. << " seconds" << std::endl;
}

void exploreTimelines (Parameters parameters) {

  static constexpr auto DT = simu::Environment::DurationSetType::APPEND;

  uint epoch_digits = std::ceil(std::log10(parameters.epochs)),
       alternatives_digits = std::ceil(std::log10(parameters.branching));
  auto alternativeDataFolder =
    [&parameters, epoch_digits, alternatives_digits]
    (uint epoch, uint alternative) {
      std::ostringstream oss;
      oss << "results/e" << std::setfill('0') << std::setw(epoch_digits)
          << epoch << "_a" << std::setw(alternatives_digits) << alternative;
      return parameters.subfolder / oss.str();
    };

  auto championDataFolder = [&parameters, epoch_digits] (uint epoch) {
    std::ostringstream oss;
    oss << "results/e" << std::setfill('0') << std::setw(epoch_digits)
        << epoch << "_r";
    return parameters.subfolder / oss.str();
  };

  auto epochHeader = [epoch_digits, &parameters] {
    static const auto CTSIZE =  utils::CurrentTime::width();
    std::cout << std::string(8+epoch_digits+4+CTSIZE+2, '#') << "\n"
              << "# Epoch " << std::setw(epoch_digits) << parameters.epoch
              << " at " << utils::CurrentTime{} << " #"
              << std::endl;
  };

  rng::FastDice dice (parameters.gaseed);
  auto start = Simulation::clock::now();

  Population alternatives;
  for (uint a=0; a<parameters.branching; a++) {
    alternatives.emplace_back();
    alternatives.back().index = a;
  }

  GenePool genepool;

  stdfs::create_directories(parameters.subfolder / "results/");
  std::ofstream timelinesOFS (parameters.subfolder / "results/timelines.dat");
  const auto logFitnesses =
    [&timelinesOFS, &alternatives] (uint epoch, uint winner) {
      std::ofstream &ofs = timelinesOFS;
      auto N = alternatives.size();

      if (epoch == 0) {
        N = 1;
        ofs << "E A W";
        for (auto f: FUtils::iterator())
          ofs << " " << FUtils::getName(f);
        ofs << "\n";
      }

      for (uint i=0; i<N; i++) {
        ofs << epoch << " " << i << " " << (i == winner);
        for (const auto &f: alternatives[i].fitnesses)
          ofs << " " << f;
        ofs << "\n";
      }

      ofs.flush();
    };

  const auto logEnvController = [] (const Simulation &s) {
    const genotype::Environment g = s.environment().genome();
    g.toFile(s.dataFolder() / "controller");

    std::ofstream ofs (s.dataFolder() / "controller.tex");
    ofs << "\\documentclass[preview]{standalone}\n"
           "\\usepackage{amsmath}\n"
           "\\begin{document}\n"
        << g.controller.toTex()
        << "\\end{document}\n";
    ofs.close();

    using O = genotype::Environment::CGP::DotOptions;
    g.controller.toDot(s.dataFolder() / "controller.dot", O::FULL | O::NO_ARITY);
  };

  Alternative *reality = &alternatives.front();
  uint winner = 0;

  reality->simulation.init(parameters.envGenome, parameters.plantGenome);
  reality->simulation.setDataFolder(championDataFolder(parameters.epoch),
                                    parameters.overwrite);
  reality->simulation.setDuration(DT, parameters.epochDuration);
  logEnvController(reality->simulation);

  epochHeader();
  genepool.parse(reality->simulation);

  while (!reality->simulation.finished()) reality->simulation.step();

  computeFitnesses(*reality, genepool);
  printSummary(parameters, alternatives, ParetoFront(), -1);
  logFitnesses(0, 0);

  parameters.epoch++;
  do {
    epochHeader();
    genepool.parse(reality->simulation);

    // Populate next epoch from current best alternative
    if (winner != 0)  alternatives[0].simulation.clone(reality->simulation);
    #pragma omp parallel for schedule(dynamic)
    for (uint a=1; a<parameters.branching; a++) {
      if (winner != a)  alternatives[a].simulation.clone(reality->simulation);
      alternatives[a].simulation.mutateEnvController(dice);
    }

    // Prepare data folder and set durations
    for (uint a=0; a<parameters.branching; a++) {
      Simulation &s = alternatives[a].simulation;
      s.setDuration(DT, parameters.epochDuration);
      s.setDataFolder(alternativeDataFolder(parameters.epoch, a),
                      parameters.overwrite);
      logEnvController(s);
    }

    // Execute alternative simulations in parallel
    #pragma omp parallel for schedule(dynamic)
    for (uint a=0; a<parameters.branching; a++) {
      Simulation &s = alternatives[a].simulation;

      while (!s.finished()) s.step();

      computeFitnesses(alternatives[a], genepool);
    }

    // Find 'best' alternative
    ParetoFront pFront;
    paretoFront(alternatives, pFront);
    winner = *dice(pFront);
    reality = &alternatives[winner];

    logFitnesses(parameters.epoch, winner);

    // Store result accordingly
    stdfs::create_directory_symlink(reality->simulation.dataFolder().filename(),
                                    championDataFolder(parameters.epoch));

    printSummary(parameters, alternatives, pFront, winner);

    if (reality->simulation.extinct()) {
      reality = nullptr;
      break;
    }

    parameters.epoch++;
  } while (parameters.epoch < parameters.epochs);

  if (reality)
    std::cout << "\nTimelines exploration completed in "
              << Simulation::duration(start) / 1000. << " seconds" << std::endl;
  else
    std::cout << "\nTimelines exploration failed. Extinction at epoch "
              << parameters.epoch << std::endl;
}

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  std::string envGenomeArg, plantGenomeArg;
  decltype(genotype::Environment::rngSeed) envOverrideSeed;

  Parameters parameters;
  char overwrite;

  cxxopts::Options options("ReusWorld (timelines explorer)",
                           "Continuous 1 + lambda of a 2D simulation of plants"
                           " in a changing environment");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))
    ("f,folder", "Folder under which to store the results",
     cxxopts::value(parameters.subfolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))
    ("e,environment", "Environment's genome or a random seed",
     cxxopts::value(envGenomeArg))
    ("p,plant", "Plant genome to start from or a random seed",
     cxxopts::value(plantGenomeArg))
    ("env-seed", "Overrides enviroment's seed with provided value",
     cxxopts::value(envOverrideSeed))
    ("ga-seed", "RNG seed for the genetic algorithm",
     cxxopts::value(parameters.gaseed))
    ("epochs", "Number of epochs",
     cxxopts::value(parameters.epochs))
    ("epoch-duration", "Number of years per epoch",
     cxxopts::value(parameters.epochDuration))
    ("alternatives-per-epoch", "Number of explored alternatives per epoch",
     cxxopts::value(parameters.branching))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << "\n\nOption 'auto-config' is the default and overrides 'config'"
                 " if both are provided"
              << "\nEither both 'plant' and 'environment' options are used or "
                 "a valid file is to be provided to 'load' (the former has "
                 "precedance in case all three options are specified)"
              << std::endl;
    return 0;
  }

  bool missingArgument = !result.count("environment") || !result.count("plant");
  if (missingArgument) {
    if (result.count("environment"))
      utils::doThrow<std::invalid_argument>("No value provided for the plant's genome");

    if (result.count("plant"))
      utils::doThrow<std::invalid_argument>("No value provided for the environment's genome");
  }

  if (result.count("overwrite"))
    parameters.overwrite = simu::Simulation::Overwrite(overwrite);

  if (!stdfs::is_empty(parameters.subfolder)) {
    std::cerr << "Base forlder is not empty!\n";
    switch (parameters.overwrite) {
    case simu::Simulation::Overwrite::PURGE:
      std::cerr << "Purging" << std::endl;
      stdfs::remove_all(parameters.subfolder);
      break;

    case simu::Simulation::Overwrite::ABORT:
    default:
      std::cerr << "Aborting" << std::endl;
      exit(1);
      break;
    }
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  config::Simulation::setupConfig(configFile, verbosity);
  config::Simulation::verbosity.ref() = 0;
  if (configFile.empty()) config::Simulation::printConfig("");

  genotype::Plant::printMutationRates(std::cout, 2);

  if (!isValidSeed(envGenomeArg)) {
    std::cout << "Reading environment genome from input file '"
              << envGenomeArg << "'" << std::endl;
    parameters.envGenome = genotype::Environment::fromFile(envGenomeArg);

  } else {
    rng::FastDice dice (std::stoi(envGenomeArg));
    std::cout << "Generating environment genome from rng seed "
              << dice.getSeed() << std::endl;
    parameters.envGenome = genotype::Environment::random(dice);
  }
  if (result.count("env-seed")) parameters.envGenome.rngSeed = envOverrideSeed;
  parameters.envGenome.toFile("initial", 2);

  if (!isValidSeed(plantGenomeArg)) {
    std::cout << "Reading plant genome from input file '"
              << plantGenomeArg << "'" << std::endl;
    parameters.plantGenome = genotype::Plant::fromFile(plantGenomeArg);

  } else {
    rng::FastDice dice (std::stoi(plantGenomeArg));
    std::cout << "Generating plant genome from rng seed "
              << dice.getSeed() << std::endl;
    parameters.plantGenome = genotype::Plant::random(dice);
  }

  parameters.plantGenome.toFile("inital", 2);

  std::cout << "Environment:\n" << parameters.envGenome
            << "\nPlant:\n" << parameters.plantGenome
            << std::endl;

//  // ===========================================================================
//  // == SIGINT management

//  struct sigaction act = {};
//  act.sa_handler = &sigint_manager;
//  if (0 != sigaction(SIGINT, &act, nullptr))
//    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
//  if (0 != sigaction(SIGTERM, &act, nullptr))
//    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == Test area

//#warning autoremoving datafolder
//  stdfs::remove_all(parameters.subfolder);

#if !defined(NDEBUG) && TEST == 1
  // Test cloning

//  uint i = 1;
////  utils::assertEqual(i, i, true);

//  double d = 0;
////  utils::assertEqual(d, d, true);

//  std::array<double, 10> a;
////  utils::assertEqual(a, a, true);

//  std::vector<double*> v0;
//  for (uint i=0; i<10; i++) v0.push_back(&a[i]);

//  std::vector<double*> v1 = v0;
//  v1[0] = &d;

//  utils::assertEqual(v0, v1, true);

  uint D = 1;
  Simulation clonedSimu;
  {
    Simulation baseSimu;
    baseSimu.init(envGenome, plantGenome);
    baseSimu.setDuration(simu::Environment::DurationSetType::SET, D);
    baseSimu.setDataFolder("tmp/test/simulation_cloning/base/");
    while (!baseSimu.finished())  baseSimu.step();
    baseSimu.atEnd();

    clonedSimu.clone(baseSimu);
    assertEqual(baseSimu, clonedSimu, true);
  }

  clonedSimu.setDuration(simu::Environment::DurationSetType::APPEND, D);
  clonedSimu.setDataFolder("tmp/test/simulation_cloning/clone/");
  while (!clonedSimu.finished())  clonedSimu.step();
  clonedSimu.atEnd();

  return 255;

#elif !defined(NDEBUG) && TEST == 2
  // Test pareto

  std::ofstream ofsPop ("pareto_test_population.dat");

  rng::FastDice dice (0);
  PopFitnesses pfitnesses (1000);

  ofsPop << "A B\n";
  for (Fitnesses &f: pfitnesses) {
    for (std::string k: {"A", "B"}) {
      double v = dice(0., 1.);
      f[k] = v;
      ofsPop << v << " ";
    }
    ofsPop << "\n";
  }

  std::ofstream ofsPar ("pareto_test_front.dat");
  std::vector<uint> front;
  paretoFront(pfitnesses, front);

  ofsPar << "A B\n";
  for (uint i: front) {
    const auto &f = pfitnesses[i];
    for (std::string k: {"A", "B"})
      ofsPar << f.at(k) << " ";
    ofsPar << "\n";
  }

#else

  // ===========================================================================
  // == Core setup

  if (parameters.branching <= 1)
    controlGroup(parameters);

  else
    exploreTimelines(parameters);

  return 0;

#endif
}
