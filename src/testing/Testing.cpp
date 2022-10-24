#include <algorithm>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/test/framework.hpp>
#include <cstdlib>
#include <string>

#include "logging/LogMacros.hpp"
#include "logging/Logger.hpp"
#include "testing/Testing.hpp"
#include "utils/assertion.hpp"

namespace precice::testing {

std::string getPathToRepository()
{
  precice::logging::Logger _log("testing");
  char *                   preciceRoot = std::getenv("PRECICE_ROOT");
  PRECICE_CHECK(preciceRoot != nullptr,
                "Environment variable PRECICE_ROOT is required to run the tests, but has not been set. Please set it to the root directory of the precice repository.");

  // Cleanup the path by canonicalising it.
  boost::filesystem::path root(preciceRoot);
  return boost::filesystem::weakly_canonical(root).string();
}

std::string getPathToSources()
{
  return getPathToRepository() + "/src";
}

std::string getPathToTests()
{
  return getPathToRepository() + "/tests";
}

std::string getTestPath()
{
  const auto &cspan = boost::unit_test::framework::current_test_case().p_file_name;
  return {cspan.begin(), cspan.end()};
}

std::string getTestName()
{
  return boost::unit_test::framework::current_test_case().p_name;
}

} // namespace precice::testing
