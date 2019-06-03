#include "kgd/external/cxxopts.hpp"

#include "../simu/simulation.h"

#include "../config/dependencies.h"
using Simulation = simu::Simulation;

int main (int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  std::vector<std::string> saveFiles;
  std::string loadConstraints;

  cxxopts::Options options("ReusWorld (save equal asserter)",
                           "Asserts that both provided save files are equal");
  options.add_options()
    ("h,help", "Display help")
    ("saves", "Save files to compare", cxxopts::value(saveFiles))
    ("load-constraints", "Constraints to apply on dependencies check",
     cxxopts::value(loadConstraints))
    ;

  options.parse_positional("saves");
  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << "\n\n" << config::Dependencies::Help{}
              << std::endl;
    return 0;
  }

  if (saveFiles.size() != 2)
    utils::doThrow<std::invalid_argument>("Not enough save file(s) provided.");

  for (uint i=0; i<2; i++)
    if (!stdfs::exists(saveFiles[i]))
      utils::doThrow<std::invalid_argument>(
        "File ", i, " '", saveFiles[i], "' does not exits");

  // ===========================================================================
  // == Core setup

  Simulation lhs, rhs;
  Simulation::load(saveFiles[0], lhs, loadConstraints);
  Simulation::load(saveFiles[1], rhs, loadConstraints);

  assertEqual(lhs, rhs);

  lhs.destroy();
  rhs.destroy();
  return 0;
}
