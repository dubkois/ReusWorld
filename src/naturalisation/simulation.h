#ifndef NAT_SIMULATION_H
#define NAT_SIMULATION_H

#include "../simu/simulation.h"

DEFINE_PRETTY_ENUMERATION(NType, NATURAL, ARTIFICIAL, TYPE3)
DEFINE_PRETTY_ENUMERATION(NTag, LHS, RHS, HYB)

namespace simu {
namespace naturalisation {

struct Parameters {
  stdfs::path lhsSimulationFile = "";
  stdfs::path rhsSimulationFile = "";
  std::string loadConstraints = "none";

  float stabilityThreshold = 1e-3;
  uint stabilitySteps = 3;
};

struct Ratios {
  std::array<float, 2> _data;

  void fill (float f) { _data.fill(f);  }

  auto& data (void) {   return _data;   }
  const auto& data (void) const {   return _data;   }

  const float& operator[] (NTag tag) const {
    return _data[EnumUtils<NTag>::underlying_t(tag)];
  }

  float& operator[] (NTag tag) {
    return const_cast<float&>(const_cast<const Ratios*>(this)->operator [](tag));
  }

  Ratios& operator+= (const Ratios &that) {
    for (uint i=0; i<_data.size(); i++)
      this->_data[i] += that._data[i];
    return *this;
  }

  Ratios& operator/= (float f) {
    for (uint i=0; i<_data.size(); i++)
      _data[i] /= f;
    return *this;
  }

  static Ratios fromTag (NTag tag) {
    Ratios r;
    r._data.fill(0);
    r[tag] = 1;
    return r;
  }

  static Ratios fromRatios (const Ratios &lhs, const Ratios &rhs) {
    Ratios r;
    for (NTag t: {NTag::LHS, NTag::RHS})
      r[t] = (lhs[t] + rhs[t]) / 2.f;
    return r;
  }

  friend std::ostream& operator<< (std::ostream &os, const Ratios &r) {
    return os << "{LHS:" << r[NTag::LHS] << ",RHS:" << r[NTag::RHS] << "}";
  }
};

class NatSimulation : public Simulation {
public:
  NatSimulation(void);
  virtual ~NatSimulation (void) = default;

  bool finished (void) const override;

  void step (void) override;

  Plant* addPlant (const PGenome &g, float x, float biomass) override;
  void delPlant (Plant &p, Plant::Seeds &seeds) override;
  void newSeed (const Plant *mother, const Plant *father, GID child) override;
  void stillbornSeed (const Plant::Seed &seed) override;

  auto counts (NTag tag) const {
    return _counts[EnumUtils<NTag>::underlying_t(tag)];
  }

  const auto& ratios (void) const {
    return _ratios;
  }

  const auto& minXRanges (void) const {
    return _xmin;
  }

  const auto& maxXRanges (void) const {
    return _xmax;
  }

  float ratio (void) const;

  struct StatsHeader {
    friend std::ostream& operator<< (std::ostream &os, const StatsHeader &) {
      return os << "date ratio clhs crhs chyb rlhs rrhs"
                   " xllhs xlrhs xlhyb xrlhs xrrhs xrhyb";
    }
  };

  struct Stats {
    const NatSimulation &s;
    friend std::ostream& operator<< (std::ostream &os, const Stats &s) {
      os << s.s.time() << " " << s.s.ratio();
      for (NTag tag: EnumUtils<NTag>::iterator())
        os << " " << s.s.counts(tag);
      for (auto r: s.s.ratios().data())
        os << " " << r;
      for (auto x: s.s.minXRanges())
        os << " " << x;
      for (auto x: s.s.maxXRanges())
        os << " " << x;
      return os;
    }
  };

  static NatSimulation* artificialNaturalisation (const Parameters &params);
  static NatSimulation* naturalNaturalisation (const Parameters &params);

private:
  using PID = phylogeny::GID;
  using Counts = std::array<uint, EnumUtils<NTag>::size()>;
  using Ranges = std::array<float, EnumUtils<NTag>::size()>;

  float _stabilityThreshold;
  uint _stabilitySteps;

  std::map<const Plant*, Ratios> _populations;
  std::map<PID, Ratios> _pendingSeeds;
  Ratios _ratios;
  Counts _counts;
  Ranges _xmin, _xmax;

  float _prevratio;
  uint _stableSteps;

  void commonInit (const Parameters &params);

  void updateCounts (const Ratios &r, int dir);
  void updateRatios (void);
  void updateRanges (void);
};

} // end of namespace naturalisation
} // end of namespace simu

#endif // NAT_SIMULATION_H
