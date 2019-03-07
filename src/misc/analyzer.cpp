#include "../simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

using SID = phylogeny::SID;
using Sex = simu::Plant::Sex;
using Simulation = simu::Simulation;

struct Count {
  SID sid;
  uint count;
  friend bool operator< (const Count &lhs, const Count &rhs) {
    if (lhs.count != rhs.count) return lhs.count > rhs.count;
    return lhs.sid > rhs.sid;
  }
};


void extractField (const Simulation &simu, const std::string &field) {
  std::cout << "Extracting 'field'..." << std::endl;
  std::string jqcmd = " | jq '" + field + "'";
  for (const auto &pair: simu.plants()) {
    genotype::Plant p = pair.second->genome();
    std::string cmd = "echo '" + p.dump() + "'" + jqcmd;
    system(cmd.c_str());
  }
}

stdfs::path replaceFullExtension (const stdfs::path &p, const std::string &ext) {
  stdfs::path out = p.parent_path();
  stdfs::path stem = p.filename();
  while (!stem.extension().empty()) stem = stem.stem();
  out /= stem;
  out += ext;
  return out;
}

void plotSpeciesRanges (const Simulation &simu, const stdfs::path &savefile) {
  struct Range {
    float min = std::numeric_limits<float>::max(), max = -min;
    uint count = 0;
  };
  std::map<SID, Range> ranges;
  const auto &phylogeny = simu.phylogeny();
  for (const auto &pair: simu.plants()) {
    simu::Plant &p = *pair.second;
    Range &r = ranges[phylogeny.getSpeciesID(p.id())];
    const simu::Rect b = p.boundingRect().translated(p.pos());
    if (b.l() < r.min)  r.min = b.l();
    if (r.max < b.r())  r.max = b.r();
    r.count++;
  }

  stdfs::path datafile = replaceFullExtension(savefile, ".ranges.dat");
  uint i=0;
  std::ofstream ofs (datafile);
  ofs << "SID Center Population Width\n";
  for (const auto &pair: ranges) {
    if (i++ > 15) break;
    const Range &r = pair.second;
    ofs << pair.first << " " << (r.max + r.min) / 2.f << " " << r.count
        << " " << (r.max - r.min) << "\n";
  }
  ofs.close();

  stdfs::path outfile = datafile;
  outfile.replace_extension(".png");

  std::ostringstream cmdOSS;
  cmdOSS << "gnuplot -e \""
              "set term pngcairo size 1680,1050;\n"
              "set output '" << outfile.string() << "';\n"
              "set style fill transparent solid .2 border -1;\n"
              "set xlabel 'Position';\n"
              "set ylabel 'Population';\n"
              "plot '" << datafile.string() << "' using 2:3:4 with boxes notitle,"
                  " '' u 2:(\\$3):(\\$1) with labels notitle;\"\n";

  std::string cmd = cmdOSS.str();
  std::cout << "Executing\n" << cmd << std::endl;

  if (0 != system(cmd.c_str()))
    std::cerr << "Error in generating plot..." << std::endl;
  else
    std::cout << "Generated '" << outfile.string() << "'" << std::endl;
}

void plotDensityHistogram (const Simulation &simu, const stdfs::path &savefile) {
  std::cout << "Generating density histogram..." << std::endl;

  const float plantCount = simu.plants().size();
  std::map<SID, uint> counts;
  const auto &phylogeny = simu.phylogeny();
  for (const auto &p: simu.plants())
    counts[phylogeny.getSpeciesID(p.second->id())]++;

  std::set<Count> sortedCounts;
  for (auto &p: counts) sortedCounts.insert({p.first, p.second});

  stdfs::path outfile = replaceFullExtension(savefile, ".density.png");

  stdfs::path datafile = outfile;
  datafile.replace_extension(".dat");
  std::ofstream ofs (datafile);
  ofs << "SID Count Ratio Appearance Extinction\n";
  uint i=0;
  for (auto &c: sortedCounts) {
    if (i++ > 15) break;
    float r = 100 * float(c.count) / plantCount;
    auto data = phylogeny.nodeAt(c.sid)->data;
    ofs << c.sid << " " << c.count << " " << r
        << " " << data.firstAppearance << " " << data.lastAppearance
        << "\n";
  }
  ofs.close();

  std::ostringstream cmdOSS;
  cmdOSS << "gnuplot -e \""
              "set term pngcairo size 1680,1050;\n"
              "set output '" << outfile.string() << "';\n"
              "set style data histogram;\n"
              "set style histogram cluster gap 1;\n"
              "set style fill solid border -1;\n"
              "set boxwidth 0.9;\n"
              "set xlabel 'Species';\n"
              "set ylabel 'Frequency (%)';\n"
              "plot '" << datafile.string() << "' using 3:xtic(1) notitle;\"\n";

  std::string cmd = cmdOSS.str();
  std::cout << "Executing\n" << cmd << std::endl;

  if (0 != system(cmd.c_str()))
    std::cerr << "Error in generating plot..." << std::endl;
  else
    std::cout << "Generated '" << outfile.string() << "'" << std::endl;
}

void compatibilityMatrix (const Simulation &simu, const std::string &sidList) {
  std::cout << "Generating compatibility matrix..." << std::endl;

  int maxLength = 7;
  std::set<SID> sids;
  for (const std::string ssid: utils::split(sidList, ',')) {
    std::istringstream iss (ssid);
    uint isid;
    iss >> isid;
    if (!iss)
      utils::doThrow<std::invalid_argument>(
        "Unable to parse '", sidList, "' into a collection of species ids");
    sids.insert(SID(isid));

    int l = std::ceil(std::log10(isid));
    if (maxLength < l)  maxLength = l;
  }

  const auto &ptree = simu.phylogeny();
  std::map<SID, std::vector<const simu::Plant*>> plants;
  for (const auto &p: simu.plants()) {
    SID sid = ptree.getSpeciesID(p.second->id());
    if (sids.find(sid) != sids.end())
      plants[sid].push_back(p.second.get());
  }

  std::map<std::pair<SID,SID>, float> compats;
  for (const auto &lhsSpecies: plants) {
    const auto &lhsPlants = lhsSpecies.second;

    for (const auto &rhsSpecies: plants) {
      const auto &rhsPlants = rhsSpecies.second;

      float compatSum = 0;
      uint compatCount = 0;

      for (const simu::Plant *lhsPlant: lhsPlants) {
        if (lhsPlant->sex() != Sex::FEMALE)  continue;
        const auto &lhs = lhsPlant->genome();

        for (const simu::Plant *rhsPlant: rhsPlants) {
          if (rhsPlant->sex() != Sex::MALE)  continue;
          const auto &rhs = rhsPlant->genome();

          compatSum += lhs.compatibility(distance(lhs, rhs));
          compatCount++;
        }
      }

      compats.try_emplace({lhsSpecies.first, rhsSpecies.first},
                          compatSum / compatCount);
    }
  }

  std::cout << std::string(maxLength, ' ');
  for (SID sid: sids) std::cout << " " << std::setw(maxLength) << sid;
  std::cout << "\n";
  for (SID sidLhs: sids) {
    std::cout << std::setw(maxLength) << sidLhs;

    for (SID sidRhs: sids)
      std::cout << " " << std::setw(maxLength) << std::setprecision(3)
                << compats.at({sidLhs, sidRhs});

    std::cout << "\n";
  }
  std::cout << std::endl;

  std::cout << "plotData: Timestamp";
  for (const auto &p: compats)
    std::cout << " " << p.first.first << "-" << p.first.second;
  std::cout << "\nplotData: " << simu.time().pretty();
  for (const auto &p: compats)
    std::cout << " " << p.second;
  std::cout << std::endl;
}

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string loadSaveFile;

  std::string viewField;
  bool doSpeciesRanges = false;
  bool doDensityHistogram = false;
  std::string compatMatrixSIDList;

  cxxopts::Options options("ReusWorld (analyzer)",
                           "Performs computation on a saved reus world simulation");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))
    ("l,load", "Load a previously saved simulation", cxxopts::value(loadSaveFile))
    ("view-field", "Extracts field from the plant population",
     cxxopts::value(viewField))
    ("species-ranges", "Plots the occupied ranges",
     cxxopts::value(doSpeciesRanges))
    ("density-histogram", "Plots the population count per species",
     cxxopts::value(doDensityHistogram))
    ("compatibility-matrix", "Computes average compatibilities between the "
                             "specified species",
     cxxopts::value(compatMatrixSIDList))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (!result.count("load"))
    utils::doThrow<std::invalid_argument>("No save file provided.");

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  config::Simulation::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::Simulation::printConfig("");

  // ===========================================================================
  // == Core setup

  Simulation s;

  std::cout << "Loading '" << loadSaveFile << "' ..." << std::endl;
  Simulation::load(loadSaveFile, s);

  if (!viewField.empty()) extractField(s, viewField);

  if (doSpeciesRanges) plotSpeciesRanges(s, loadSaveFile);
  if (doDensityHistogram) plotDensityHistogram(s, loadSaveFile);

  if (!compatMatrixSIDList.empty())
    compatibilityMatrix(s, compatMatrixSIDList);

  s.destroy();
  return 0;
}