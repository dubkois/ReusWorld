#include <csignal>

#include "../simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

struct ITest {
  virtual const std::string& name (void) const = 0;

  virtual void operator() (void) const = 0;
  virtual void next (bool success) const = 0;
  virtual bool preciseEnough (void) const = 0;
  virtual void finalize (void) const = 0;

  virtual std::string finalState (void) const = 0;
  virtual void doPrint (std::ostream &os) const = 0;
  friend std::ostream& operator<< (std::ostream &os, const ITest &t) {
    t.doPrint(os);
    return os;
  }
};

template <template <typename> class ConfigValue, typename T>
struct Test : public ITest {
  ConfigValue<T> &cvalue;

  T min, max;
  float margin;
  bool decreasing;

  mutable T currMin, currVal, currMax;

  mutable bool converged;
  mutable T finalValue;

  Test (ConfigValue<T> &cvalue, T min, T value, T max, float margin, bool decreasing)
    : cvalue(cvalue), min(min), max(max), margin(margin), decreasing(decreasing),
      currMin(min), currVal(value), currMax(max),
      converged(false), finalValue(currVal) {}

  const std::string& name (void) const override {
    return cvalue.name();
  }

  void operator() (void) const override {
    cvalue.overrideWith(currVal);
  }

  void next (bool success) const override {
    if (decreasing) {
      if (success)  currMax = currVal;  // Search for stricter (lower) value
      else          currMin = currVal;  // Went too low, ease up
    } else {
      if (success)  currMin = currVal;  // Search for stricter (higher) value
      else          currMax = currVal;  // Went too high, ease down
    }
    currVal = .5f * (currMin + currMax);
  }

  bool preciseEnough (void) const override {
    return (currMax - currMin) < (max - min) / (1<<10) ;
  }

  void finalize (void) const override {
    finalValue = currVal;
    float r = 1.f + margin;
    if (decreasing)
          finalValue *= r;
    else  finalValue /= r;
    cvalue.overrideWith(finalValue);
    converged = true;
  }

  void doPrint (std::ostream &os) const override {
    os << name() << " (" << min;
    if (min < currMin)  os << "..." << currMin;
    if (currMin < currVal)  os << "...>" << currVal << "<";
    if (currVal < currMax)  os << "..." << currMax;
    if (currMax < max)  os << "..." << max;
    os << ")";
  }

  std::string finalState (void) const override {
    std::ostringstream oss;
    oss << name() << ": " << finalValue << " ("
        << (decreasing ? "min" : "max")
        << "(" << currVal << " "
        << (decreasing ? "*" : "/") << " " << (1.f + margin);
    oss << "))";
    return oss.str();
  }
};

template <template <typename> class CV, typename T>
static auto lowerBoundTest (CV<T> &configValue, float min, float margin) {
  return std::make_unique<Test<CV, T>> (configValue, min, configValue(), configValue(), margin, true);
}

template <template <typename> class CV, typename T>
static auto upperBoundTest (CV<T> &configValue, float max, float margin) {
  return std::make_unique<Test<CV, T>> (configValue, configValue(), configValue(), max, margin, false);
}

using SConfig = config::Simulation;

int main(void) {
  static constexpr uint SEEDS = 20;
  static const std::unique_ptr<ITest> tests[] {
    upperBoundTest(SConfig::lifeCost, 1, .2),
    lowerBoundTest(SConfig::assimilationRate, 0, .2),
    upperBoundTest(SConfig::saturationRate, 10, .2),
    lowerBoundTest(SConfig::baselineShallowWater, 0, .2),
    lowerBoundTest(SConfig::baselineLight, 0, .2),
    upperBoundTest(SConfig::floweringCost, 9, 0),
  };
  static const auto genomes = [] {
    std::vector<genotype::Ecosystem> genomes;
    for (uint i=0; i<SEEDS; i++) {
      rng::FastDice dice (i);
      genomes.push_back(genotype::Ecosystem::random(dice));
    }
    return genomes;
  }();

  SConfig::stopAtYear.overrideWith(5);
  SConfig::stopAtMinGen.overrideWith(10);
  SConfig::stopAtMaxGen.overrideWith(20);
  SConfig::verbosity.overrideWith(0);
  SConfig::setupConfig("auto", config::Verbosity::SHOW);

  std::cout << "\nGenomes are: " << std::endl;
  {
    uint i=0;
    for (const auto &g: genomes)
      std::cout << i++ << "/" << SEEDS << ": " << g << std::endl;
  }

  const auto &initSeeds = SConfig::initSeeds();
  for (const auto &test_ptr: tests) {
    const ITest &test = *test_ptr;
    bool success = false;

    std::cout << std::endl;
    while (!(test.preciseEnough() && success)) {
      test();

      std::cout << "## Testing " << test << ": " << std::flush;
      uint successCount = 0, totalPlants = 0, totalSteps = 0;
      for (uint i=0; i<SEEDS; i++) { // Various rng seeds
        simu::Simulation s (genomes[i]);
        s.init();

        while (!s.finished())
          s.step();

        bool thisSuccess = (s.plants().size() >= initSeeds);
        successCount += thisSuccess;

        totalPlants += s.plants().size();
        totalSteps += s.currentStep();

        std::cout << (thisSuccess ? '+' : '-') << std::flush;

        s.destroy();
      }

      success = (SEEDS == successCount);
      test.next(success);

      std::cout << " ";
      if (success) {
        std::cout << "success (" << float(totalSteps) / SEEDS
                  << " avg steps and " << float(totalPlants) / SEEDS
                  << " plants)";
        if (test.preciseEnough())  std::cout << ". Precise enough, stopping here";
      } else
        std::cout << "failure.";
      std::cout << std::endl;
    }

    test.finalize();
    std::cout << "# Final value for " << test.finalState() << std::endl;
  }

  stdfs::path configPath ("configs/auto-calibration/");
  std::cout << "\nWriting calibrated config file(s) to " << configPath << std::endl;
  config::Simulation::printConfig(configPath);

  std::cout << "\nAuto-calibration summary:\n";
  for (const auto &test_ptr: tests)
    std::cout << "\t- " << test_ptr->finalState() << std::endl;

  return 0;
}
