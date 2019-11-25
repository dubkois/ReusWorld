#ifndef NAT_SIMULATION_H
#define NAT_SIMULATION_H

#include "common.h"

DEFINE_PRETTY_ENUMERATION(PVEType, TYPE1)
//DEFINE_PRETTY_ENUMERATION(PVETag, LHS, RHS, HYB)

namespace simu {
namespace naturalisation {

struct Parameters {
  stdfs::path lhsSimulationFile = "";
  std::string loadConstraints = "none";

  stdfs::path environmentFile = "";
  bool noTopology = true;
  float totalWidth = 100;

  float stabilityThreshold = 1e-3;
  uint stabilitySteps = 3;
};

class PVESimulation : public Simulation {
public:
  PVESimulation(void);
  virtual ~PVESimulation (void) = default;

  bool finished (void) const override;

  void step (void) override;

  float score (void) const;

  struct StatsHeader {
    friend std::ostream& operator<< (std::ostream &os, const StatsHeader &) {
      return os << "date score";
    }
  };

  struct Stats {
    const PVESimulation &s;
    friend std::ostream& operator<< (std::ostream &os, const Stats &s) {
      os << s.s.time() << " " << s.s.score();
      return os;
    }
  };

  static PVESimulation* type1PVE (const Parameters &params);

private:
  using PID = phylogeny::GID;

  static constexpr auto R = 100;
  using Regions = std::bitset<R>;
  Regions _regions;

  float _stabilityThreshold;
  uint _stabilitySteps;

  float _prevscore;
  uint _stableSteps;

  void commonInit (const Parameters &params);

  void updateRegions (void);
};

} // end of namespace naturalisation
} // end of namespace simu

#endif // NAT_SIMULATION_H
