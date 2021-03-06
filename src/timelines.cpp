#include <csignal>

#include <numeric>

#include <omp.h>

#include "kgd/external/cxxopts.hpp"

#include "simu/simulation.h"
#include "config/dependencies.h"
#include "genotype/genepool.h"

//#define TEST 1
#if TEST == 1
#warning Test mode activated for timelines executable
#endif
void tests (void) {
#if !defined(NDEBUG) && TEST == 1

  // ===========================================================================
  // == Test area

//#warning autoremoving datafolder
//  stdfs::remove_all(parameters.subfolder);

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
#endif
}

bool isValidSeed(const std::string& s) {
  return !s.empty()
    && std::all_of(s.begin(),
                   s.end(),
                   [](char c) { return std::isdigit(c); }
    );
}

using Plant = simu::Plant;
using Simulation = simu::Simulation;
using GenePool = misc::GenePool;

struct Parameters {
  genotype::Environment envGenome;
  genotype::Plant plantGenome;

  stdfs::path subfolder = "tmp/test_timelines/";

  uint epoch = 0;
  uint epochs = 100;
  uint epochDuration = 1;
  uint branching = 3;

  decltype(genotype::Environment::rngSeed) gaseed;

  int resume = 0;
};

DEFINE_UNSCOPED_PRETTY_ENUMERATION(Fitnesses, GDIST, NVLT, DENS, TIME)
using FUtils = EnumUtils<Fitnesses>;
using Fitnesses_t = std::array<double, FUtils::size()>;

struct Format {
  int width, precision;

  using Flags = std::ios_base::fmtflags;
  Flags flags;

  Format (int w, int p, Flags f = Flags())
    : width(w), precision(p), flags(f) {}

  static Format integer (int width) {
    return Format (width, 0, std::ios_base::fixed);
  }

  static Format decimal (int precision, bool neg = false) {
    return Format (5+precision+neg, precision,
                   std::ios_base::showpoint | ~std::ios_base::floatfield);
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

struct CFormat {
  const char *header, *value;
};

const std::map<Fitnesses, CFormat> cformatters {
//  { CMPT, { "%9s", "%9.3e" } },
  { GDIST, { "%9s", "%9.3g" } },
  { NVLT, { "%5s", "%5.3f" } },
  { DENS, { "%6s", "% 6.3f" } },
  { TIME, { "%10s", "%10.3g" } },
};

struct Alternative {
  static constexpr double MAGNITUDE = 1;

  uint index;
  Fitnesses_t fitnesses;

  Simulation simulation;

  Alternative (uint i) : index(i) {}

  /// Compare with a margin
//  friend bool operator< (const Alternative &lhs, const Alternative &rhs) {
//    for (Fitnesses f: FUtils::iterator()) {
//      double lhsF = lhs.fitnesses[f],
//             rhsF = rhs.fitnesses[f];
//      bool invert = lhsF < 0 && rhsF < 0;
//      if (invert) lhsF *= -1, rhsF *= -1;
//      lhsF = log10(lhsF);
//      rhsF = log10(rhsF);
//      if (std::fabs(lhsF - rhsF) >= MAGNITUDE)
//        return invert ? rhsF < lhsF : lhsF < rhsF;
//    }
//    return false;
//  }

  /// Just compare
  friend bool operator< (const Alternative &lhs, const Alternative &rhs) {
    for (Fitnesses f: FUtils::iterator()) {
      auto lhsF = lhs.fitnesses[f], rhsF = rhs.fitnesses[f];
      if (lhsF != rhsF) return lhsF < rhsF;
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

double interspeciesGeneticDistanceStd (const Simulation &s) {
  float distAvg = 0, distStd = 0;
  std::vector<float> dists;

  const auto &p = s.plants();

  for (auto lhs_it = p.begin(); lhs_it != p.end(); ++lhs_it) {
    const Plant &lhs = *(*lhs_it).second;
    if (lhs.sex() != Plant::Sex::FEMALE)  continue;

    for (auto rhs_it = std::next(lhs_it); rhs_it != p.end(); ++rhs_it) {
      const Plant &rhs = *(*rhs_it).second;
      if (rhs.sex() != Plant::Sex::MALE)  continue;

      if (lhs.genealogy().self.sid == rhs.genealogy().self.sid) continue;

      float d = distance(lhs.genome(), rhs.genome());
      distAvg += d;
      dists.push_back(d);
    }
  }

  if (!dists.empty()) {
    distAvg /= float(dists.size());
    for (float d: dists)  distStd += std::fabs(distAvg - d);
    distStd /= float(dists.size());
  }

  return distStd;
}

double geneticDistanceStd (const Simulation &s) {
  float distAvg = 0, distStd = 0;
  std::vector<float> dists;

  const auto &p = s.plants();

  for (auto lhs_it = p.begin(); lhs_it != p.end(); ++lhs_it) {
    const Plant &lhs = *(*lhs_it).second;

    for (auto rhs_it = std::next(lhs_it); rhs_it != p.end(); ++rhs_it) {
      const Plant &rhs = *(*rhs_it).second;

      float d = distance(lhs.genome(), rhs.genome());
      distAvg += d;
      dists.push_back(d);
    }
  }

  if (!dists.empty()) {
    distAvg /= float(dists.size());
    for (float d: dists)  distStd += std::fabs(distAvg - d);
    distStd /= float(dists.size());
  }

  return distStd;
}

/// TODO put this back to its place in APOGeT@Node.hpp
double fullness (const Simulation::PTree::Node *n) {
  static const auto &K = config::PTree::rsetSize();
  return n->rset.size() / float(K);
}

// == Inter-species evolutionary distance
/* Better than raw interspeciesCompatibility but still dangerously slow */
double interspeciesDistance (const Simulation &s) {
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

void cancelAllBut (Fitnesses_t &fitnesses, Fitnesses except) {
  static constexpr auto M_INF = -std::numeric_limits<double>::infinity();

  for (Fitnesses f: FUtils::iterator())
    if (f != except)
      fitnesses.at(f) = M_INF;
}

void computeFitnesses(Alternative &a, const GenePool &atstart) {
  // 1 hour
  static constexpr auto maxDuration = 1 * 60 * 60;

  const Simulation &s = a.simulation;

  // ===========================================================================
  // ** Control fitnesses

  // == Population size (keep in range with soft tails)
  double p = s.plants().size();
  static constexpr double L = 500, H = 2500;
  static constexpr double sL = 120, sH = 800;
  if (p < L)      a.fitnesses[DENS] = 2*utils::gauss(p, L, sL)-1;
  else if (p > H) a.fitnesses[DENS] = utils::gauss(p, H, sH);
  else            a.fitnesses[DENS] = 1;

  // == Population size (keep ~constant)
  //  a.fitnesses[CNST] = -std::fabs(startpop - int(s.plants().size()));

  // == Computation time (minimize)
  auto secDuration = s.wallTimeDuration() / 1000.;
  a.fitnesses[TIME] = -secDuration;

  // ===========================================================================
  // ** Work fitnesses

  if (s.plants().size() < config::Simulation::initSeeds())
    cancelAllBut(a.fitnesses, DENS);

  else if (maxDuration < secDuration)
    cancelAllBut(a.fitnesses, TIME);

  else {
//    a.fitnesses[CMPT] = interspeciesCompatibility(s);
//    a.fitnesses[CMPT] = interspeciesCompatibilitySTD(s);

    a.fitnesses[GDIST] = interspeciesGeneticDistanceStd(s);
//    a.fitnesses[GDIST] = geneticDistanceStd(s);

//    a.fitnesses[EDST] = interspeciesDistance(s, a.index);

// == Genepool diversity
/* Can cause extinction */
//    GenePool atend;
//    atend.parse(s);
//    a.fitnesses[STGN] = matching(atstart, atend);

// == Genepool frequency variation
/* ? */
    GenePool atend;
    atend.parse(s);
    a.fitnesses[NVLT] = divergence(atstart, atend);
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
//      formatters.at(f)(std::cout, FUtils::getName(f));
      printf(cformatters.at(f).header, FUtils::getName(f).c_str());
    }
    std::cout << "\n";
  };

  static const auto printFitnesses = [] (const Alternative &a) {
    for (Fitnesses f: FUtils::iterator()) {
      std::cout << "   ";
//       formatters.at(f)(std::cout, a.fitnesses[f]);
      printf(cformatters.at(f).value, a.fitnesses[f]);
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

  Alternative a (0);
  Simulation &s = a.simulation;
  auto start = Simulation::clock::now();
  GenePool genepool;

  s.init(parameters.envGenome, parameters.plantGenome);
  s.setDataFolder(parameters.subfolder / "results",
                  Simulation::Overwrite::ABORT);
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
              << Simulation::prettyDuration(start) << std::endl;
}


uint digits (uint number) {
  return std::ceil(std::log10(number));
}

stdfs::path alternativeDataFolder (const stdfs::path &basefolder,
                                   uint epoch, uint alternative,
                                   uint epochs, uint alternatives) {
  std::ostringstream oss;
  oss << "results/e" << std::setfill('0')
      << std::setw(digits(epochs)) << epoch
      << "_a" << std::setw(digits(alternatives)) << alternative;
  return basefolder / oss.str();
}

stdfs::path championDataFolder (const stdfs::path &basefolder,
                                uint epoch, uint epochs) {
  std::ostringstream oss;
  oss << "results/e" << std::setfill('0')
      << std::setw(digits(epochs)) << epoch << "_r";
  return basefolder / oss.str();
}

void saveGlobalTimelineState (Parameters parameters) {
  nlohmann::json j;
  j["epochs"] = parameters.epochs;
  j["epochDuration"] = parameters.epochDuration;
  j["branching"] = parameters.branching;
  j["baseEnv"] = parameters.envGenome;
  j["basePlant"] = parameters.plantGenome;
  j["gaseed"] = parameters.gaseed;

  std::ofstream ofs (parameters.subfolder / "timelines.json");
  ofs << j;
  ofs.close();
}

void saveLastEpochState (Parameters parameters, const rng::FastDice &dice,
                         const stdfs::path &reality, unsigned long duration) {
  nlohmann::json j;
  simu::save(j["dice"], dice);
  j["epoch"] = parameters.epoch;
  j["reality"] = reality;
  j["duration"] = duration;

  std::ofstream ofs (parameters.subfolder / "lastEpoch.json");
  ofs << j;
  ofs.close();
}

void loadTimelinesStates (Alternative &reality, Parameters &parameters,
                          rng::FastDice &dice,
                          unsigned long &previousDuration) {

  stdfs::path realitySaveFile;

  {
    stdfs::path datafile = parameters.subfolder / "timelines.json";
    std::ifstream ifs (datafile);
    if (!ifs)
      utils::doThrow<std::invalid_argument>(
        "Could not find timelines global data at", datafile);

    std::cout << "Restoring timelines exploration global state from "
              << datafile << "\r";

    nlohmann::json j = nlohmann::json::parse(utils::readAll(ifs));

    parameters.branching = j["branching"];
    parameters.epochDuration = j["epochDuration"];
    parameters.epochs = j["epochs"];
  }

  {
    stdfs::path datafile = parameters.subfolder / "lastEpoch.json";
    std::ifstream ifs (datafile);
    if (!ifs)
      utils::doThrow<std::invalid_argument>(
        "Could not find timelines previous epoch data at", datafile);

    std::cout << "Restoring timelines exploration state from " << datafile << "\r";

    nlohmann::json j = nlohmann::json::parse(utils::readAll(ifs));

    simu::load(j["dice"], dice);
    parameters.epoch = j["epoch"];
    realitySaveFile = j["reality"].get<stdfs::path>();
    previousDuration = j["duration"];

    std::cout << "Resume type requested: " << parameters.resume
              << std::endl;

    if (parameters.resume > 0) { // Specific epoch requested -> try to honor
      if (parameters.epoch < uint(parameters.resume))
        utils::doThrow<std::invalid_argument>(
          "Cannot resume at epoch ", parameters.resume,
          ". Max epoch reached was ", parameters.epoch);

      parameters.epoch = parameters.resume;
      auto realityFolder =
        championDataFolder(parameters.subfolder,
                           parameters.epoch - 1, parameters.epochs);
      realitySaveFile =
        Simulation::periodicSaveName(realityFolder,
                                     parameters.epoch
                                     * parameters.epochDuration);
      realitySaveFile += ".ubjson";

      for (auto &p: stdfs::directory_iterator(parameters.subfolder/"results")) {
        std::istringstream iss (p.path().filename());
        char junk;
        int epoch = -1;
        iss >> junk >> epoch;
        if (int(parameters.epoch) <= epoch) {
          std::cout << "Purging future alternative " << p.path() << std::endl;
          stdfs::remove_all(p.path());
        }
      }

      std::cout << "Loading reality at epoch " << parameters.epoch
                << " from " << realitySaveFile << "\r";

    } else
      std::cout << "Loading last reality from " << realitySaveFile << "\r";
  }

  std::cout << "Loading last reality at " << realitySaveFile << "\r";
  Simulation::load(realitySaveFile, reality.simulation, "none", "");

  std::cout << "Restored timelines exploration state from "
            << parameters.subfolder
            << ". Resuming at epoch " << parameters.epoch << std::endl;
}

void exploreTimelines (Parameters parameters,
                       const cxxopts::ParseResult &arguments) {

  static constexpr auto DT = simu::Environment::DurationSetType::APPEND;

  uint epoch_digits = digits(parameters.epochs);

  auto alternativeDataFolder =
    [&parameters]
    (uint epoch, uint alternative) {
      return ::alternativeDataFolder(parameters.subfolder,
                                     epoch, alternative,
                                     parameters.epoch, parameters.branching);
    };

  auto championDataFolder = [&parameters] (uint epoch) {
    return ::championDataFolder(parameters.subfolder, epoch, parameters.epochs);
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
  unsigned long previousDuration = 0;

  Population alternatives;
  alternatives.emplace_back(0);

  GenePool genepool;
  std::ofstream timelinesOFS;

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
    const genotype::Environment g = s.environment().genomes().front();
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

  Alternative *reality = nullptr;
  uint winner = 0;

  if (parameters.resume) {
    loadTimelinesStates(alternatives.front(),
                        parameters, dice, previousDuration);

    /// Maybe update with new arguments
    if (arguments.count("epochs"))
      parameters.epochs = arguments["epochs"].as<uint>();

    if (arguments.count("epoch-duration"))
      parameters.epochDuration = arguments["epoch-duration"].as<uint>();

    if (arguments.count("alternatives-per-epoch"))
      parameters.branching = arguments["alternatives-per-epoch"].as<uint>();

    timelinesOFS.open(parameters.subfolder / "results/timelines.dat",
                      std::ios_base::out | std::ios_base::app);

    reality = &alternatives.front();

  } else {
    stdfs::create_directories(parameters.subfolder / "results/");
    saveGlobalTimelineState(parameters);
    timelinesOFS.open(parameters.subfolder / "results/timelines.dat");

    reality = &alternatives.front();

    reality->simulation.init(parameters.envGenome, parameters.plantGenome);
    reality->simulation.setDataFolder(championDataFolder(parameters.epoch),
                                      Simulation::Overwrite::ABORT);
    reality->simulation.setDuration(DT, parameters.epochDuration);
    logEnvController(reality->simulation);

    epochHeader();
    genepool.parse(reality->simulation);

    while (!reality->simulation.finished()) reality->simulation.step();

    computeFitnesses(*reality, genepool);
    printSummary(parameters, alternatives, ParetoFront(), -1);
    logFitnesses(0, 0);

    parameters.epoch++;
  }

  for (uint a=1; a<parameters.branching; a++) alternatives.emplace_back(a);

  // Reality is still the first but the vector has probably been reallocated
  reality = &alternatives.front();

  do {
    epochHeader();
    genepool.parse(reality->simulation);

//    std::cout << "Generating alternatives..." << std::endl;

    // Populate next epoch from current best alternative
    if (winner != 0)  alternatives[0].simulation.clone(reality->simulation);
    #pragma omp parallel for schedule(dynamic)
    for (uint a=1; a<parameters.branching; a++) {
      if (winner != a)  alternatives[a].simulation.clone(reality->simulation);
      alternatives[a].simulation.mutateEnvController(dice);
    }

//    std::cout << "Preparing folders..." << std::endl;

    // Prepare data folder and set durations
    for (uint a=0; a<parameters.branching; a++) {
      Simulation &s = alternatives[a].simulation;
      s.setDuration(DT, parameters.epochDuration);
      s.setDataFolder(alternativeDataFolder(parameters.epoch, a),
                      Simulation::Overwrite::ABORT);
      logEnvController(s);
    }

//    std::cout << "Executing alternatives..." << std::endl;

    // Execute alternative simulations in parallel
    #pragma omp parallel for schedule(dynamic)
    for (uint a=0; a<parameters.branching; a++) {
      Simulation &s = alternatives[a].simulation;

      while (!s.finished()) s.step();

      computeFitnesses(alternatives[a], genepool);
    }

//    std::cout << "Picking reality..." << std::endl;

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
    saveLastEpochState(parameters, dice,
                       reality->simulation.periodicSaveName().concat(".ubjson"),
                       Simulation::duration(start) + previousDuration);

  } while (parameters.epoch < parameters.epochs);

  if (reality)
    std::cout << "\nTimelines exploration completed in "
              << Simulation::prettyDuration(Simulation::duration(start)
                                            + previousDuration)
              << std::endl;
  else
    std::cout << "\nTimelines exploration failed. Extinction at epoch "
              << parameters.epoch << std::endl;
}

int main(int argc, char *argv[]) {
//  rng::FastDice dice (0);

//  using Config = config::EDNAConfigFile<genotype::Environment>;
//  Config::setupConfig("auto", config::Verbosity::SHOW);
//  Config::printConfig("");

//  auto e = genotype::Environment::random(dice);
//  std::cout << "random env:\n" << e << std::endl;

//  uint n = 10000;
//  for (uint i=0; i<n; i++)  e.controller.mutate(dice);

//  std::cout << "\nAfter " << n << " mutations:\n" << e << std::endl;

//  exit (255);

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;
  Simulation::Overwrite overwrite = Simulation::ABORT;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  std::string envGenomeArg, plantGenomeArg;
  decltype(genotype::Environment::rngSeed) envOverrideSeed;

  Parameters parameters;
  char coverwrite;

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
     cxxopts::value(coverwrite))
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
    ("resume", "Whether to continue from the data in the work folder",
     cxxopts::value(parameters.resume)->default_value("0")
                                        ->implicit_value("-1"))
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

  bool missingArgument = !parameters.resume
                    && (!result.count("environment") || !result.count("plant"));
  if (missingArgument) {
    if (result.count("environment"))
      utils::doThrow<std::invalid_argument>("No value provided for the plant's genome");

    if (result.count("plant"))
      utils::doThrow<std::invalid_argument>("No value provided for the environment's genome");
  }

  if (result.count("overwrite"))
    overwrite = simu::Simulation::Overwrite(coverwrite);

  stdfs::path rfolder = parameters.subfolder / "results";
  if (!parameters.resume
   && stdfs::exists(rfolder) && !stdfs::is_empty(rfolder)) {
    std::cerr << "Base folder is not empty!\n";
    switch (overwrite) {
    case simu::Simulation::Overwrite::PURGE:
      std::cerr << "Purging" << std::endl;
      stdfs::remove_all(rfolder);
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

  if (!parameters.resume) {
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
  }

//  // ===========================================================================
//  // == SIGINT management

//  struct sigaction act = {};
//  act.sa_handler = &sigint_manager;
//  if (0 != sigaction(SIGINT, &act, nullptr))
//    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
//  if (0 != sigaction(SIGTERM, &act, nullptr))
//    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

#if !defined(NDEBUG) && TEST
  tests();
  return 255;
#endif

  // ===========================================================================
  // == Core setup

  if (parameters.branching <= 1)
    controlGroup(parameters);

  else
    exploreTimelines(parameters, result);

  return 0;
}
