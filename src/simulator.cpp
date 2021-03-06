#include <csignal>

#include "simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

#include "config/dependencies.h"

/*!
 * NOTE Table of todos
 *  FIXME PTree noise resilience: Still not shiny
 *  NOTE Binary save : not deterministic (not sure)
 *  TODO Reduce light (clearly not well calibrated)
 *  TODO Find ways to increase morphological complexity
 *   TODO Test if random group promotes vertical competition
 *  TODO Evolutionary algo (timeline picking)
 */

std::atomic<bool> aborted = false;
void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  aborted = true;
}

bool isValidSeed(const std::string& s) {
  return !s.empty()
    && std::all_of(s.begin(),
                    s.end(),
                    [](char c) { return std::isdigit(c); }
    );
}

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  stdfs::path envGenomeArg, plantGenomeArg;
  genotype::Environment envGenome;
  genotype::Plant plantGenome;

  std::string loadSaveFile, loadConstraints = "all", loadFields = "all";

  decltype(genotype::Environment::rngSeed) envOverrideSeed;

  std::string duration = "=100";
  std::string outputFolder = "";
  char overwrite = simu::Simulation::UNSPECIFIED;

  cxxopts::Options options("ReusWorld (headless)",
                           "2D simulation of plants in a changing environment (no gui output)");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))
    ("d,duration", "Simulation duration. ",
     cxxopts::value(duration))
    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))
    ("e,environment", "Environment's genome or a random seed",
     cxxopts::value(envGenomeArg))
    ("p,plant", "Plant genome to start from or a random seed",
     cxxopts::value(plantGenomeArg))
    ("s,env-seed", "Overrides enviroment's seed with provided value",
     cxxopts::value(envOverrideSeed))
    ("l,load", "Load a previously saved simulation",
     cxxopts::value(loadSaveFile))
    ("load-constraints", "Constraints to apply on dependencies check",
     cxxopts::value(loadConstraints))
    ("load-fields", "Individual fields to load",
     cxxopts::value(loadFields))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << "\n\nOption 'auto-config' is the default and overrides 'config'"
                 " if both are provided"
              << "\nEither both 'plant' and 'environment' options are used or "
                 "a valid file is to be provided to 'load' (the former has "
                 "precedance in case all three options are specified)"
              << "\n\n" << config::Dependencies::Help{}
              << "\n\n" << simu::Simulation::LoadHelp{}
              << std::endl;
    return 0;
  }

  bool missingArgument = (!result.count("environment") || !result.count("plant"))
      && !result.count("load");

  if (missingArgument) {
    if (result.count("environment"))
      utils::doThrow<std::invalid_argument>("No value provided for the plant's genome");

    else if (result.count("plant"))
      utils::doThrow<std::invalid_argument>("No value provided for the environment's genome");

    else
      utils::doThrow<std::invalid_argument>(
        "No starting state provided. Either provide both an environment and plant genomes"
        " or load a previous simulation");
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  if (loadSaveFile.empty()) {
    config::Simulation::setupConfig(configFile, verbosity);
    if (configFile.empty()) config::Simulation::printConfig("");

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

    if (isValidSeed(plantGenomeArg)) {
      rng::FastDice dice (std::stoi(plantGenomeArg));
      std::cout << "Generating plant genome from rng seed "
                << dice.getSeed() << std::endl;
      plantGenome = genotype::Plant::random(dice);

    } else {
      std::cout << "Reading plant genome from input file '"
                << plantGenomeArg << "'" << std::endl;
      plantGenome = genotype::Plant::fromFile(plantGenomeArg);
    }

    plantGenome.toFile("last", 2);

    std::cout << "Environment:\n" << envGenome
              << "\nPlant:\n" << plantGenome
              << std::endl;
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

  simu::Simulation s;

  if (!loadSaveFile.empty()) {  // Full load init
    simu::Simulation::load(loadSaveFile, s, loadConstraints, loadFields);
    if (verbosity != Verbosity::QUIET)  config::Simulation::printConfig();

  } else {  // Regular init
    s.init(envGenome, plantGenome);
//    s.periodicSave(); /// FIXME Carefull with this things might, surprisingly,
    /// not be all that initialized
  }

  if (!outputFolder.empty())
    s.setDataFolder(outputFolder, simu::Simulation::Overwrite(overwrite));

  genotype::Plant::printMutationRates(std::cout, 2);

  {
    std::ofstream ofs (s.dataFolder() / "controller.last.tex");
    ofs << "\\documentclass[preview]{standalone}\n"
           "\\usepackage{amsmath}\n"
           "\\begin{document}\n"
        << envGenome.controller.toTex()
        << "\\end{document}\n";
    ofs.close();

    envGenome.controller.toDot(s.dataFolder() / "controller.last.dot",
                               genotype::Environment::CGP::DotOptions::SHOW_DATA);
  }

  if (!duration.empty()) {
    if (duration.size() < 2)
      utils::doThrow<std::invalid_argument>(
        "Invalid duration '", duration, "'. [+|=]<years>");

    uint dvalue = 0;
    if (!(std::istringstream (duration.substr(1)) >> dvalue))
      utils::doThrow<std::invalid_argument>(
        "Failed to parse '", duration.substr(1), "' as a duration (uint)");

    s.setDuration(simu::Environment::DurationSetType(duration[0]), dvalue);
  }

  uint step = 0;
  while (!s.finished()) {
    if (aborted)  s.abort();
    s.step();
    step++;
  }
  s.atEnd();

  s.destroy();
  return 0;
}
