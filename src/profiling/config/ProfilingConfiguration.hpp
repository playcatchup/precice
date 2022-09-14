#pragma once

#include <string>
#include "logging/Logger.hpp"
#include "xml/XMLTag.hpp"

namespace precice::profiling {

/**
 * @brief Configuration class for exports.
 */
class ProfilingConfiguration : public xml::XMLTag::Listener {
public:
  ProfilingConfiguration(xml::XMLTag &parent);

  virtual void xmlTagCallback(const xml::ConfigurationContext &context, xml::XMLTag &callingTag);

  /// Callback from automatic configuration. Not utilitzed here.
  virtual void xmlEndTagCallback(const xml::ConfigurationContext &context, xml::XMLTag &callingTag) {}

private:
  logging::Logger _log{"profiling::ProfilingConfiguration"};
};

} // namespace precice::profiling
