#ifndef PHYLOGENY_STATS_HPP
#define PHYLOGENY_STATS_HPP

#include "../genotype/plant.h"
#include "types.h"

namespace simu  {
struct Plant;
struct PStats {
  static constexpr auto NaN = std::nanf("");

  using GID = genotype::BOCData::GID;
  GID id;
  bool born;
  bool seed;

  Point pos;

  std::string shoot, root;

  Time birth, death;
  float lifespan;

  float avgTemperature;
  float avgHygrometry;
  float avgLight;

  Plant *plant;

  PStats (GID id) : id(id), born(false), seed(true),
    pos{NaN, NaN}, shoot(""), root(""),
    birth(), death(), lifespan(0),
    avgTemperature(NaN), avgHygrometry(NaN), avgLight(NaN),
    plant(nullptr) {}

  void removedFromEnveloppe (void);

  friend void to_json (nlohmann::json &j, const PStats &ps) {
    j["_id"] = ps.id;
    j["_isBorn"] = ps.born;
    j["_isSeed"] = ps.seed;
    j["pos"] = { ps.pos.x, ps.pos.y };
    j["birth"] = ps.birth;
    j["death"] = ps.death;
    j["lspan"] = ps.lifespan;
    j["shoot"] = ps.shoot;
    j["root"] = ps.root;
    j["avgT"] = ps.avgTemperature;
    j["avgH"] = ps.avgHygrometry;
    j["avgL"] = ps.avgLight;

//    assert(!std::isnan(ps.pos.x));
  }

  friend void from_json (const nlohmann::json &j, PStats &ps) {
    ps = PStats(genotype::BOCData::INVALID_GID);
    ps.id = j["_id"];
    ps.born = j["_isBorn"];
    ps.seed = j["_isSeed"];
    ps.pos = { j["pos"][0], j["pos"][1] };
    ps.birth = j["birth"];
    ps.death = j["death"];
    ps.lifespan = j["lspan"];
    ps.shoot = j["shoot"];
    ps.root = j["root"];
    ps.avgTemperature = j["avgT"];
    ps.avgHygrometry = j["avgH"];
    ps.avgLight = j["avgL"];
  }
};

struct PStatsWorkingCache {
  float largestBiomass;
  float sumTemperature;
  float sumHygrometry;
  float sumLight;

  float tmpSum;

  PStatsWorkingCache (void)
    : largestBiomass(0), sumTemperature(0), sumHygrometry(0), sumLight(0),
      tmpSum(0) {}
};

} // end of namespace simu

#endif // PHYLOGENY_STATS_HPP
