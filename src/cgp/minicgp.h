#ifndef MINI_CGP_H
#define MINI_CGP_H

#include <iomanip>
#include <list>

#include "kgd/settings/configfile.h"
#include "kgd/utils/functions.h"

namespace simu {
void save (nlohmann::json &j, const rng::FastDice &d);
void load (const nlohmann::json &j, rng::FastDice &d);
} // end of namespace simu

namespace utils {

template <typename T, typename F, size_t... Is>
static auto make_array (F f, std::index_sequence<Is...>)
  -> std::array<T, sizeof...(Is)> {
  return {{f(std::integral_constant<std::size_t, Is>{})...}};
}

template <typename T, size_t COUNT, typename F>
static auto make_array (F f) {
  return make_array<T>(f, std::make_index_sequence<COUNT>{});
}

} // end of namespace utils

namespace cgp {
namespace functions {
using ID = std::array<unsigned char, 4>;

constexpr ID FID (const std::string_view &s) {
  if (std::tuple_size<ID>::value < s.size())
    utils::doThrow<std::invalid_argument>("Function name '", s, "' is too long");

  ID fid {};
  uint i = 0;
  for (; i<fid.size() - s.size(); i++) fid[i] = '_';
  for (; i<fid.size(); i++) fid[i] = s[i-fid.size()+s.size()];

  return fid;
}

std::ostream& operator<< (std::ostream &os, const ID &id);
std::istream& operator>> (std::istream &os, ID &id);

template <size_t A>
using Inputs = std::array<double, A>;

} // end of namespace functions

using FuncID = functions::ID;
} // end of namespace cgp

namespace config {

struct CONFIG_FILE(CGP) {
  using FunctionSet = std::set<cgp::FuncID>;
  DECLARE_PARAMETER(FunctionSet, functionSet)
};

} // end of namespace config

namespace cgp {

template <typename IEnum, uint N, typename OEnum, uint A = 2>
class CGP {
  enum NodeType { IN = 1<<1, INTERNAL = 1<<2, OUT = 1<<3 };

  using IUtils = EnumUtils<IEnum>;
  static constexpr uint I = IUtils::size();

  using OUtils = EnumUtils<OEnum>;
  static constexpr uint O = OUtils::size();

  enum Connection {  BEGIN = 0, END = I+N-1  };
  static bool isInput (Connection c) { return c < I; }
  static bool isNode (Connection c) { return I <= c; }

  using InputID = uint;
  using NodeID = uint;
  using OutputID = uint;

  static bool isValid (NodeID nid) { return nid < N; }
  static NodeID toNodeID (Connection c) { return c - I; }

  static Connection fromNodeID (NodeID nid) { return Connection(nid + I); }

  using Connections = std::array<Connection, A>;
  using Function = std::function<double(const functions::Inputs<A> &)>;

  struct Node {
    Connections connections;

    FuncID fid;
    Function function;

    Node (void) : Node({}, functions::FID("!!!!"), nullptr) {}
    Node (const Connections &c, FuncID id, const Function f)
      : connections(c), fid(id), function(f) {}

    friend bool operator== (const Node &lhs, const Node &rhs) {
      return lhs.connections == rhs.connections
          && lhs.fid == rhs.fid;
    }

    friend bool operator!= (const Node &lhs, const Node &rhs) {
      return !(lhs == rhs);
    }

    friend void assertEqual (const Node &lhs, const Node &rhs) {
      using utils::assertEqual;
      assertEqual(lhs.connections, rhs.connections);
      assertEqual(lhs.fid, rhs.fid);
    }
  };

  using Nodes = std::array<Node, N>;
  Nodes nodes;

  std::array<Connection, O> outputConnections;

  using Data = std::vector<double>;
  Data persistentData;

  using UsedNodes = std::map<NodeID, uint>;
  UsedNodes usedNodes; /// Maps into node id into data

  using FunctionsMap = std::map<FuncID, Function>;
  static const FunctionsMap functionsMap; // Stateless functions
  FunctionsMap localFunctionsMap;   // Context-dependent (e.g. rand)
  
  static const std::map<FuncID, uint> arities;

  struct LatexFormatter {
    using Formatter = std::function<void(std::ostream&)>;
    Formatter before, separator, after;
  };
  static const std::map<FuncID, LatexFormatter> latexFormatters;
  static const LatexFormatter defaultLatexFormatter;

  rng::FastDice ldice;

public:
  using Inputs = std::array<double, I>;
  using Outputs = std::array<double, O>;

  CGP (void) {
    for (Node &n: nodes) {
      n.fid = functions::FID("UINT");
      n.function = nullptr;
      for (Connection &c: n.connections)  c = Connection(0);
    }
    for (Connection &o: outputConnections)  o = Connection(0);
  }

  void clear (void) {
    std::fill(persistentData.begin(), persistentData.end(), 0);
  }

  void prepare (void) {
    usedNodes.clear();
    uint d = 0;
    
    std::array<bool, N> toEvaluate {false};

    for (uint i=0; i<O; i++) {
      Connection c = outputConnections[i];
      if (isNode(c))  toEvaluate[toNodeID(c)] = true;
    }
    
    for (NodeID nid=N-1; isValid(nid); nid--) {
      if (!toEvaluate[nid]) continue;

      Node &node = nodes[nid];
      uint a = arity(node);
      for (uint j=0; j<a; j++) {
        Connection c = node.connections[j];
        if (isNode(c))  toEvaluate[toNodeID(c)] = true;
      }

      usedNodes[nid] = I+d;
      d++;
    }

    persistentData.resize(I+usedNodes.size(), 0);
  }
  
  void evaluate (const Inputs &inputs, Outputs &outputs) {
    std::copy(inputs.begin(), inputs.end(), persistentData.begin());

    std::array<double, A> localInputs;
    for (const auto &p: usedNodes) {
      uint i = p.second;
      const Node &n = nodes[p.first];
      
      uint a = arity(n);
      for (uint j=0; j<a; j++)
        localInputs[j] = data(n.connections[j]);
      persistentData[i] = n.function(localInputs);
    }
    
    for (uint o=0; o<O; o++)
      outputs[o] = utils::clip(-1., data(I+N+o), 1.);

    for (double d: persistentData)  assert(!isnan(d));
  }

  CGP clone (void) const {
    return CGP(*this);
  }

  /// Uses parameter-less procedure 'single' from "Reducing Wasted Evaluations
  /// in Cartesian Genetic Programming"
  /// https://doi.org/10.1007/978-3-642-37207-0_6
  uint mutate (rng::AbstractDice &dice) {
    static constexpr auto GENES_PER_NODE = A+1;
    static constexpr auto NODE_GENES = N*GENES_PER_NODE;
    static constexpr auto TOTAL_GENES = NODE_GENES + O;

    uint n = 0;
    bool activeMutated = false;
    while (!activeMutated) {
      // Must be uniform over the whole genome
      auto gene = dice(0u, TOTAL_GENES-1);
      if (gene < NODE_GENES) {
        NodeID nid = gene / GENES_PER_NODE; // Get index of node
        Node &n = nodes[nid], old = n;
        auto field = gene % GENES_PER_NODE; // Get index in node

//        std::cerr << "Mutated node " << nid << "'s ";

        if (field == 0) { // Generate new function
          n.fid = *dice(config::CGP::functionSet());
          n.function = functionfromID(n.fid);

//          using functions::operator<<;
//          std::cerr << "function from " << old.fid << " to " << n.fid;

        } else {  // Change connection
          n.connections[field-1] = Connection(dice(0u, I+nid-1));
//          std::cerr << field-1 << "th connection from "
//                    << old.connections[field-1] << " to "
//                    << n.connections[field-1];
        }

        if (usedNodes.find(fromNodeID(nid)) != usedNodes.end() && n != old)
          activeMutated = true;

      } else {
        auto o = gene - NODE_GENES;
        auto old = outputConnections[o];
        outputConnections[o] = Connection(dice(0u, I+N-1));
        activeMutated = (outputConnections[o] != old);
//        std::cerr << "Mutated output " << o << " from " << old << " to "
//                  << outputConnections[o];
      }

//      std::cerr << std::endl;

      n++;
    }

    prepare();

    return n;
  }

  friend std::ostream& operator<< (std::ostream &os, const CGP &cgp) {
    auto flags = os.flags();
    const auto pad = [] { return std::setw(4); };

    os << "CGP {\n\t" << std::setfill(' ');
    for (IEnum i: IUtils::iterator()) os << pad() << IUtils::getName(i, false)
                                         << " ";
    os << "\n\t" << std::setfill('0');
    for (uint i=0; i<I; i++)  os << pad() << i << " ";
    os << "\n\n\tFunc In...\n";
    for (uint i=0; i<N; i++) {
      os << pad() << I+i << "\t";
      const Node &n = cgp.nodes[i];
      for (auto c: n.fid)  os << c;
      for (uint i=0; i<A; i++)
        os << " " << pad() << n.connections[i];
      os << "\n";
    }
    os << "\n\t" << std::setfill(' ');
    for (OEnum o: OUtils::iterator()) os << pad() << OUtils::getName(o, false)
                                         << " ";
    os << "\n\t" << std::setfill('0');
    for (NodeID nid: cgp.outputConnections)
      os << pad() << nid << " ";
    os << "\n}" << std::endl;

    os.flags(flags);
    return os;
  }

  std::string toTex (void) const {
    std::ostringstream oss;
    oss << "\\begin{align*}\n";

    for (uint o=0; o<O; o++) {
      std::set<NodeID> inputs;
      oss << OUtils::getName(o, false);

      std::ostringstream oss2;
      toTexRec(oss2, outputConnections[o], inputs);

      if (!inputs.empty()) {
        oss << "(" << IUtils::getName(*inputs.begin(), false);
        for (auto it = std::next(inputs.begin()); it != inputs.end(); ++it)
          oss << ", " << IUtils::getName(*it, false);
        oss << ") ";
      }

      oss << " &= " << oss2.str() << " \\\\\n";
    }

    oss << "\\end{align*}\n";
    return oss.str();
  }

  enum DotOptions {
    DEFAULT = 0,
    FULL = 1<<1,
    NO_ARITY = 1<<2,
    SHOW_DATA = 1<<3
  };
  friend DotOptions operator| (DotOptions lhs, DotOptions rhs) {
    return DotOptions(int(lhs) | int(rhs));
  }

  void toDot (const stdfs::path &outfile, DotOptions options = DEFAULT) const {

    auto ext = outfile.extension().string().substr(1);
    auto dotfile = outfile;
    if (ext != "dot")
      dotfile.replace_extension("dot");

    std::ofstream ofs (dotfile);
    if (!ofs) {
      std::cerr << "Failed to open '" << dotfile << "' for writing in "
                << __PRETTY_FUNCTION__ << std::endl;
      return;
    }

    ofs << "digraph {\n";

    bool d = (options & DotOptions::SHOW_DATA);

    for (auto i: IUtils::iteratorUValues())
      printDotNodeOrIO(ofs, i, IUtils::getName(i, false), d);

    ofs << "\t{rank=source";
    for (uint i=0; i<I; i++)  ofs << " " << i;
    ofs << "}\n\n";

    if (options & DotOptions::FULL) {
      for (uint n=0; n<N; n++) {
        uint i = I+n;
        const Node &node = nodes[n];
        printDotNode(ofs, i, node.fid, d);
        uint a = A;
        if (!(options & DotOptions::NO_ARITY))  a = arity(node);
        for (uint j=0; j<a; j++)
          ofs << "\t" << node.connections[j] << " -> " << i << ";\n";
      }
      for (uint o=0; o<O; o++)
        ofs << "\t" << outputConnections[o] << " -> " << I+N+o << ";\n";

    } else { // only print used nodes
      std::set<Connection> seen, pending;
      for (uint o=0; o<outputConnections.size(); o++) {
        Connection c = outputConnections[o];
        pending.insert(c);
        ofs << "\t" << c << " -> " << o+N+I << ";\n";
      }
      while (!pending.empty()) {
        Connection c = *pending.begin();
        pending.erase(pending.begin());

        if (seen.find(c) != seen.end()) continue;
        seen.insert(c);

        if (isInput(c)) continue;

        NodeID nid = toNodeID(c);
        const Node &n = nodes[nid];
        printDotNode(ofs, c, n.fid, d);
        uint a = A;
        if (!(options & DotOptions::NO_ARITY))  a = arity(n);
        for (uint j=0; j<a; j++) {
          Connection c_ = n.connections[j];
          pending.insert(c_);
          ofs << "\t" << c_ << " -> " << c << "\n";
        }
      }
    }
    ofs << "\n";

    ofs << "\t0 -> " << I+N << " [style=invis, constraint=false];\n\n";

    for (auto o: OUtils::iteratorUValues())
      printDotNodeOrIO(ofs, I+N+o, OUtils::getName(o, false), d);

    ofs << "\t{rank=sink";
    for (uint o=0; o<O; o++)  ofs << " " << I+N+o;
    ofs << "}\n";

    ofs << "}" << std::endl;

    if (ext != "dot") {
      std::ostringstream oss;
      oss << "dot -T" << ext << " -o " << outfile << " " << dotfile;
      std::string cmd = oss.str();
      auto res = system(cmd.c_str());
      if (res == 0)
        std::cout << "Saved cgp to " << dotfile << " and rendering to "
                  << outfile << std::endl;
      else
        std::cerr << "Executing '" << cmd << "' resulted in error code " << res
                  << std::endl;
    }
  }

  static CGP null (rng::AbstractDice &dice) {
    CGP cgp (dice);
    FuncID fid = functions::FID("zero");
    cgp.nodes[0].fid = fid;
    cgp.nodes[0].function = functionsMap.at(fid);
    for (Connection &o: cgp.outputConnections)  o = Connection(I);
    cgp.prepare();
    return cgp;
  }

  friend bool operator== (const CGP &lhs, const CGP &rhs) {
    return lhs.nodes == rhs.nodes
        && lhs.outputConnections == rhs.outputConnections
        && lhs.persistentData == rhs.persistentData
        && lhs.usedNodes == rhs.usedNodes
        && lhs.ldice == rhs.ldice;
  }

  friend void assertEqual (const CGP &lhs, const CGP &rhs) {
    using utils::assertEqual;
    assertEqual(lhs.nodes, rhs.nodes);
    assertEqual(lhs.outputConnections, rhs.outputConnections);
    assertEqual(lhs.persistentData, rhs.persistentData);
    assertEqual(lhs.usedNodes, rhs.usedNodes);
//    assertEqual(lhs.localFunctionsMap, rhs.localFunctionsMap);
    assertEqual(lhs.ldice, rhs.ldice);
  }

  friend void to_json (nlohmann::json &j, const CGP &cgp) {
    j["a"] = { I, N, O, A };
    j["d"] = cgp.ldice.getSeed();
    j["n"] = cgp.nodes;
    j["o"] = cgp.outputConnections;
  }

  friend void to_json (nlohmann::json &j, const Node &n) {
    j = { toString(n.fid), n.connections };
  }

  friend void from_json (const nlohmann::json &j, CGP &cgp) {
    checkTemplateArguments(j["a"]);

    cgp.ldice.reset(j["d"]);
    cgp.registerLocalFunctions();

    for (uint i=0; i<N; i++) {
      const auto &jn = j["n"][i];
      auto &n = cgp.nodes[i];
      n.fid = functions::FID(jn[0]);
      n.function = cgp.functionfromID(n.fid);
      n.connections = jn[1];
    }
    cgp.outputConnections = j["o"];

    cgp.prepare();
  }

  static void save (nlohmann::json &j, const CGP &cgp) {
    uint i=0;
    j[i++] = { I, N, O, A };
    simu::save(j[i++], cgp.ldice);
    j[i++] = cgp.nodes;
    j[i++] = cgp.outputConnections;
    j[i++] = cgp.persistentData;
    j[i++] = cgp.usedNodes;
  }

  static void load (const nlohmann::json &j, CGP &cgp) {
    uint i=0;
    checkTemplateArguments(j[i++]);

    simu::load(j[i++], cgp.ldice);

    auto jnodes = j[i++];
    for (uint i=0; i<N; i++) {
      const auto &jn = jnodes[i];
      auto &n = cgp.nodes[i];
      n.fid = functions::FID(jn[0]);
      n.function = cgp.functionfromID(n.fid);
      n.connections = jn[1];
    }

    cgp.outputConnections = j[i++];
    cgp.persistentData = j[i++].get<Data>();
    cgp.usedNodes = j[i++].get<UsedNodes>();

    cgp.registerLocalFunctions();
  }

private:
  CGP (rng::AbstractDice &dice) {
    registerLocalFunctions();

    using utils::make_array;
    nodes = make_array<Node, N>([this, &dice] (auto i) {
      Connections connections =
        make_array<Connection, A>([&dice, i] (auto) {
          return Connection(dice(0u, uint(I+i-1)));
        });

      FuncID fid = *dice(config::CGP::functionSet());
      Function function = functionfromID(fid);

      return Node(connections, fid, function);
    });

    outputConnections = make_array<Connection, O>([&dice] (auto) {
      return Connection(dice(0u, I+N-1));
    });
  }

  uint arity (const Node &n) const {
    return std::min(A, arities.at(n.fid));
  }

  static void checkTemplateArguments (const nlohmann::json &j) {
    if (j[0] != I)
      utils::doThrow<std::invalid_argument>("Wrong number of inputs");
    if (j[1] != N)
      utils::doThrow<std::invalid_argument>("Wrong number of internal nodes");
    if (j[2] != O)
      utils::doThrow<std::invalid_argument>("Wrong number of outputs");
    if (j[3] != A)
      utils::doThrow<std::invalid_argument>("Wrong arity");
  }

  void registerLocalFunctions (void) {
    using functions::FID;
    localFunctionsMap[FID("rand")] = [this] (auto) { return ldice(-1., 1.); };
  }

  Function functionfromID (const FuncID &fid) const {
    auto lit = localFunctionsMap.find(fid);
    if (lit != localFunctionsMap.end()) return lit->second;

    auto rit = functionsMap.find(fid);
    if (rit != functionsMap.end())      return rit->second;

    utils::doThrow<std::invalid_argument>("Unknown function ", fid);
    return nullptr;
  }

  static std::string toString (const FuncID &fid) {
    std::string fidStr (fid.begin(), fid.end());
    fidStr.erase(std::remove(fidStr.begin(), fidStr.end(), '_'), fidStr.end());
    return fidStr;
  }

  void toTexRec (std::ostream &os, NodeID i,
                 std::set<NodeID> &usedInputs) const {

    using functions::operator<<;
    if (i < I) {
      os << IUtils::getName(i, false);
      usedInputs.insert(i);

    } else {
      const Node &node = nodes[i-I];

      LatexFormatter fmt = defaultLatexFormatter;
      auto it = latexFormatters.find(node.fid);
      if (it != latexFormatters.end())  fmt = it->second;
      else
        os << toString(node.fid);

      uint a = arity(node);
      fmt.before(os);
      for (uint i=0; i<a; i++) {
        if (i > 0)  fmt.separator(os);
        toTexRec(os, node.connections[i], usedInputs);
      }
      fmt.after(os);
    }
  }

  /// 0 <= id < I+N+O
  void printDotNodeOrIO (std::ostream &os, uint id,
                         const std::string &func, bool withData) const {
    os << "\t" << id << " [shape=none, label=<"
        << "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\""
                 " VALIGN=\"MIDDLE\">"
         << "<TR><TD><FONT POINT-SIZE=\"8\"><I>\\N</I></FONT></TD>"
            "<TD><B>" << func << "</B></TD></TR>";
    if (withData)
      os << "<TR><TD COLSPAN=\"2\">"
             "<FONT POINT-SIZE=\"10\">" << data(id) << "</FONT>"
            "</TD></TR>";
    os << "</TABLE>>];\n";
  }

  /// I <= id < I+N
  void printDotNode (std::ostream &os, uint id, const FuncID &fid,
                     bool withData) const {
    printDotNodeOrIO(os, id, toString(fid), withData);
  }

  double data (uint i) const {
    if (i < I)  return persistentData[i];
    if (i < I+N)  return persistentData[usedNodes.at(i-I)];
    return data(outputConnections[i-I-N]);
  }
};


namespace functions {

namespace oary {
template <uint S>
double one (const Inputs<S> &) {  return 1;     }

template <uint S>
double zero (const Inputs<S> &) { return 0;     }

template <uint S>
double pi (const Inputs<S> &) {   return M_PI;  }
} // end of namespace oary

namespace unary {
template <uint S>
double id (const Inputs<S> &inputs) {
  return inputs[0];
}

template <uint S>
double abs (const Inputs<S> &inputs) {
  return std::fabs(inputs[0]);
}

template <uint S>
double sq (const Inputs<S> &inputs) {
  return inputs[0] * inputs[0];
}

template <uint S>
double sqrt (const Inputs<S> &inputs) {
  if (inputs[0] < 0)  return 0;
  return std::sqrt(inputs[0]);
}

template <uint S>
double exp (const Inputs<S> &inputs) {
  return std::exp(inputs[0]);
}

template <uint S>
double sin (const Inputs<S> &inputs) {
  return std::sin(inputs[0]);
}

template <uint S>
double cos (const Inputs<S> &inputs) {
  return std::cos(inputs[0]);
}

template <uint S>
double tan (const Inputs<S> &inputs) {
  return std::tan(inputs[0]);
}

template <uint S>
double tanh (const Inputs<S> &inputs) {
  return std::tanh(inputs[0]);
}

template <uint S>
double step (const Inputs<S> &inputs) {
  return utils::sgn(inputs[0]);
}
} // end of namespace unary

namespace binary {
template <uint S>
double del (const Inputs<S> &inputs) {
  return inputs[0] - inputs[1];
}

template <uint S>
double div (const Inputs<S> &inputs) {
  if (inputs[1] != 0)
    return inputs[0] / inputs[1];
  return 0;
}

template <uint S>
double hgss (const Inputs<S> &inputs) {
  if (inputs[1] == 0) return inputs[0] == 0 ? 1 : 0;
  return utils::gauss(inputs[0], 0., inputs[1]);
}
} // end of namespace binary

namespace nary {
template <uint S>
double add (const Inputs<S> &inputs) {
  double sum = 0;
  for (auto i: inputs)  sum += i;
  return sum;
}

template <uint S>
double mult (const Inputs<S> &inputs) {
  double prod = 0;
  for (auto i: inputs)  prod *= i;
  return prod;
}
} // end of namespace nary

} // end of namespace functions


template <typename IE, uint N, typename OE, uint A>
const std::map<FuncID, typename CGP<IE,N,OE,A>::Function>
CGP<IE,N,OE,A>::functionsMap {
#define FUNC(X) std::make_pair(functions::FID(#X), &functions::ARITY::X<A>)
#define ARITY oary
//  FUNC(rand)
  FUNC(one),  FUNC(zero), FUNC(pi),
#undef ARITY

#define ARITY unary
  FUNC(id),   FUNC(abs),  FUNC(sq),   FUNC(sqrt), FUNC(exp),  FUNC(sin),
  FUNC(cos),  FUNC(tan),  FUNC(step),
#undef ARITY

#define ARITY binary
  FUNC(del),  FUNC(div),  FUNC(hgss),
#undef ARITY

#define ARITY nary
  FUNC(add),  FUNC(mult)
#undef ARITY
#undef FUNC
};


template <typename IE, uint N, typename OE, uint A>
const std::map<FuncID, uint> CGP<IE,N,OE,A>::arities {
#define AR(X) std::make_pair(functions::FID(#X), ARITY)
#define ARITY 0
  AR(rand), AR(one),  AR(zero), AR(pi),
#undef ARITY
#define ARITY 1
  AR(id),   AR(abs),  AR(sq),  AR(sqrt), AR(exp),  AR(sin),
  AR(cos),  AR(tan),  AR(step),
#undef ARITY
#define ARITY 2
  AR(del),  AR(div),  AR(hgss),
#undef ARITY
#define ARITY -1
  AR(add),  AR(mult)
#undef ARITY
#undef AR
};

template <typename IE, uint N, typename OE, uint A>
const typename CGP<IE,N,OE,A>::LatexFormatter
CGP<IE,N,OE,A>::defaultLatexFormatter {
  [] (std::ostream &os) { os << "("; },
  [] (std::ostream &os) { os << ", "; },
  [] (std::ostream &os) { os << ")"; }
};

template <typename IE, uint N, typename OE, uint A>
const std::map<FuncID, typename CGP<IE,N,OE,A>::LatexFormatter>
CGP<IE,N,OE,A>::latexFormatters {
#define FMT(NAME, BEFORE, SEPARATOR, AFTER) \
  { functions::FID(#NAME), \
    typename CGP<IE,N,OE,A>::LatexFormatter { BEFORE, SEPARATOR, AFTER } \
  }
#define PRINT(X) [] (std::ostream &os) { os << X; }
#define NOOP [] (std::ostream &) {}
  FMT(rand,    PRINT("rand"),         NOOP, NOOP),
  FMT( one,       PRINT("1"),         NOOP, NOOP),
  FMT(zero,       PRINT("0"),         NOOP, NOOP),
  FMT(  pi,    PRINT("\\pi"),         NOOP, NOOP),

  FMT(  id,             NOOP,         NOOP, NOOP),
  FMT( abs,       PRINT("|"),         NOOP, PRINT("|")),
  FMT(  sq,       PRINT("("),         NOOP, PRINT(")^2")),
  FMT(sqrt, PRINT("\\sqrt{"),         NOOP, PRINT("}")),
  FMT( exp,     PRINT("e^{"),         NOOP, PRINT("}")),

  FMT( del,       PRINT("("), PRINT(" - "), PRINT(")")),
  FMT( div, PRINT("\\frac{"),  PRINT("}{"), PRINT("}")),

  FMT( add,       PRINT("("), PRINT(" + "), PRINT(")")),
  FMT(mult,       PRINT("("), PRINT(" * "), PRINT(")")),
#undef NOOP
#undef PRINT
#undef FMT
};

} // end of namespace cgp

#endif
