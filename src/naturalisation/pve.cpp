#include <csignal>

#include <numeric>

#include <omp.h>

#include "kgd/external/cxxopts.hpp"

#include "pvesimulation.h"
#include "../config/dependencies.h"

using Plant = simu::Plant;
using Simulation = simu::naturalisation::PVESimulation;
using Parameters = simu::naturalisation::Parameters;

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

//  using Verbosity = config::Verbosity;
  Simulation::Overwrite overwrite = Simulation::ABORT;

//  std::string configFile = "auto";  // Default to auto-config
//  Verbosity verbosity = Verbosity::SHOW;

  Parameters params {};

  PVEType type = PVEType::TYPE1;
  stdfs::path subfolder = "tmp/pvp_test/";
  char coverwrite;
  uint stepDuration = 4;

  cxxopts::Options options("ReusWorld (naturalisation test)",
                           "Resilience assertion through naturalisation tests");
  options.add_options()
    ("h,help", "Display help")
//   ("a,auto-config", "Load configuration data from default location")
//    ("c,config", "File containing configuration data",
//     cxxopts::value(configFile))
//    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
//     cxxopts::value(verbosity))
    ("f,folder", "Folder under which to store the results",
     cxxopts::value(subfolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(coverwrite))
    ("load-constraints", "Constraints to apply on dependencies check",
     cxxopts::value(params.loadConstraints))
    ("lhs", "Left hand side simulation",
     cxxopts::value(params.lhsSimulationFile))
    ("env", "Environment for the right hand side",
     cxxopts::value(params.environmentFile))
    ("no-topology", "Whether to deactivate topology output.",
     cxxopts::value(params.noTopology)
     ->default_value("true")->implicit_value("true"))
    ("total-width", "Total width of combined environment",
     cxxopts::value(params.totalWidth)->default_value("100"))
    ("type", "PVE type (unused).", cxxopts::value(type))
    ("step", "Number of years per epoch for the evolved controllers",
     cxxopts::value(stepDuration))
    ("stability-threshold", "Stability threshold",
     cxxopts::value(params.stabilityThreshold))
    ("stability-steps", "Number of stable steps required for convergence",
     cxxopts::value(params.stabilitySteps))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << "\n\nInterpretation of the parameters depend on the value the "
                 "pve type (type):\n"
                 "\tTYPE1: lhs is placed on the left with an empty environment"
                 "controlled by env. Success is measured by the capability to"
                 "colonize the right hand side.\n"
              << std::endl;
    return 0;
  }

  if (params.lhsSimulationFile.empty())
    utils::doThrow<std::invalid_argument>("No lhs simulation provided");

  if (params.environmentFile.empty())
    utils::doThrow<std::invalid_argument>("No rhs simulation provided");

  if (!result.count("folder"))
    subfolder /= EnumUtils<PVEType>::getName(type);

  if (result.count("overwrite"))
    overwrite = simu::Simulation::Overwrite(coverwrite);

  if (stdfs::exists(subfolder) && !stdfs::is_empty(subfolder)) {
    std::cerr << "Base folder " << subfolder << " is not empty!\n";
    switch (overwrite) {
    case simu::Simulation::Overwrite::PURGE:
      std::cerr << "Purging" << std::endl;
      stdfs::remove_all(subfolder);
      break;

    case simu::Simulation::Overwrite::ABORT:
    default:
      std::cerr << "Aborting" << std::endl;
      exit(1);
      break;
    }
  }

  Simulation *s = nullptr;

  switch (type) {
  case PVEType::TYPE1:
    s = Simulation::type1PVE(params);
    break;
  }

  s->setDataFolder(subfolder, overwrite);
  s->setDuration(simu::Environment::DurationSetType::APPEND, stepDuration);

  config::Simulation::saveEvery.ref() = 1;

  std::cout << "Topology is: "
            << (params.noTopology ? "deactivated" : "active") << std::endl;

//  if (result.count("auto-config") && result["auto-config"].as<bool>())
//    configFile = "auto";

//  config::Simulation::setupConfig(configFile, verbosity);
//  config::Simulation::verbosity.ref() = 0;
//  if (configFile.empty()) config::Simulation::printConfig("");

  std::ofstream nstats (subfolder / "evaluation.dat");
  nstats << Simulation::StatsHeader{} << "\n";
  nstats << Simulation::Stats{*s} << "\n";

  std::cout << "Initial score: " << s->score() << "\n"
            << "Convergence requires dL/R <= " << params.stabilityThreshold
            << " for " << params.stabilitySteps << " steps" << std::endl;

  if (!s->time().isStartOfYear()) s->printStepHeader();

  uint epochs = 0;
  while (!s->finished()) {
    if (!s->environment().topology()[0] == 0)
      throw std::logic_error("nope");

    if (s->time().isStartOfYear()) s->printStepHeader();
    s->step();
    nstats << Simulation::Stats{*s} << "\n";

    if (s->timeout() && epochs++ < 25)
      s->setDuration(simu::Environment::DurationSetType::APPEND, stepDuration);
  }
  s->atEnd();

  delete s;
  return 0;
}
