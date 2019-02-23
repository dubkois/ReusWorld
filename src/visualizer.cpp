#include <QApplication>
#include <QMainWindow>

#include "config/visuconfig.h"
#include "visu/controller.h"

#include "kgd/external/cxxopts.hpp"

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  std::string envGenomeFile, plantGenomeFile;

  float speed = 0.f;
  bool autoQuit = false;

  cxxopts::Options options("ReusWorld",
                           "2D simulation of plants in a changing environment");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "[2;D] Load configuration data from default location")
    ("c,config", "[2] File containing configuration data", cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(), cxxopts::value(verbosity))
    ("e,environment", "[M] Environment's genome", cxxopts::value(envGenomeFile))
    ("p,plant", "[M] Plant genome to start from", cxxopts::value(plantGenomeFile))
    ("r,run", "Immediatly start running. Optionnally specify at which speed",
      cxxopts::value(speed))
    ("q,auto-quit", "Quit as soon as the simulation ends", cxxopts::value(autoQuit))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
      std::cout << options.help() << std::endl;
      return 0;
  }

  if (!result.count("environment"))
    utils::doThrow<std::invalid_argument>("No value provided for the environment's genome");

  if (!result.count("plant"))
    utils::doThrow<std::invalid_argument>("No value provided for the plant's genome");

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  config::Visualization::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::Visualization::printConfig("");

  // ===========================================================================
  // == Core setup

  genotype::Environment e = genotype::Environment::fromFile(envGenomeFile);
  genotype::Plant p = genotype::Plant::fromFile(plantGenomeFile);
  visu::GraphicSimulation s;

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QMainWindow w;
  gui::MainView *v = new gui::MainView(s.environment(), &w);
  visu::Controller c (s, w, v);

  w.show();
  s.init(e, p);

  c.nextPlant();
  c.setAutoQuit(autoQuit);

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
