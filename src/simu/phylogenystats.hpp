#ifndef PHYLOGENY_STATS_HPP
#define PHYLOGENY_STATS_HPP

#include "../genotype/plant.h"
#include "types.h"

namespace simu  {
struct Plant;
struct PStats {
  static constexpr auto NaN = std::nanf("");

  using GID = phylogeny::GID;
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

  PStats (void) : PStats(phylogeny::GID::INVALID) {}

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
  }

  friend void from_json (const nlohmann::json &j, PStats &ps) {
    ps = PStats(phylogeny::GID::INVALID);
    ps.id = j["_id"];
    ps.born = j["_isBorn"];
    ps.seed = j["_isSeed"];
    if (!j["pos"][0].is_null()) ps.pos.x = j["pos"][0];
    if (!j["pos"][1].is_null()) ps.pos.y = j["pos"][1];
    ps.birth = j["birth"];
    ps.death = j["death"];
    ps.lifespan = j["lspan"];
    ps.shoot = j["shoot"];
    ps.root = j["root"];
    if (!j["avgT"].is_null()) ps.avgTemperature = j["avgT"];
    if (!j["avgH"].is_null()) ps.avgHygrometry = j["avgH"];
    if (!j["avgL"].is_null()) ps.avgLight = j["avgL"];
  }

  friend void assertEqual (const PStats &lhs, const PStats &rhs,
                           bool deepcopy) {
    using utils::assertEqual;
    assertEqual(lhs.id, rhs.id, deepcopy);
    assertEqual(lhs.born, rhs.born, deepcopy);
    assertEqual(lhs.seed, rhs.seed, deepcopy);
    assertEqual(lhs.pos, rhs.pos, deepcopy);
    assertEqual(lhs.shoot, rhs.shoot, deepcopy);
    assertEqual(lhs.root, rhs.root, deepcopy);
    assertEqual(lhs.birth, rhs.birth, deepcopy);
    assertEqual(lhs.death, rhs.death, deepcopy);
    assertEqual(lhs.lifespan, rhs.lifespan, deepcopy);
    assertEqual(lhs.avgTemperature, rhs.avgTemperature, deepcopy);
    assertEqual(lhs.avgHygrometry, rhs.avgHygrometry, deepcopy);
    assertEqual(lhs.avgLight, rhs.avgLight, deepcopy);
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
