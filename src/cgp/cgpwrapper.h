#ifndef _CGP_WRAPPER_H_
#define _CGP_WRAPPER_H_

#include <cassert>

#include <memory>

#include "kgd/external/json.hpp"
#include "kgd/utils/utils.h"

#include "cgp.h"

namespace cgp {

namespace custom_functions {

/// 0 Arity
double zero (const int, const double*, const double*);
double one (const int, const double*, const double*);

/// 1 Arity
double abs (const int, const double *inputs, const double*);
double ceil (const int, const double *inputs, const double*);
double floor (const int, const double *inputs, const double*);
double neg (const int, const double *inputs, const double*);
double cos (const int, const double *inputs, const double*);
double sin (const int, const double *inputs, const double*);
double gauss1 (const int, const double *inputs, const double*);

/// 2 Arity
double min (const int, const double *inputs, const double*);
double max (const int, const double *inputs, const double*);
double sum (const int, const double *inputs, const double*);
double sub (const int, const double *inputs, const double*);
double mult (const int, const double *inputs, const double*);
double pow (const int, const double *inputs, const double*);
double gauss2 (const int, const double *inputs, const double*);
} // end of namespace custom_functions

class CGPWrapperBase {
protected:
  using Param_ptr = std::unique_ptr<struct parameters, decltype(&freeParameters)>;

  chromosome *_cgp;

  CGPWrapperBase (chromosome *c) : _cgp(c) {}
  CGPWrapperBase(void) : _cgp(nullptr) {}

public:
  CGPWrapperBase(const CGPWrapperBase &that) {
    assert(that._cgp);
    _cgp = initialiseChromosomeFromChromosome(that._cgp);
  }

  CGPWrapperBase(CGPWrapperBase &&that) : CGPWrapperBase() {
    swap(*this, that);
  }

  CGPWrapperBase& operator= (CGPWrapperBase that) {
    swap(*this, that);
    return *this;
  }

  ~CGPWrapperBase (void) {
    if (_cgp)   freeChromosome(_cgp);
  }

  friend void swap (CGPWrapperBase &first, CGPWrapperBase &second) {
    using std::swap;
    swap(first._cgp, second._cgp);
  }

  void toFile (const std::string &filename) const;
  void toDot (const std::string &filename) const;
  void toLatex (const std::string &filename) const;

#if defined(__linux__)
  static int dotToSvg (const std::string &dotFile, bool autoview = false);
  static int texToPdf(const std::string &texFile, bool autoview = false);
#endif

  nlohmann::json serialize(void) const;

protected:
  static CGPWrapperBase deserialize(const nlohmann::json& j, const Param_ptr& params);

  static parameters* initParameters(int inputs, int nodes, int output, int arity);
  static void setRandomNumberSeed(uint seed);

  virtual std::string inputName (uint i) const {
    return std::string("Input ") + std::to_string(i);
  }

  virtual std::string outputName (uint i) const {
    return std::string("Output ") + std::to_string(i);
  }

private:
  static int funcNameToIndex (const std::string& name, const Param_ptr &params);

  void toLatex (std::ostream &ofs, int index, std::set<int> &usedInputs) const;
};

template <size_t I, size_t O>
class CGPWrapper : public CGPWrapperBase {

  static const std::array<std::string, I> inputNames;
  static const std::array<std::string, O> outputNames;

  static const Param_ptr& CGP_params (void) {
    static Param_ptr params (CGPWrapperBase::initParameters(I, NODES, O, ARITY), &freeParameters);
    return params;
  }

  CGPWrapper (const CGPWrapperBase &base) : CGPWrapperBase(base) {}
  CGPWrapper (chromosome *c) : CGPWrapperBase(c) {}

  std::string inputName (uint i) const override {
    return inputNames.at(i);
  }

  std::string outputName (uint i) const override {
    return outputNames.at(i);
  }

public:
  static constexpr uint INPUTS = I;
  static constexpr uint NODES = (I+O) * 5;
  static constexpr uint ARITY = 2;
  static constexpr uint OUTPUTS = O;

  using Inputs  = std::array<double, INPUTS>;
  using Outputs = std::array<double, OUTPUTS>;

  CGPWrapper (void) : CGPWrapperBase() {}
  CGPWrapper (const std::string &s) : CGPWrapper(deserialize(s)) {}

  void reset (void) {}

  CGPWrapper crossover (const CGPWrapper &) { return *this; }

  void mutate (void) {
    mutateChromosome(CGP_params().get(), _cgp);
  }

  static CGPWrapper random (void) {
    return CGPWrapper (initialiseChromosome(CGP_params().get()));
  }

  void execute(const Inputs &inputs, Outputs &outputs) {
    executeChromosome(_cgp, inputs.data());

    for (uint i=0; i<outputs.size(); i++)
      outputs[i] = getChromosomeOutput(_cgp, i);
  }

  static CGPWrapper deserialize(const nlohmann::json &j) {
    return CGPWrapperBase::deserialize(j, CGP_params());
  }

  static CGPWrapper fromFile (const std::string &filename) {
    return deserialize(nlohmann::json::parse(utils::readAll(filename)));
  }

  static void demoCGP (const Inputs &inputs, Outputs &outputs);
};

} // end of namespace cgp

#endif // _CGP_WRAPPER_H_
