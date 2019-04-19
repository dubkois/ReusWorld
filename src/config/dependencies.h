#ifndef DEPENDENCIES_H
#define DEPENDENCIES_H

#include "kgd/settings/configfile.h"

namespace config {

struct CONFIG_FILE(Dependencies) {
  static constexpr auto projects = {
    "cxxopts", "json", "tools", "apt", "reus"
  };

  static constexpr auto fields = {
    "BuildDate", "BuildType", "CommitHash"
  };

  static const std::map<std::string, bool> buildables;

  DECLARE_CONST_PARAMETER(std::string, cxxoptsCommitHash)
  DECLARE_CONST_PARAMETER(std::string, cxxoptsCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, cxxoptsCommitDate)

  DECLARE_CONST_PARAMETER(std::string, jsonCommitHash)
  DECLARE_CONST_PARAMETER(std::string, jsonCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, jsonCommitDate)

  DECLARE_CONST_PARAMETER(std::string, toolsBuildDate)
  DECLARE_CONST_PARAMETER(std::string, toolsBuildType)
  DECLARE_CONST_PARAMETER(std::string, toolsCommitHash)
  DECLARE_CONST_PARAMETER(std::string, toolsCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, toolsCommitDate)

  DECLARE_CONST_PARAMETER(std::string, aptBuildDate)
  DECLARE_CONST_PARAMETER(std::string, aptBuildType)
  DECLARE_CONST_PARAMETER(std::string, aptCommitHash)
  DECLARE_CONST_PARAMETER(std::string, aptCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, aptCommitDate)

  DECLARE_CONST_PARAMETER(std::string, reusBuildDate)
  DECLARE_CONST_PARAMETER(std::string, reusBuildType)
  DECLARE_CONST_PARAMETER(std::string, reusCommitHash)
  DECLARE_CONST_PARAMETER(std::string, reusCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, reusCommitDate)

  using Save = std::map<std::string, std::string>;
  static Save saveState (void);
  static bool compareStates (const Save &previous,
                             std::string constraints);

  struct Help {
    friend std::ostream& operator<< (std::ostream &os, const Help&);
  };

private:
  using Keys = std::set<Dependencies::ConfigIterator::key_type>;
  static void keysToCheck(const std::string &constraints, Keys &keys,
                          bool &nodirty);
};
}

#endif // DEPENDENCIES_H
