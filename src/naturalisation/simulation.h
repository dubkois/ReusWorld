#ifndef NAT_SIMULATION_H
#define NAT_SIMULATION_H

#include "../simu/simulation.h"

DEFINE_PRETTY_ENUMERATION(NType, NATURAL, ARTIFICIAL)
DEFINE_PRETTY_ENUMERATION(NTag, LHS, RHS, HYB)

namespace simu {
namespace naturalisation {

struct Parameters {
  stdfs::path lhsSimulationFile = "";
  stdfs::path rhsSimulationFile = "";
  std::string loadConstraints = "none";
};

class NatSimulation : public Simulation {
public:
  NatSimulation(void);
  virtual ~NatSimulation (void) = default;

  Plant* addPlant (const PGenome &g, float x, float biomass) override;
  void delPlant (Plant &p, Plant::Seeds &seeds) override;
  void newSeed (const Plant *mother, const Plant *father, GID child) override;

  const auto& counts (NTag tag) const {
    return _counts[EnumUtils<NTag>::underlying_t(tag)];
  }

  auto& counts (NTag tag) {
    return const_cast<uint&>(const_cast<const NatSimulation*>(this)->counts(tag));
  }

  static NatSimulation* artificialNaturalisation (Parameters &params);
  static NatSimulation* naturalNaturalisation (Parameters &params);

private:
  using PID = phylogeny::GID;
  std::map<const Plant*, NTag> _populations;
  std::array<uint, EnumUtils<NTag>::size()> _counts;

  std::map<PID, NTag> _pendingSeeds;
};

} // end of namespace naturalisation
} // end of namespace simu

#endif // NAT_SIMULATION_H
