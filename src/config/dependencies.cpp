#include "dependencies.h"

namespace config {

const std::map<std::string, bool> Dependencies::buildables {
  std::make_pair("cxxopts", false),
  std::make_pair(   "json", false),
  std::make_pair(  "tools", true),
  std::make_pair(    "apt", true),
  std::make_pair(   "reus", true)
};

size_t firstUppercase (const std::string &s) {
  for (size_t i=0; i<s.length(); i++) if (std::isupper(s[i])) return i;
  return std::string::npos;
}

std::string prefix (const std::string &s) {
  size_t i = firstUppercase(s);
  return s.substr(0, i);
}

std::string suffix (std::string s) {
  size_t i = firstUppercase(s);
  s = s.substr(i);
  if (s == "Commit" || s == "Hash") s = "CommitHash";
  if (s == "Build" || s == "Date")  s = "BuildDate";
  return s;
}

void removeDirty(std::string &str) {
  static constexpr auto dirty = "-dirty";
  if (str.length() < std::strlen(dirty))  return;

  size_t i = str.length()-std::strlen(dirty);
  if (str.substr(i) == dirty) str = str.substr(0, i);
}

template <typename S>
void populate (S &set, const std::string &s) {
  for (const std::string &p: Dependencies::projects)
    if (s != "BuildDate" || Dependencies::buildables.at(p))
      set.insert(p);
}

template <typename T>
std::string toString (const T &v) {
  std::ostringstream oss;
  oss << v;
  return oss.str();
}

Dependencies::Save Dependencies::saveState (void) {
  Save save;
  for (const auto &p: config_iterator())
    save[p.first] = toString(p.second);
  return save;
}

void Dependencies::keysToCheck(const std::string &constraints, Keys &keys,
                               bool &nodirty) {

  std::map<std::string, std::set<std::string>> splitKeys;

  for (std::string c: utils::split(constraints, ',')) {

//    std::cerr << "Processing '" << c << "'";

    c = utils::trim(c);
    if (c == "none")      continue;
    else if (c == "nodirty")  nodirty = true;
    else if (c == "all") {
      for (const std::string &s: fields)
        populate(splitKeys[s], s);

    } else if (c.length() > 3 && c.substr(0, 3) == "all") {
      std::string s = suffix(c);
      populate(splitKeys[s], s);

    } else if (c.length() > 1) {
      bool neg = (c[0] == '!');
      if (neg)  c = c.substr(1);
      const auto s = suffix(c), p = prefix(c);
      if (!neg)  splitKeys[s].insert(p);
      else {
        auto &prefixes = splitKeys[s];
        if (prefixes.empty()) populate(splitKeys[s], s);
        prefixes.erase(p);
      }
    } else
      std::cerr << "Did not understand constraint '" << c << "'. Ignoring"
                << std::endl;

//    std::cerr << " >> " << c << "\n";
//    for (const auto &pair: splitKeys) {
//      std::cerr << "\t" << pair.first << ":\n";
//      for (const auto &prefix: pair.second)
//        std::cerr << "\t\t" << prefix << "\n";
//    }
  }

  for (const auto &pair: splitKeys)
    for (const auto &prefix: pair.second)
      keys.insert(prefix + pair.first);
}

bool Dependencies::compareStates (const Save &previous,
                                  std::string constraints) {

  const Save current = saveState();

  bool nodirty;
  Keys keys;
  if (constraints.empty())  constraints = "all";
  keysToCheck(constraints, keys, nodirty);
  std::cerr << "Constraints are: '" << constraints << "'\n"
            << "Checking keys:\n";
  for (const auto &k: keys) std::cerr << "\t'" << k << "'\n";
  std::cerr << std::endl;

  bool ok = true;
  for (const auto &key: keys) {
    auto lhsIT = previous.find(key);
    if (lhsIT == previous.end()) {
      ok = false;
      std::cerr << "Found previously unkown key '" << key << "'" << std::endl;
      continue;
    }

    auto rhsIT = current.find(key);
    if (rhsIT == current.end()) {
      ok = false;
      std::cerr << "Lost track of key '" << key << "'" << std::endl;
      continue;
    }

    std::string lhsStr, rhsStr;
    lhsStr = lhsIT->second;
    rhsStr = rhsIT->second;

    if (nodirty && suffix(key) == "CommitHash") {
      removeDirty(lhsStr);
      removeDirty(rhsStr);
    }

    std::cerr << key << ":\t" << lhsStr << "\t" << rhsStr << std::endl;
    if (lhsStr != rhsStr) {
      ok = false;
      std::cerr << "Mismatching values for key '" << key << "'\n"
                << "  previous: '" << lhsStr << "'\n"
                << "   current: '" << rhsStr << "'" << std::endl;
    }
  }

  return ok;
}

std::ostream& operator<< (std::ostream &os, const Dependencies::Help&) {
  return os <<
    "Option 'load-constraints' is a comma separated list of constraints"
    "\nFormat:"
    "\n\tC = none    (no constraint)"
    "\n\t  | nodirty (ignore 'dirty' flag when comparing hashes)"
    "\n\t  | all<F>  (activate all constraints for field 'F')"
    "\n\t  | <P><F>  (activate the constraint for field 'F' and project 'P')"
    "\n\t  | !<P><F> (deactivate the constraint for field 'F' and project 'P')"
    "\n\tP = "
     << utils::join(Dependencies::projects.begin(),
                    Dependencies::projects.end(), "\n\t  | ")
     <<
    "\n\tF = buildDate | Build | Date"
    "\n\t  | commitHash | Commit | Hash"
    "\nNotes:"
    "\n\tThe list is parsed linearly without much care for consistency"
    "\n\tE.g. writing 'none,all,none' is legal and behaves as expected"
    "\nExemples:"
    "\n\t                   all\tall git hash and build date must match"
    "\n\t       all[Commit]Hash\tall git hash must match"
    "\n\t  cxxoptsHash,jsonHash\tOnly git hashes for cxxopts and json must match"
    "\n\t         all,!reusDate\tAll but the build date for reus must match"
    "\n\t!jsonDate,!cxxoptsDate\tactivates all but those constraints for the "
     "date field";
}

}
