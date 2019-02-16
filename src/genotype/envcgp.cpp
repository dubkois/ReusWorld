#include "kgd/utils/functions.h"

#include "envcgp.h"

#ifdef FULL_CGP

template <>
const std::array<std::string, genotype::EnvCGP::INPUTS>
genotype::EnvCGP::inputNames {
  "X", "A", "T", "W"
};

template <>
const std::array<std::string, genotype::EnvCGP::OUTPUTS>
genotype::EnvCGP::outputNames {
  "A'", "T'", "W'"
};

template <>
void genotype::EnvCGP::demoCGP (const Inputs &/*inputs*/, Outputs &/*outputs*/) {
//  using namespace custom_functions;
//  using A = double[1];
//  using B = double[2];

//  outputs[Fields::CATACLYSM] = inputs[4] > .99 ? 1 : -1;
//  outputs[Fields::RAIN] = (inputs[2]*inputs[3] > 0) ? inputs[4] * inputs[1] : 0;

//  if (false) { // Flat
//    outputs[Fields::TOPOLOGY] = 0;

//  } else if (false) {// Hyperbolic paraboloid
//    outputs[Fields::TOPOLOGY] = utils::sgn(inputs[2]*inputs[3]) * std::min(1., std::max(.05, 1.5 * fabs(inputs[5+Fields::TOPOLOGY])));

//  } else if (false) { // Volcano
//    B sqX {inputs[2], inputs[2] };
//    B sqZ {inputs[3], inputs[3] };

//    B sumXZ { mult(2, sqX, 0), mult(2, sqZ, 0) };

//    B gaussO { sum(2, sumXZ, 0), .25 };
//    B gaussI { sum(2, sumXZ, 0), .125 };

//    B subG { gauss2(2, gaussO, 0), gauss2(2, gaussI, 0) };
//    B sumGT { inputs[5+Fields::TOPOLOGY], sub(2, subG, 0) };

//    outputs[Fields::TOPOLOGY] = sum(2, sumGT, 0);

//  } else if (true) {
//    outputs[Fields::TOPOLOGY] = .1 * (
//          2. * std::floor(
//            inputs[2] - .75 - std::sin(
//            4.*M_PI*(inputs[3] + .125)
//          )
//        ) - std::floor(
//          2. * (
//            inputs[2] - .25 - std::sin(
//            2 * M_PI * (inputs[3] * .25)
//          )
//        )
//        )
//        );

//  } else if (false) {
//    static constexpr double a = .1;
//    static constexpr double l = 4;
//    static constexpr double w = 4*2*M_PI;
//    static constexpr double p = 0;
//    const auto t = [inputs] {
//      double x = inputs[2];
//      double y = inputs[3];
//      return sqrt((x)*(x)+(y)*(y));
//      //        return sqrt((x-.5)*(x-.5)+(y-.5)*(y-.5));
//    };
//    outputs[Fields::TOPOLOGY] = inputs[5+Fields::TOPOLOGY] + a * exp(-l*t()) * std::cos(w*t()+p);
//  }
}
#else

#define GENOME EnvCGP
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, t_a0, "", 0.f, .25f, .25f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, t_m0, "", 0.f, .75f, .75f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, t_s0, "", .01f, .05f, .05f, .2f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, t_a1, "", 0.f, .125f, .125f, .5f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, t_m1, "", 0.f, .25f, .25f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, t_s1, "", .01f, .05f, .05f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  h_a, "", 0.f, .5f, .5f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  h_f, "", 0.f, 1.f, 1.f, 10.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  w_m, "", 0.f, .25f, .25f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  w_s, "", 0.f, .1f, .1f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  c0, "", 0.f, 1.f, 1.f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  c1, "", 0.f, 1.f, 1.f, 1.f)

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(t_a0, .1f),
  MUTATION_RATE(t_m0, .1f),
  MUTATION_RATE(t_s0, .1f),
  MUTATION_RATE(t_a1, .1f),
  MUTATION_RATE(t_m1, .1f),
  MUTATION_RATE(t_s1, .1f),
  MUTATION_RATE( h_a, .1f),
  MUTATION_RATE( h_f, .1f),
  MUTATION_RATE( w_m, .1f),
  MUTATION_RATE( w_s, .1f),
  MUTATION_RATE(  c0, .1f),
  MUTATION_RATE(  c1, .1f),
})

#undef GENOME

namespace genotype {

float pos (float x) {
  return x < 0 ? 0 : x;
}

void EnvCGP::process(const Inputs &inputs, Outputs &outputs) {
  using utils::gauss;
  using namespace cgp;

  float x = inputs[COORDINATE];

#warning Bypassing CGP!

//  outputs[ALTITUDE_] = .5*utils::sgn(std::sin(4*(x+.25) *M_PI));
  outputs[ALTITUDE_] = (t_a0 * gauss(x, t_m0, t_s0) - t_a1 * gauss(x, t_m1, t_s1))
                     * inputs[YEAR];

//  outputs[TEMPERATURE_] = .5*utils::sgn(std::sin(4*(x+.25) *M_PI));
//  outputs[TEMPERATURE_] = 2 * inputs[COORDINATE] - 1;
//  outputs[TEMPERATURE_] = x < .4 ? -1 : x > .6 ? 1 : 0;
  outputs[TEMPERATURE_] = -c0 * pos(inputs[ALTITUDE])
                        + (1-c0) * h_a * sin(2 * h_f * inputs[DAY] * M_PI);

//  outputs[HYGROMETRY_] = 1;
//  outputs[HYGROMETRY_] = .5*utils::sgn(std::sin(4*(x+.25) *M_PI));
//  outputs[HYGROMETRY_] = 1 - 2 * inputs[COORDINATE];
  outputs[HYGROMETRY_] = c1 * (1 - pos(inputs[TEMPERATURE] - .5))
                       + (1 - c1) * (1 - gauss(x, w_m, w_s));
}

} // end of namespace genotype

#endif
