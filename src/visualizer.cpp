#include <QApplication>
#include <QMainWindow>

#include "config/visuconfig.h"
#include "visu/graphicsimulation.h"

#include "kgd/external/cxxopts.hpp"

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  cxxopts::Options options("ReusWorld",
                           "2D simulation of plants in a changing environment");
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

  Verbosity verbosity = Verbosity::SHOW;
  if (result.count("verbosity")) verbosity = result["verbosity"].as<Verbosity>();

  config::Visualization::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::Visualization::printConfig("");

  std::string inputFile = result["genome"].as<std::string>();

  // ===========================================================================
  // == Core setup

  rng::FastDice dice (0);
  genotype::Ecosystem e = inputFile.empty() ?
        genotype::Ecosystem::random(dice)
      : genotype::Ecosystem::fromFile(inputFile);

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QMainWindow w;
  visu::GraphicSimulation s (&w, e);

  w.setWindowTitle("ReusWorld");
  w.setCentralWidget(s.view());
  w.show();

  return a.exec();
}
