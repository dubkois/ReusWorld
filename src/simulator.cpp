#include "simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

/*!
 * TODO Table of todos
 *  TODO Environment
 *    TODO Water supply
 *    TODO Topology
 *  TODO Temperature
 *    TODO In the environment
 *    TODO Effect on plants ?
 */




int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  cxxopts::Options options("ReusWorld (headless)",
                           "2D simulation of plants in a changing environment (no gui output)");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location", cxxopts::value<bool>())
    ("c,config", "File containing configuration data", cxxopts::value<std::string>())
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(), cxxopts::value<Verbosity>())
    ("g,genome", "Genome to start from", cxxopts::value<std::string>()->default_value(""))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return 0;
  }

  std::string configFile;
  if (result.count("config"))    configFile = result["config"].as<std::string>();
  if (result.count("auto-config"))  configFile = "auto";

  Verbosity verbosity = Verbosity::QUIET;
  if (result.count("verbosity")) verbosity = result["verbosity"].as<Verbosity>();

  config::Simulation::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::Simulation::printConfig("");

  std::string inputFile = result["genome"].as<std::string>();

  // ===========================================================================
  // == Core setup

  rng::FastDice dice (0);
  genotype::Ecosystem e = inputFile.empty() ?
        genotype::Ecosystem::random(dice)
      : genotype::Ecosystem::fromFile(inputFile);

  simu::Simulation s (e);
  s.init();

  while (!s.finished())
    s.step();

  return 0;
}
