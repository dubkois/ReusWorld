#ifndef DEPENDENCIES_H
#define DEPENDENCIES_H

#include "kgd/settings/configfile.h"

namespace config {

struct CONFIG_FILE(Dependencies) {
  DECLARE_CONST_PARAMETER(std::string, cxxoptsCommitHash)
  DECLARE_CONST_PARAMETER(std::string, cxxoptsCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, cxxoptsCommitDate)

  DECLARE_CONST_PARAMETER(std::string, jsonCommitHash)
  DECLARE_CONST_PARAMETER(std::string, jsonCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, jsonCommitDate)

  DECLARE_CONST_PARAMETER(std::string, toolsBuildDate)
  DECLARE_CONST_PARAMETER(std::string, toolsCommitHash)
  DECLARE_CONST_PARAMETER(std::string, toolsCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, toolsCommitDate)

  DECLARE_CONST_PARAMETER(std::string, aptBuildDate)
  DECLARE_CONST_PARAMETER(std::string, aptCommitHash)
  DECLARE_CONST_PARAMETER(std::string, aptCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, aptCommitDate)

  DECLARE_CONST_PARAMETER(std::string, reusWorldBuildDate)
  DECLARE_CONST_PARAMETER(std::string, reusWorldCommitHash)
  DECLARE_CONST_PARAMETER(std::string, reusWorldCommitMsg)
  DECLARE_CONST_PARAMETER(std::string, reusWorldCommitDate)
};
}

#endif // DEPENDENCIES_H
