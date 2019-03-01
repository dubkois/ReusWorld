#include "kgd/apt/visu/standaloneviewer.hpp"

#include "genotype/plant.h"
#include "simu/phylogenystats.hpp"

int main(int argc, char *argv[]) {
  return run<genotype::Plant, simu::PStats>(argc, argv);
}
