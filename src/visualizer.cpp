#include <QApplication>
#include <QMainWindow>

#include "config/visuconfig.h"
#include "visu/controller.h"

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
    ("g,genome", "Genome to start from", cxxopts::value<std::string>())
    ("r,run", "Immediatly start running. Optionnally specify at which speed",
      cxxopts::value<float>()->implicit_value("1"))
    ("q,auto-quit", "Quit as soon as the simulation ends", cxxopts::value<bool>())
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

  if (!result.count("genome"))
    utils::doThrow<std::invalid_argument>("You must provide a starting genome file!");
  std::string inputFile = result["genome"].as<std::string>();

  float speed = 0.f;
  if (result.count("run"))  speed = result["run"].as<float>();

  // ===========================================================================
  // == Core setup

  genotype::Ecosystem e = genotype::Ecosystem::fromFile(inputFile);
  visu::GraphicSimulation s (e);

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QMainWindow w;
  gui::MainView *v = new gui::MainView(s.environment(), &w);
  visu::Controller c (s, w, v);

  w.show();
  s.init();

  c.nextPlant();
  if (result.count("auto-quit")) c.setAutoQuit(true);

  if (speed > 0) {
    QTimer::singleShot(500, [&c, speed] {
      c.setSpeed(speed);
      c.play(true);
    });
  }

  auto ret = a.exec();

  s.abort();
  s.step();

  return ret;
}
