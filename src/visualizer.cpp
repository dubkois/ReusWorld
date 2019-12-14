#include <QApplication>
#include <QMainWindow>
#include <QSettings>

#include "kgd/external/cxxopts.hpp"

#include "visu/controller.h"

#include "config/dependencies.h"

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

  std::string envGenomeArg, plantGenomeArg;

  genotype::Environment envGenome;
  genotype::Plant plantGenome;

  std::string loadSaveFile, loadConstraints, loadFields;

  std::string morphologiesSaveFolder, screenshotSaveFile;

  int speed = 0;
  bool autoQuit = false;

  std::string duration = "=100";
  std::string outputFolder = "";
  char overwrite = simu::Simulation::UNSPECIFIED;

  cxxopts::Options options("ReusWorld",
                           "2D simulation of plants in a changing environment");
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
    ("l,load", "Load a previously saved simulation",
     cxxopts::value(loadSaveFile))
    ("load-constraints", "Constraints to apply on dependencies check",
     cxxopts::value(loadConstraints))
    ("load-fields", "Individual fields to load",
     cxxopts::value(loadFields))
    ("r,run", "Immediatly start running. Optionnally specify at which speed",
      cxxopts::value(speed))
    ("q,auto-quit", "Quit as soon as the simulation ends",
     cxxopts::value(autoQuit))
    ("collect-morphologies", "Save morphologies in the provided folder",
     cxxopts::value(morphologiesSaveFolder))
    ("screenshot", "Save simulation state after initialisation/loading in"
                   "specified file",
     cxxopts::value(screenshotSaveFile))
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

//  if (!morphologiesSaveFolder.empty() && loadSaveFile.empty())
//    utils::doThrow<std::invalid_argument>(
//        "Generating morphologies is only meaningful when loading a simulation");

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QSettings settings ("kgd", "ReusWorld");

  visu::GraphicSimulation s;

  QMainWindow *w = new QMainWindow;
  gui::MainView *v = new gui::MainView(s.environment(), w);
  visu::Controller c (s, *w, v);

  if (loadSaveFile.empty()) {    
    config::Simulation::setupConfig(configFile, verbosity);
    if (configFile.empty()) config::Simulation::printConfig("");
    genotype::Plant::printMutationRates(std::cout, 2);

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

    s.init(envGenome, plantGenome);

  } else {
    visu::GraphicSimulation::load(loadSaveFile, s, loadConstraints, loadFields);
    if (verbosity != Verbosity::QUIET)
      config::Simulation::printConfig();
  }

  if (!outputFolder.empty())
    s.setDataFolder(outputFolder, simu::Simulation::Overwrite(overwrite));

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

  // Load settings
  w->setGeometry(settings.value("geometry").toRect());

  int ret = 0;
  if (!morphologiesSaveFolder.empty()) {
    if (!stdfs::exists(morphologiesSaveFolder)) {
      stdfs::create_directories(morphologiesSaveFolder);
      std::cout << "Created folder(s) " << morphologiesSaveFolder
                << " for morphologies storage" << std::endl;
    }

    // If just initialised, let it have one step to show its morphology
    if (loadSaveFile.empty())
      c.step();

    v->saveMorphologies(QString::fromStdString(morphologiesSaveFolder));

  } else if (!screenshotSaveFile.empty()) {
    auto img = v->fullScreenShot();
    img.save(QString::fromStdString(screenshotSaveFile));
    std::cout << "Saved img of size " << img.width() << "x" << img.height()
              << " into " << screenshotSaveFile << std::endl;

  } else {  // Regular simulation
    w->setAttribute(Qt::WA_QuitOnClose);
    w->show();

    c.nextPlant();
    c.setAutoQuit(autoQuit);

    // Setup debug config
    if (config::Simulation::DEBUG_NO_METABOLISM())
      c.step();

    if (speed != 0) {
      QTimer::singleShot(500, [&c, speed] {
        if (speed > 0)  c.play(true);
        c.setSpeed(std::abs(speed));
      });
    }

    ret = a.exec();

    s.abort();
    s.step();
  }

  if (w->geometry().isValid()) {
//    std::cerr << "Saving " <<
    settings.setValue("geometry", w->geometry());
  }

  return ret;
}
