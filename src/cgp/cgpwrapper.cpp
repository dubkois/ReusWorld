#include <fstream>
#include <set>

#include "cgpwrapper.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "cgp.c"
#pragma GCC diagnostic warning "-Wunused-parameter"

namespace cgp {

namespace custom_functions {

double gauss (double x, double stddev) {
  return exp(- (x*x)/(2*stddev*stddev));
}

/// 0 Arity

double zero (const int /*numInputs*/, const double */*inputs*/, const double */*weights*/) {
  return 0;
}

double one (const int /*numInputs*/, const double */*inputs*/, const double */*weights*/) {
  return 1;
}

/// 1 Arity

double abs (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return fabs(inputs[0]);
}

double ceil (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return std::ceil(inputs[0]);
}

double floor (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return std::floor(inputs[0]);
}

double neg (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return -inputs[0];
}

double cos (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return std::cos(M_PI*inputs[0]);
}

double sin (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return std::sin(M_PI*inputs[0]);
}

double gauss1 (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return gauss(inputs[0], 1);
}

/// 2 Arity

double min (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return std::min(inputs[0], inputs[1]);
}

double max (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return std::max(inputs[0], inputs[1]);
}

double sum (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return (inputs[0] + inputs[1]) / 2.;
}

double sub (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return fabs(inputs[0] - inputs[1]) / 2.;
}

double mult (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return inputs[0] * inputs[1];
}

double pow (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return fabs(inputs[0]) * inputs[1];
}

double gauss2 (const int /*numInputs*/, const double *inputs, const double */*weights*/) {
  return gauss(inputs[0], inputs[1]);
}


} // end of namespace custom functions

parameters* CGPWrapperBase::initParameters(int inputs, int nodes, int outputs, int arity) {
  namespace cf = custom_functions;

  parameters *p = initialiseParameters(inputs, nodes, outputs, arity);
  addCustomNodeFunction(p, cf::zero,   "zero",   0);
  addCustomNodeFunction(p, cf::one,    "one",    0);
  addCustomNodeFunction(p, cf::abs,    "abs",    1);
  addCustomNodeFunction(p, cf::ceil,   "ceil",   1);
  addCustomNodeFunction(p, cf::floor,  "floor",  1);
  addCustomNodeFunction(p, cf::neg,    "neg",    1);
  addCustomNodeFunction(p, cf::cos,    "cos",    1);
  addCustomNodeFunction(p, cf::sin,    "sin",    1);
  addCustomNodeFunction(p, cf::gauss1, "gauss1", 1);
  addCustomNodeFunction(p, cf::min,    "min",    2);
  addCustomNodeFunction(p, cf::max,    "max",    2);
  addCustomNodeFunction(p, cf::sum,    "sum",    2);
  addCustomNodeFunction(p, cf::sub,    "sub",    2);
  addCustomNodeFunction(p, cf::mult,   "mult",   2);
  addCustomNodeFunction(p, cf::pow,    "pow",    2);
  addCustomNodeFunction(p, cf::gauss2, "gauss2", 2);

//  setConnectionWeightRange(p, 1);

  printParameters(p);
  return p;
}

void CGPWrapperBase::setRandomNumberSeed(uint seed) {
  setRandomNumberSeed(seed);
}

nlohmann::json CGPWrapperBase::serialize (void) const {
  nlohmann::json j;
  j["I"] = _cgp->numInputs;
  j["N"] = _cgp->numNodes;
  j["O"] = _cgp->numOutputs;
  j["A"] = _cgp->arity;

  auto functionNames = _cgp->funcSet->functionNames;
  for (int i=0; i<_cgp->funcSet->numFunctions; i++)
    j["F"].push_back(functionNames[i]);

  for (int i=0; i<_cgp->numNodes; i++) {
    node *n = _cgp->nodes[i];
    nlohmann::json jn;

    jn["f"] = functionNames[n->function];
    for (int j=0; j<_cgp->arity; j++)
      //            jn["i"].push_back({n->inputs[j],n->weights[j]});
      jn["i"].push_back(n->inputs[j]);

    j["n"].push_back(jn);
  }

  for (int i=0; i<_cgp->numOutputs; i++)
    j["o"].push_back(_cgp->outputNodes[i]);

  return j;
}

void CGPWrapperBase::toFile (const std::string &filename) const {
  std::ofstream ofs (filename);
  if (!ofs)
    throw std::invalid_argument(std::string("Unable to write to file ") + filename);

  ofs << serialize().dump(2);
  ofs.close();
}

#if defined(__linux__)
int CGPWrapperBase::dotToSvg (const std::string &dotFile, bool autoview) {
  std::string svgFile = dotFile.substr(0, dotFile.find_last_of('.')) + ".svg";

  int ret = system(std::string("dot -Tsvg " + dotFile + " -o " + svgFile).c_str());
  if (autoview && ret == 0)
    ret = system(std::string("xdg-open " + svgFile).c_str());

  return ret;
}

int CGPWrapperBase::texToPdf (const std::string &texFile, bool autoview) {
  std::string basename = texFile.substr(0, texFile.find_last_of('.'));

  int ret = system(std::string(
                     "pdflatex -output-directory " + basename.substr(0, basename.find_last_of('/'))
                     + " " + texFile).c_str());
  if (autoview && ret == 0)
    ret = system(std::string("xdg-open " + basename + ".pdf").c_str());

  return ret;
}
#endif



int CGPWrapperBase::funcNameToIndex (const std::string &name, const Param_ptr &params) {
  int numFunctions = params->funcSet->numFunctions;
  auto functionNames = params->funcSet->functionNames;

  for (int i=0; i<numFunctions; i++)
    if (strcmp(functionNames[i], name.c_str()) == 0)
      return i;
  return -1;
}

CGPWrapperBase CGPWrapperBase::deserialize(const nlohmann::json &j, const Param_ptr &params) {
  int numInputs = j["I"];
  int numNodes = j["N"];
  int numOutputs = j["O"];
  int arity = j["A"];

  if (params->numInputs != numInputs)
    throw std::invalid_argument("Provided CGP has mismatching inputs count");
  if (params->numNodes != numNodes)
    throw std::invalid_argument("Provided CGP has mismatching nodes count");
  if (params->numOutputs != numOutputs)
    throw std::invalid_argument("Provided CGP has mismatching outputs count");
  if (params->arity != arity)
    throw std::invalid_argument("Provided CGP has mismatching arity");

  chromosome *c = initialiseChromosome(params.get());
  for (int i=0; i<numNodes; i++) {
    node *n = c->nodes[i];
    nlohmann::json jn = j["n"][i];

    n->function = funcNameToIndex(jn["f"], params);
    assert(n->function >= 0);
    for (int j=0; j<arity; j++)
      n->inputs[j] = jn["i"][j];

    //        for (int j=0; j<arity; j++) {
    //            n->inputs[j] = jn["i"][j][0];
    //            n->weights[j] = jn["i"][j][1];
    //        }
  }

  for (int i=0; i<numOutputs; i++)
    c->outputNodes[i] = j["o"][i];

  setChromosomeActiveNodes(c);

  return CGPWrapperBase(c);
}

struct ColorFlag {
  const node *n;
  ColorFlag (node *n) : n(n) {}
  friend std::ostream& operator<< (std::ostream &os, const ColorFlag &f) {
    static const std::string c = "lightgrey";
    if (f.n->active == 1)
      return os;
    else    return os << "labelfontcolor=" << c
                      << ", fontcolor=" << c
                      << ", color=" << c;
  }
};

/// Extends original library implementations by using custom names for inputs/outputs
void CGPWrapperBase::toDot (const std::string &filename) const {
  std::ofstream ofs (filename);
  if (!ofs)
    throw std::invalid_argument(std::string("Unable to save to ") + filename);

  /* */
  ofs << "digraph CGP {\n";

  /* landscape, and center */
  ofs << "rankdir=LR;\n";
  ofs << "center = true;\n";

  /* for all the inputs */
  int inputs = getNumChromosomeInputs(_cgp);
  for (int i = 0; i < inputs; i++)
    ofs << i << " [label=\"" << inputName(i) << "\"];\n";

  /* for all nodes */
  int inodes = getNumChromosomeNodes(_cgp);
  auto functionNames = _cgp->funcSet->functionNames;
  for (int i = 0; i < inodes; i++) {
    int index = i + inputs;
    node *n = _cgp->nodes[i];
    ColorFlag color (n);

    ofs << index << " [label=\"(" << index << ") " << functionNames[n->function]
        << "\", " << color << "];\n";

    /* for each node input */
    int arity = getChromosomeNodeArity(_cgp, i);
    for (int j = 0; j < arity; j++)
      ofs << n->inputs[j] << " -> " << index
          << " [label=\"(" << j << ")\", bold=true, " << color << "];\n";

    //        for (int j = 0; j < arity; j++)
    //            ofs << n->inputs[j] << " -> " << index
    //                << " [label=<(" << j << ") "
    //                << "<FONT POINT-SIZE=\"10\">" << n->weights[j] << "</FONT>"
    //                << ">, bold=true, " << color << "];\n";
  }

  int outputs = getNumChromosomeOutputs(_cgp);
  for (int i = 0; i < outputs; i++) {
    int index = i + inputs + inodes;
    ofs << index << " [label=\"" << outputName(i) << "\"];\n",

        ofs << _cgp->outputNodes[i] << " -> " << index << " [bold=true];\n";
  }


  /* place inputs  on same line */
  ofs << "{ rank = source;";
  for (int i = 0; i < inputs; i++)
    ofs << " \"" << i << "\";";
  ofs << " }\n";


  /* place outputs  on same line */
  ofs << "{ rank = max;";
  for (int i = 0; i < outputs; i++)
    ofs << "\"" << i + inputs + inodes << "\";";
  ofs << " }\n";

  /* last line of dot file */
  ofs << "}";

  ofs.close();
}

void CGPWrapperBase::toLatex (const std::string &filename) const {
  std::ofstream ofs (filename);
  if (!ofs)
    throw std::invalid_argument(std::string("Unable to save to ") + filename);

  /* document header */
  ofs << "\\documentclass{standalone}\n";
  ofs << "\\begin{document}\n";
  ofs << "\\begin{tabular}{r@{ = }l}\n";

  for (int output = 0; output < _cgp->numOutputs; output++) {
    std::set<int> usedInputs;

    std::ostringstream oss;
    toLatex(oss, _cgp->outputNodes[output], usedInputs);

//    std::cout << outputName(output) << " uses inputs ";
//    for (int i: usedInputs) std::cout << i << " ";
//    std::cout << std::endl;

    ofs << "$" << outputName(output) << "(";

    /* Actually used inputs */
    if (usedInputs.empty()) ofs << ")";
    else {
      auto it = usedInputs.begin();
      ofs << inputName(*it);

      for (++it; it != usedInputs.end(); ++it)
        ofs << "," << inputName(*it);

      ofs << ")$&$";
    }

    ofs << oss.str()
        << "$\\\\\n";
  }


  /* document footer */
  ofs << "\n\\end{tabular}\n\\end{document}" << std::endl;
  ofs.close();
}

void CGPWrapperBase::toLatex (std::ostream &ofs, int index, std::set<int> &usedInputs) const {
  struct LatexFormat {
    std::string prefix, infix, suffix;
  };

  static const std::map<std::string, LatexFormat> formats {
    {  "add", { "\\left(",       "+",   "\\right)" }},
    {  "sub", { "\\left(",       "-",   "\\right)" }},
    { "mult", { "\\left(", "\\times",   "\\right)" }},
    {  "abs", { "\\left|",        "",   "\\right|" }},
    { "sqrt", { "\\sqrt{",        "",          "}" }},
    {   "sq", { "\\left(",        "", "\\right)^2" }},
    { "cube", { "\\left(",        "", "\\right)^3" }},
    {  "exp", {     "e^{",        "",          "}" }},
  };

  if (index < _cgp->numInputs) {
    ofs << inputName(index);
    usedInputs.insert(index);
    return;
  }

  auto functionNames = _cgp->funcSet->functionNames;

  index -= _cgp->numInputs;
  node *n = _cgp->nodes[index];

  const char *fname = functionNames[n->function];
  LatexFormat format;
  if (formats.find(fname) == formats.end())
    format = { std::string(fname) + "(", ",", ")" };
  else    format = formats.at(fname);

  ofs << format.prefix;

  int arity = getChromosomeNodeArity(_cgp, index);
  if (arity > 0) {
    toLatex(ofs, n->inputs[0], usedInputs);

    for (int i=1; i<arity; i++) {
      ofs << format.infix;
      toLatex(ofs, n->inputs[i], usedInputs);
    }
  }

  ofs << format.suffix;
}

} // end of namespace cgp
