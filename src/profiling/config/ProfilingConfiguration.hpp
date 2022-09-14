#pragma once

#include <string>
#include "logging/Logger.hpp"
#include "xml/XMLTag.hpp"

namespace precice::profiling {

/**
 * @brief Configuration class for exports.
 */
class ProfilingConfiguration final : public xml::XMLTag::Listener {
public:
  ProfilingConfiguration(xml::XMLTag &parent);

  ~ProfilingConfiguration() override = default;

  void xmlTagCallback(const xml::ConfigurationContext &context, xml::XMLTag &callingTag) override;

  void xmlEndTagCallback(const xml::ConfigurationContext &context, xml::XMLTag &callingTag) override{};

private:
  logging::Logger _log{"profiling::ProfilingConfiguration"};
};

} // namespace precice::profiling
