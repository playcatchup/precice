#include "Constants.hpp"

namespace precice::cplscheme::constants {

const std::string &actionWriteIterationCheckpoint()
{
  static std::string actionWriteIterationCheckpoint("write-iteration-checkpoint");
  return actionWriteIterationCheckpoint;
}

const std::string &actionReadIterationCheckpoint()
{
  static std::string actionReadIterationCheckpoint("read-iteration-checkpoint");
  return actionReadIterationCheckpoint;
}

const std::string &actionWriteInitialData()
{
  static std::string actionWriteInitialData("write-initial-data");
  return actionWriteInitialData;
}

} // namespace precice::cplscheme::constants
