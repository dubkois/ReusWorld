#include <csignal>

#include <numeric>

#include <omp.h>

#include "kgd/external/cxxopts.hpp"

#include "pvpsimulation.h"
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

  PVPType ntype = PVPType::ARTIFICIAL;
  stdfs::path subfolder = "tmp/naturalisation_test/";
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
    ("rhs", "Right hand side simulation",
     cxxopts::value(params.rhsSimulationFile))
    ("ntype", "Naturalisation type (ARTIFICIAL or NATURAL)."
              " Defaults to the first",
     cxxopts::value(ntype))
    ("no-topology", "Whether to deactivate topology output.",
     cxxopts::value(params.noTopology))
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
                 "naturalisation type (ntype):\n"
                 "\tARTIFICIAL: lhs is the \"at home\" simulation whose"
                 " environmental controller and values will be used. rhs is the"
                 " invader. The population is created by taking the seeds of"
                 " both sides.\n"
                 "\t   NATURAL: Simulations are put side-by-side allowing for"
                 " passive invasion. lhs is on the left.\n"
              << std::endl;
    return 0;
  }

  if (params.lhsSimulationFile.empty())
    utils::doThrow<std::invalid_argument>("No lhs simulation provided");

  if (params.rhsSimulationFile.empty())
    utils::doThrow<std::invalid_argument>("No rhs simulation provided");

  if (!result.count("folder"))
    subfolder /= EnumUtils<PVPType>::getName(ntype);

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

  switch (ntype) {
  case PVPType::ARTIFICIAL:
    s = Simulation::artificialNaturalisation(params);
    break;

  case PVPType::NATURAL:
    s = Simulation::naturalNaturalisation(params);
    break;
  }

  s->setDataFolder(subfolder, overwrite);
  s->setDuration(simu::Environment::DurationSetType::APPEND, stepDuration);

  config::Simulation::saveEvery.ref() = 1;

//  if (result.count("auto-config") && result["auto-config"].as<bool>())
//    configFile = "auto";

//  config::Simulation::setupConfig(configFile, verbosity);
//  config::Simulation::verbosity.ref() = 0;
//  if (configFile.empty()) config::Simulation::printConfig("");

  std::ofstream nstats (subfolder / "naturalisation.dat");
  nstats << Simulation::StatsHeader{} << "\n";
  nstats << Simulation::Stats{*s} << "\n";

  std::cout << "Initial counts: "
            << s->counts(PVPTag::LHS) << " (lhs), "
            << s->counts(PVPTag::RHS) << " (rhs), "
            << s->counts(PVPTag::HYB) << " (hyb)\n"
            << "L/R ratio: " << s->ratio() << "\n"
            << "Convergence requires dL/R <= " << params.stabilityThreshold
            << " for " << params.stabilitySteps << " steps" << std::endl;


  if (!s->time().isStartOfYear()) s->printStepHeader();

  uint epochs = 0;
  while (!s->finished()) {
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
