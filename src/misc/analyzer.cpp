#include "kgd/external/cxxopts.hpp"

#include "../simu/simulation.h"

#include "../config/dependencies.h"

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

void finalCounts (const Simulation &simu) {
  std::cout << "GID: " << simu::Plant::ID(simu.gidManager()) << "\n"
            << "SID: " << simu.phylogeny().nextNodeID() << std::endl;
}

void extractField (const Simulation &simu,
                   const std::vector<std::string> &fields) {
//  std::cout << "Extracting field '" << field << "'..." << std::endl;
  for (const auto &pair: simu.plants()) {
    for (const auto &field: fields) {
      if (field == ".morphology")
        std::cout << field << ".shoot: "
                  << pair.second->toString(simu::Plant::Layer::SHOOT) << "\n"
                  << field << ".root: "
                  << pair.second->toString(simu::Plant::Layer::ROOT) << "\n";

      else if (field == ".boundingBox")
        std::cout << field << ": " << pair.second->translatedBoundingRect()
                  << "\n";

      else
        std::cout << field << ": " << pair.second->genome().getField(field)
                  << "\n";
    }
  }
  std::cout << std::endl;
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
  for (const auto &pair: simu.plants()) {
    simu::Plant &p = *pair.second;
    Range &r = ranges[p.species()];
    const simu::Rect b = p.boundingRect().translated(p.pos());
    if (b.l() < r.min)  r.min = b.l();
    if (r.max < b.r())  r.max = b.r();
    r.count++;
  }

  std::vector<std::pair<SID,Range>> sortedRanges;
  for (const auto &pair: ranges)  sortedRanges.push_back({pair});
  std::sort(sortedRanges.begin(), sortedRanges.end(),
            [] (const auto &lhs, const auto &rhs) {
    if (lhs.second.count != rhs.second.count)
      return lhs.second.count > rhs.second.count;
    return lhs.first > rhs.first;
  });

  stdfs::path datafile = replaceFullExtension(savefile, ".ranges.dat");
  uint i=0;
  std::ofstream ofs (datafile);
  ofs << "SID Center Population Width\n";
  for (const auto &pair: sortedRanges) {
//    if (i++ > 15) break;
    const Range &r = pair.second;
    ofs << pair.first << " " << (r.max + r.min) / 2.f << " " << r.count
        << " " << (r.max - r.min) << "\n";
  }
  ofs.close();

  stdfs::path outfile = datafile;
  outfile.replace_extension(".png");

  auto w = simu.environment().xextent();
  std::ostringstream cmdOSS;
  cmdOSS << "gnuplot -e \""
              "set term pngcairo size 1680,1050;\n"
              "set output '" << outfile.string() << "';\n"
              "set style fill transparent solid .2 border -1;\n"
              "set style textbox opaque noborder;\n"
              "set xrange [" << -w << ":" << w << "];\n"
              "set xlabel 'Position';\n"
              "set ylabel 'Population';\n"
              "plot '" << datafile.string() << "' using 2:3:4 with boxes notitle,"
                  " '' u 2:(\\$3):(\\$1) with labels boxed notitle;\"\n";

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
    counts[p.second->species()]++;

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
  std::cout << "Generating compatibility matrix...\r" << std::flush;

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

  bool fullMatrix = sids.empty();
  uint i = 0, total = simu.plants().size();
  std::map<SID, std::set<genotype::BOCData::Sex>> sexes;
  std::map<SID, std::vector<const simu::Plant*>> species;
  for (const auto &p: simu.plants()) {
    std::cout << "[" << std::setw(3) << 100 * (++i) / total
              << "%] Examining GID: " << p.second->id() << std::flush;

    SID sid = p.second->species();
    std::cout << " (SID: " << sid << ") ...\r" << std::flush;

    if (fullMatrix || sids.find(sid) != sids.end())
      species[sid].push_back(p.second.get());
    if (fullMatrix) sids.insert(sid);
    sexes[sid].insert(p.second->sex());
  }

  std::set<SID> ignoredSpecies;
  for (const auto &s: sexes) {
    if (s.second.size() < 2 && sids.find(s.first) != sids.end()) {
      sids.erase(s.first);
      species.erase(s.first);
      ignoredSpecies.insert(s.first);
    }
  }

  i = 0;  total = species.size() * species.size();
  std::map<SID, std::map<SID,float>> compats;
  for (const auto &lhsSpecies: species) {
    const auto &lhsPlants = lhsSpecies.second;

    for (const auto &rhsSpecies: species) {
      const auto &rhsPlants = rhsSpecies.second;

      float compatSum = 0;
      uint compatCount = 0;

      std::cout << "[" << std::setw(3)
                << 100 * (++i) / total << "%] Computing "
                << lhsSpecies.first << "x" << rhsSpecies.first
                << "\r" << std::flush;

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

      compats[lhsSpecies.first][rhsSpecies.first] = compatSum / compatCount;
    }
  }

  std::cout << "                                                             \r";
  std::cout << "Ignoring " << ignoredSpecies.size() << " species:";
  for (SID sid: ignoredSpecies) std::cout << " " << sid;
  std::cout << "\n";
  std::cout << "cm " << std::string(maxLength-1, ' ') << "-";
  for (SID sid: sids) std::cout << " " << std::setw(maxLength) << sid;
  std::cout << "\n";
  for (SID sidLhs: sids) {
    std::cout << "cm " << std::setw(maxLength) << sidLhs;

    const auto &compats_ = compats.at(sidLhs);
    for (SID sidRhs: sids)
      std::cout << " " << std::setw(maxLength) << std::setprecision(3)
                << compats_.at(sidRhs);

    std::cout << "\n";
  }
  std::cout << std::endl;

//  std::cout << "plotData: Timestamp";
//  for (const auto &p: compats)
//    for (const auto &p_: p.second)
//    std::cout << " " << p.first << "-" << p_.first;
//  std::cout << "\nplotData: " << simu.time().pretty();
//  for (const auto &p: compats)
//    for (const auto &p_: p.second)
//    std::cout << " " << p_.second;
//  std::cout << "\n" << std::endl;

  std::cout << "rc " << std::string(maxLength-1, ' ') << "-";
  for (SID sid: sids) std::cout << " " << std::setw(maxLength) << sid;
  std::cout << "\n";
  for (SID sidLhs: sids) {
    std::cout << "rc " << std::setw(maxLength) << sidLhs;

    const auto &compats_ = compats.at(sidLhs);
    for (SID sidRhs: sids)
      std::cout << " " << std::setw(maxLength) << std::setprecision(3)
                << 100 * compats_.at(sidRhs) / compats_.at(sidLhs);

    std::cout << "\n";
  }
  std::cout << std::endl;
}

void computeFlowerRobustnessScore (const Simulation &s, char /*type*/) {
  using Organ = simu::Organ;
  using SSizes = std::map<const Organ*, uint>;
  using SSizesLamda = std::function<uint(SSizes&, const Organ*)>;
  static const SSizesLamda computeSubtreesSize =
    [] (SSizes &ssize, const Organ *o) {
    uint sum = 0;
    for (const Organ *c: o->children())
      sum += computeSubtreesSize(ssize, c);
    ssize[o] = sum;
    return sum + o->isFlower();
  };

  using Scores = std::array<float, 2>;
  Scores globalScores {0};
  uint validPlants = 0;

  for (auto &ppair: s.plants()) {
    const simu::Plant &p = *ppair.second;

    if (p.isInSeedState())    continue;
    if (p.flowers().empty())  continue;
    if (!p.fruits().empty())  continue;

    SSizes subtreesSize;
    for (const Organ *o: p.organs())
      if (!o->parent() && o->layer() == simu::Plant::Layer::SHOOT)
        computeSubtreesSize(subtreesSize, o);

    uint flowers = p.flowers().size();

    float score = 0;
    for (const Organ *f: p.flowers()) {
      float fscore = (flowers - subtreesSize.at(f)) / float(flowers);
      score += fscore;

//      std::cout << simu::OrganID(f) << ": ("
//                << flowers << " - " << subtreesSize.at(f) << ") / " << flowers
//                << " = " << fscore << "\n";
    }

    globalScores[0] += score;
    globalScores[1] += score / flowers;
    validPlants++;
//    std::cout << simu::PlantID(&p) << ": " << score << "\n";
  }

  std::cout << "FlowerRobustness:";
  for (float s: globalScores) std::cout << " " << s / validPlants;
  std::cout << std::endl;
}

int main (int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string loadSaveFile, loadConstraints, loadFields;

  bool doFinalCounts = false;
  std::vector<std::string> viewFields;
  bool doSpeciesRanges = false;
  bool doDensityHistogram = false;
  std::string compatMatrixSIDList;
  char flowerMarkingType = '\0';
  std::string popOutFile, ptreeOutFile;

  cxxopts::Options options("ReusWorld (analyzer)",
                           "Performs computation on a saved reus world simulation");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))
    ("l,load", "Load a previously saved simulation",
     cxxopts::value(loadSaveFile))
    ("load-constraints", "Constraints to apply on dependencies check",
     cxxopts::value(loadConstraints))
    ("load-fields", "Individual fields to load",
     cxxopts::value(loadFields))

    ("final-counts", "Extracts number of generated plants (GID) and species (SID)",
     cxxopts::value(doFinalCounts))
    ("extract-field",
     "Extracts field from the plant population (repeatable option)",
     cxxopts::value(viewFields))
    ("species-ranges", "Plots the occupied ranges",
     cxxopts::value(doSpeciesRanges))
    ("density-histogram", "Plots the population count per species",
     cxxopts::value(doDensityHistogram))
    ("compatibility-matrix", "Computes average compatibilities between the "
                             "specified species",
     cxxopts::value(compatMatrixSIDList))
    ("flower-robustness", "Computes the flower robustness score"
                          " (flower count weighted by probability of causing"
                          " cascading organ loss)",
     cxxopts::value(flowerMarkingType))
    ("extract-pop", "Save population to specified location",
     cxxopts::value(popOutFile))
    ("extract-tree", "Save ptree to specified location",
     cxxopts::value(ptreeOutFile))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << "\n\n" << config::Dependencies::Help{}
              << "\n\n" << simu::Simulation::LoadHelp{}
              << std::endl;
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

  Simulation::load(loadSaveFile, s, loadConstraints, loadFields);

//  uint i=0;
//  std::vector<genotype::Plant> testgenomes;
//  for (const auto &p: s.plants()) {
//    testgenomes.push_back(p.second->genome());
//    if (i++ > 15) break;
//  }
//  genotype::Plant::aggregate(std::cout, testgenomes);
//  exit (255);

  if (doFinalCounts)    finalCounts(s);

  if (!viewFields.empty())  extractField(s, viewFields);

  if (doSpeciesRanges) plotSpeciesRanges(s, loadSaveFile);
  if (doDensityHistogram) plotDensityHistogram(s, loadSaveFile);

  if (result.count("compatibility-matrix"))
    compatibilityMatrix(s, compatMatrixSIDList);

  if (flowerMarkingType != '\0')
    computeFlowerRobustnessScore(s, flowerMarkingType);

  if (!popOutFile.empty()) {
    std::ofstream ofs (popOutFile);
    if (!ofs)
      utils::doThrow<std::invalid_argument>("Unable to open ", popOutFile);
    ofs << s.serializePopulation();
    std::cout << "Saved population to " << popOutFile << std::endl;
  }

  if (!ptreeOutFile.empty())  s.phylogeny().saveTo(ptreeOutFile);

  s.destroy();
  return 0;
}
