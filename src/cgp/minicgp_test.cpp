#include "kgd/external/cxxopts.hpp"

#include "minicgp.h"

DEFINE_PRETTY_ENUMERATION(TestCGPInputs, A, B, C, D)
DEFINE_PRETTY_ENUMERATION(TestCGPOutputs, dA, dB, dC)

using CGP = cgp::CGP<TestCGPInputs, 100, TestCGPOutputs>;

void showCGP (std::string label, const CGP &cgp) {
  label = "cgp_test_" + label;

  std::cout << cgp << std::endl;
  cgp.toDot(label + ".pdf");
  cgp.toDot(label + "_full.pdf", CGP::FULL);
  cgp.toDot(label + "_noarity.pdf", CGP::NO_ARITY);
  cgp.toDot(label + "_full_noarity.pdf", CGP::FULL | CGP::NO_ARITY);
  cgp.toDot(label + "_data.pdf");
  cgp.toDot(label + "_full_no_arity_data.pdf", CGP::FULL | CGP::NO_ARITY | CGP::SHOW_DATA);

  auto texfile = label + "_tex.tex";
  std::ofstream ofs (texfile);
  ofs << "\\documentclass[preview]{standalone}\n"
         "\\usepackage{amsmath}\n"
         "\\begin{document}\n"
      << cgp.toTex()
      << "\\end{document}\n";
  ofs.close();
  {
    std::ostringstream oss;
    oss << "pdflatex " << texfile << " > /dev/null";
    auto cmd = oss.str();
    if (0 == system(cmd.c_str()))
      std::cout << "Generated file." << std::endl;
  }

}

int main (int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  cxxopts::Options options("MiniCGP-tester",
                           "Tests that MiniCGP implementation is correct");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  config::CGP::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::CGP::printConfig("");

  rng::FastDice dice;
  CGP cgp = CGP::null(dice);

  std::cout << "Generated with seed " << dice.getSeed() << std::endl;

  showCGP("null", cgp);

  uint N = 100;
  CGP mutated = cgp.clone();
  assertEqual(cgp, mutated);

  uint n = 0;
  for (uint i=0; i<N; i++) {
    std::cerr << "Mutations " << std::setw(2) << i << ": ";
    n += mutated.mutate(dice);
    std::cerr << std::endl;
  }
  std::cerr  << n << " total mutations (" << n / float(N) << " avg)" << std::endl;

  {
    std::ostringstream oss;
    oss << "mutated" << N;
    showCGP(oss.str(), mutated);
  }

  return 0;
}
