#include "kgd/utils/functions.h"

#include "envcgp.h"

#define exprtk_disable_caseinsensitivity
#include "../cgp/exprtk.hpp"

namespace genotype::env_controller {
class Controller {
  using decimal = float;
  using symbol_table_t = exprtk::symbol_table<decimal>;
  using expression_t = exprtk::expression<decimal>;
  using parser_t = exprtk::parser<decimal>;
  using error_t = exprtk::parser_error::type;

  symbol_table_t _symbol_table;
  expression_t _expression;

public:
  Controller (const std::string &expression, Inputs_array &i, Outputs_array &o);

  float evaluate (void);
};

//template <typename T>
//struct gauss : public exprtk::ifunction<T> {
//  gauss(void) : exprtk::ifunction<T>(3) {
//    exprtk::disable_has_side_effects(*this);
//  }

//  T operator()(const T& x, const T& mu, const T& sigma) {
//    return utils::gauss(x, mu, sigma);
//  }
//};

Controller::Controller (const std::string &expression,
                        Inputs_array &i, Outputs_array &o) {

  _symbol_table.add_function("gauss", utils::gauss);

  _symbol_table.add_constants();
  _symbol_table.add_variable("D", i[DAY]);
  _symbol_table.add_variable("Y", i[YEAR]);
  _symbol_table.add_variable("x", i[COORDINATE]);
  _symbol_table.add_variable("y", i[ALTITUDE]);
  _symbol_table.add_variable("t", i[TEMPERATURE]);
  _symbol_table.add_variable("w", i[HYGROMETRY]);

  _symbol_table.add_variable("y_", o[ALTITUDE_]);
  _symbol_table.add_variable("t_", o[TEMPERATURE_]);
  _symbol_table.add_variable("w_", o[HYGROMETRY_]);

  _expression.register_symbol_table(_symbol_table);

  parser_t parser;

  if (!parser.compile(expression, _expression)) {
    std::ostringstream oss;
    oss << "Error: "
        << parser.error()
        << "\tExpression: "
        << expression
        << "\n";

    for (std::size_t i = 0; i < parser.error_count(); ++i) {
      const error_t error = parser.get_error(i);
      oss << "Error: " << static_cast<int>(i) << " Position: "
          << static_cast<int>(error.token.position)
          << " Type: [" << exprtk::parser_error::to_str(error.mode)
          << "] Msg: " << error.diagnostic << " Expr: "
          << expression << "\n";
    }

    throw std::invalid_argument(oss.str());
  }
}

float Controller::evaluate(void) {
  return _expression.value();
}

} // end of namespace genotype::env_controller

#define GENOME EnvCTRL

static constexpr auto defaultExpr = "0";

auto exprFunctor (void) {
  GENOME_FIELD_FUNCTOR(std::string, _expression) f;
  f.random = [] (auto&) { return defaultExpr;  };
  f.mutate = [] (auto&, auto&) { assert(false); };
  f.cross = [] (auto&, auto&, auto&) { assert(false); return defaultExpr; };
  f.distance = [] (auto&, auto&) { assert(false); return std::nan(""); };
  f.check = [] (auto&) { return true; };
  return f;
}

DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  c0, "", 0.f, 1.f, 1.f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float,  c1, "", 0.f, 1.f, 1.f, 1.f)
DEFINE_GENOME_FIELD_WITH_FUNCTOR(std::string,  _expression, "expr", exprFunctor())

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(         c0, .1f),
  MUTATION_RATE(         c1, .1f),
  MUTATION_RATE(_expression, 0.f),
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  DISTANCE_WEIGHT(_expression, .1f),
  DISTANCE_WEIGHT(         c0, .1f),
  DISTANCE_WEIGHT(         c1, .1f),
})

#undef GENOME

namespace genotype {

template <>
struct Extractor<std::string> {
  std::string operator() (const std::string &value, const std::string &) const {
    return value;
  }
};

template <>
struct Aggregator<std::string, EnvCTRL> {
  void operator() (std::ostream &/*os*/, const std::vector<EnvCTRL> &/*objects*/,
                   std::function<const std::string& (const EnvCTRL&)> /*access*/,
                   uint /*verbosity*/) {
    throw std::logic_error("Not implemented");
  }
};

float pos (float x) {
  return x < 0 ? 0 : x;
}

void EnvCTRL::init (void) {
  _inputs.fill(0);
  _outputs.fill(0);
  _controller = new env_controller::Controller(_expression, _inputs, _outputs);
}

EnvCTRL::~EnvCTRL (void) {
  delete _controller;
}

float EnvCTRL::evaluate (void) {
  auto res = _controller->evaluate();

  using namespace env_controller;

  _outputs[TEMPERATURE_] = -c0 * pos(_inputs[ALTITUDE])
                         + (1-c0) * _outputs[TEMPERATURE_];

  _outputs[HYGROMETRY_] = -c1 * pos(_inputs[TEMPERATURE])
                        + (1 - c1) * _outputs[HYGROMETRY_];

  return res;
//  using utils::gauss;
//  using namespace cgp;

//  float x = inputs[COORDINATE];

//#warning Bypassing CGP!

////  outputs[ALTITUDE_] = .5*utils::sgn(std::sin(4*(x+.25) *M_PI));
//  outputs[ALTITUDE_] = (t_a0 * gauss(x, t_m0, t_s0) - t_a1 * gauss(x, t_m1, t_s1))
//                     * inputs[YEAR];

////  outputs[TEMPERATURE_] = .5*utils::sgn(std::sin(4*(x+.25) *M_PI));
////  outputs[TEMPERATURE_] = 2 * inputs[COORDINATE] - 1;
////  outputs[TEMPERATURE_] = x < .4 ? -1 : x > .6 ? 1 : 0;
//  outputs[TEMPERATURE_] = -c0 * pos(inputs[ALTITUDE])
//                        + (1-c0) * h_a * sin(2 * h_f * inputs[DAY] * M_PI);

////  outputs[HYGROMETRY_] = 1;
////  outputs[HYGROMETRY_] = .5*utils::sgn(std::sin(4*(x+.25) *M_PI));
////  outputs[HYGROMETRY_] = 1 - 2 * inputs[COORDINATE];
//  outputs[HYGROMETRY_] = c1 * (1 - pos(inputs[TEMPERATURE] - .5))
//                       + (1 - c1) * (1 - gauss(x, w_m, w_s));
}

void EnvCTRL::save (nlohmann::json &j, const EnvCTRL &c) {
  j = c._expression;
}

void EnvCTRL::load (const nlohmann::json &j, EnvCTRL &c) {
  c._expression = j;
  c.init();
}

} // end of namespace genotype

