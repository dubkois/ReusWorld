#ifndef ENVCGP_HPP
#define ENVCGP_HPP

#include "kgd/settings/prettyenums.hpp"
#include "kgd/genotype/selfawaregenome.hpp"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(
  genotype::env_controller, Inputs,
    DAY, YEAR, COORDINATE,                // [0:1]
    ALTITUDE, TEMPERATURE, HYGROMETRY     // [-1:1]
)

DEFINE_NAMESPACE_PRETTY_ENUMERATION(
  genotype::env_controller, Outputs,
    ALTITUDE_, TEMPERATURE_, HYGROMETRY_  // [-1:1]
)

namespace genotype::env_controller {
using Inputs_array = std::array<float, EnumUtils<Inputs>::size()>;
using Outputs_array = std::array<float, EnumUtils<Outputs>::size()>;

struct Controller;
} // end of namespace genotype::env_controller

namespace genotype {

class EnvCTRL : public EDNA<EnvCTRL> {
  APT_EDNA()

  env_controller::Inputs_array _inputs;
  env_controller::Outputs_array _outputs;
  env_controller::Controller *_controller;

  float c0; // Heat decreases with altitude
  float c1; // Water decreases with temperature

public:
  std::string _expression;

  using I = env_controller::Inputs;
  using O = env_controller::Outputs;

  EnvCTRL (void) : _controller(nullptr) {}
  ~EnvCTRL(void);

  void init (void);

  float evaluate (void);

  auto& inputs (void) {
    return _inputs;
  }

  const auto& outputs (void) const {
    return _outputs;
  }

  static void save (nlohmann::json &j, const EnvCTRL &c);
  static void load (const nlohmann::json &j, EnvCTRL &c);
};

DECLARE_GENOME_FIELD(EnvCTRL, std::string, _expression)
DECLARE_GENOME_FIELD(EnvCTRL, float, c0)
DECLARE_GENOME_FIELD(EnvCTRL, float, c1)

} // end of namespace genotype

namespace config {

template <>
struct EDNA_CONFIG_FILE(EnvCTRL) {
  using Bf = Bounds<float>;
  DECLARE_PARAMETER(Bf, c0Bounds)
  DECLARE_PARAMETER(Bf, c1Bounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

} // end of namespace config

#endif // ENVCGP_HPP
