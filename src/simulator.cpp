#include <csignal>

#include "simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

/*!
 * TODO Table of todos
 *  TODO trap SIGINT to gracefully exit
 *  TODO see simu/plant.h
 *  TODO Environment
 *    TODO Water supply
 *    TODO Topology
 *  TODO Temperature
 *    TODO In the environment
 *    TODO Effect on plants ?
 */

std::atomic<bool> aborted = false;
void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  aborted = true;
}


int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;
  using Seed_t = rng::AbstractDice::Seed_t;

  cxxopts::Options options("ReusWorld (headless)",
                           "2D simulation of plants in a changing environment (no gui output)");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location", cxxopts::value<bool>())
    ("c,config", "File containing configuration data", cxxopts::value<std::string>())
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(), cxxopts::value<Verbosity>())
    ("g,genome", "Genome to start from", cxxopts::value<std::string>())
    ("r,random", "Random starting genome. Either using provided seed or current"
                 "time", cxxopts::value<Seed_t>()->implicit_value("-1"))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return 0;
  }

  std::string configFile;
  if (result.count("config"))    configFile = result["config"].as<std::string>();
  if (result.count("auto-config"))  configFile = "auto";

  Verbosity verbosity = Verbosity::SHOW;
  if (result.count("verbosity")) verbosity = result["verbosity"].as<Verbosity>();

  config::Simulation::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::Simulation::printConfig("");

  genotype::Ecosystem genome;
  if (result.count("genome") == result.count("random"))
    utils::doThrow<std::invalid_argument>(
      "You must either provide a starting genome or declare a random run");

  else if (result.count("genome")) {
    std::string inputFile = result["genome"].as<std::string>();
    std::cerr << "Reading genome for input file " << inputFile << std::endl;
    genome = genotype::Ecosystem::fromFile(inputFile);

  } else {
    rng::FastDice dice;
    if (result["random"].as<Seed_t>() != Seed_t(-1))
      dice.reset(result["random"].as<Seed_t>());
    std::cerr << "Generating genome from rng seed " << dice.getSeed() << std::endl;
    genome = genotype::Ecosystem::random(dice);
    genome.toFile("last", 2);
  }


  // ===========================================================================
  // == SIGINT management

  struct sigaction act = {};
  act.sa_handler = &sigint_manager;
  if (0 != sigaction(SIGINT, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
  if (0 != sigaction(SIGTERM, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == Core setup

  std::cerr << "Starting from genome: " << genome << std::endl;

  simu::Simulation s (genome);
  s.init();

  while (!s.finished()) {
    if (aborted)  s.abort();
    s.step();
  }

  s.destroy();
  return 0;
}
