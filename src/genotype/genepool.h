#ifndef GENEPOOL_H
#define GENEPOOL_H

#include <ostream>
#include <map>

namespace simu { struct Simulation; }

namespace misc {

struct GenePool {
  void parse (const simu::Simulation &s);

  friend double divergence (const GenePool &lhs, const GenePool &rhs);

  friend std::ostream& operator<< (std::ostream &os, const GenePool &gp);

private:
  std::map<std::string, std::map<std::string, float>> histograms;
};

} // end of namespace misc

#endif // GENEPOOL_H
