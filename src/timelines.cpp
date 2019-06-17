#include <csignal>

#include <omp.h>

#include "kgd/external/cxxopts.hpp"

#include "simu/simulation.h"
#include "config/dependencies.h"

//#define TEST 0
#ifdef TEST
#warning Test mode activated for timelines executable
#endif

bool isValidSeed(const std::string& s) {
  return !s.empty()
    && std::all_of(s.begin(),
                   s.end(),
                   [](char c) { return std::isdigit(c); }
    );
}

using Plant = simu::Plant;
using Simulation = simu::Simulation;
using Fitnesses = std::map<std::string, double>;
using PopFitnesses = std::vector<Fitnesses>;
void computeFitnesses(Simulation &s, Fitnesses &fitnesses) {
  fitnesses["Time"] = -s.wallTimeDuration();

  float plants = s.plants().size(), organs = 0;
  for (const auto &p: s.plants()) organs += p.second->organs().size();
  fitnesses["Cplx"] = (plants > 0) ? organs / plants : 0;

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
  fitnesses["Cmpt"] = compat / ccount;
}

/// Implements strong pareto domination:
///  a dominates b iff forall i, a_i >= b_i and exists i / a_i > b_i
bool paretoDominates (const Fitnesses &lhs, const Fitnesses &rhs) {
  bool better = false;
  for (auto itLhs = lhs.begin(), itRhs = rhs.begin();
       itLhs != lhs.end();
       ++itLhs, ++itRhs) {
    auto vLhs = itLhs->second, vRhs = itRhs->second;
    if (vLhs < vRhs)  return false;
    better |= (vRhs < vLhs);
  }
  return better;
}

/// Code taken from gaga/gaga.hpp @ https://github.com/jdisset/gaga.git
void paretoFront (const PopFitnesses &fitnesses, std::vector<uint> &front) {
  for (uint i=0; i<fitnesses.size(); i++) {
    bool dominated = false;
    for (uint j=0; j<front.size() && !dominated; j++)
      dominated = (paretoDominates(fitnesses[front[j]], fitnesses[i]));

    if (!dominated)
      for (uint j=i+1; j<fitnesses.size() && !dominated; j++)
        dominated = paretoDominates(fitnesses[j], fitnesses[i]);

    if (!dominated)
      front.push_back(i);
  }
}

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  std::string envGenomeArg, plantGenomeArg;
  genotype::Environment envGenome;
  genotype::Plant plantGenome;

  decltype(genotype::Environment::rngSeed) envOverrideSeed, gaseed;

  stdfs::path subfolder = "tmp/test_timelines/";
  uint epochs = 100, epochDuration = 1, branching = 3;

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
     cxxopts::value(subfolder))
    ("e,environment", "Environment's genome or a random seed",
     cxxopts::value(envGenomeArg))
    ("p,plant", "Plant genome to start from or a random seed",
     cxxopts::value(plantGenomeArg))
    ("env-seed", "Overrides enviroment's seed with provided value",
     cxxopts::value(envOverrideSeed))
    ("ga-seed", "RNG seed for the genetic algorithm",
     cxxopts::value(gaseed))
    ("epochs", "Number of epochs",
     cxxopts::value(epochs))
    ("epoch-duration", "Number of years per epoch",
     cxxopts::value(epochDuration))
    ("alternatives-per-epoch", "Number of explored alternatives per epoch",
     cxxopts::value(branching))
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

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  config::Simulation::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::Simulation::printConfig("");

  auto alternativeDataFolder = [&subfolder] (uint epoch, uint alternative) {
    std::ostringstream oss;
    oss << "results/e" << epoch << "_a" << alternative;
    return subfolder / oss.str();
  };

  auto championDataFolder = [&subfolder] (uint epoch) {
    std::ostringstream oss;
    oss << "results/e" << epoch;
    return subfolder / oss.str();
  };

  if (!isValidSeed(envGenomeArg)) {
    std::cout << "Reading environment genome from input file '"
              << envGenomeArg << "'" << std::endl;
    envGenome = genotype::Environment::fromFile(envGenomeArg);

  } else {
    rng::FastDice dice (std::stoi(envGenomeArg));
    std::cout << "Generating environment genome from rng seed "
              << dice.getSeed() << std::endl;
    envGenome = genotype::Environment::random(dice);
  }
  if (result.count("env-seed")) envGenome.rngSeed = envOverrideSeed;
  envGenome.toFile("last", 2);

  if (!isValidSeed(plantGenomeArg)) {
    std::cout << "Reading plant genome from input file '"
              << plantGenomeArg << "'" << std::endl;
    plantGenome = genotype::Plant::fromFile(plantGenomeArg);

  } else {
    rng::FastDice dice (std::stoi(plantGenomeArg));
    std::cout << "Generating plant genome from rng seed "
              << dice.getSeed() << std::endl;
    plantGenome = genotype::Plant::random(dice);
  }

  plantGenome.toFile("last", 2);

  std::cout << "Environment:\n" << envGenome
            << "\nPlant:\n" << plantGenome
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

#if !defined(NDEBUG) && TEST == 1
  // Test cloning

  uint D = 2;
  Simulation clonedSimu;
  {
    Simulation baseSimu;
    baseSimu.init(envGenome, plantGenome);
    baseSimu.setDuration(simu::Environment::DurationSetType::SET, D);
    baseSimu.setDataFolder("tmp/test/simulation_cloning/base/");
    while (!baseSimu.finished())  baseSimu.step();
    baseSimu.atEnd();

    clonedSimu.clone(baseSimu);
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

  using Simulation = simu::Simulation;
  static constexpr auto DT = simu::Environment::DurationSetType::APPEND;
  uint epoch = 0;

  rng::FastDice dice (gaseed);

  std::vector<Simulation> alternatives;
  for (uint a=0; a<branching; a++)  alternatives.emplace_back();

  PopFitnesses pfitnesses;
  pfitnesses.resize(branching);

  Simulation *reality = &alternatives.front();
  uint winner = 0;

  reality->init(envGenome, plantGenome);
  reality->setDataFolder(championDataFolder(epoch));
  reality->setDuration(DT, epochDuration);

  while (!reality->finished()) reality->step();

  epoch++;
  do {
    std::cout << "# Epoch " << epoch << " at " << utils::CurrentTime{} << " #"
              << std::endl;

    // Populate next epoch from current best alternative
    if (winner != 0)  alternatives[0].clone(*reality);
    for (uint a=1; a<branching; a++) {
      if (winner != a)  alternatives[a].clone(*reality);
      alternatives[a].mutateEnvController(dice);
    }

    // Prepare data folder and set durations
    for (uint a = 0; a < branching; a++) {
      alternatives[a].setDuration(DT, epochDuration);
      alternatives[a].setDataFolder(alternativeDataFolder(epoch, a));
    }

    // Execute alternative simulations in parallel
    #pragma omp parallel for
    for (uint a=0; a<branching; a++) {
      Simulation &s = alternatives[a];

      while (!s.finished()) s.step();

      computeFitnesses(s, pfitnesses[a]);
    }

    // Find 'best' alternative
    std::vector<uint> pFront;
    paretoFront(pfitnesses, pFront);
    winner = *dice(pFront);
    reality = &alternatives[winner];

    // Store result accordingly
    stdfs::rename(reality->dataFolder(), championDataFolder(epoch));

    std::cout << "# " << pFront.size() << " alternatives in pareto front\n#\n";
    for (uint i: pFront) {
      const auto &f = pfitnesses[i];
      for ()
    }

    epoch++;
  } while (epoch < epochs);

  return 0;

#endif
}
