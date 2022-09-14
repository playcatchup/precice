#include <algorithm>
#include <array>
#include <cassert>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <ratio>
#include <string>
#include <sys/types.h>
#include <tuple>
#include <utility>

#include "profiling/Event.hpp"
#include "profiling/EventUtils.hpp"
#include "utils/assertion.hpp"
#include "utils/fmt.hpp"

namespace precice::profiling {

using sys_clk  = std::chrono::system_clock;
using stdy_clk = std::chrono::steady_clock;

/// Converts the time_point into a string like "2019-01-10T18:30:46.834"
std::string timepoint_to_string(sys_clk::time_point c)
{
  using namespace std::chrono;
  std::time_t ts = sys_clk::to_time_t(c);
  auto        ms = duration_cast<microseconds>(c.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << std::put_time(std::localtime(&ts), "%FT%T") << "." << std::setw(3) << std::setfill('0') << ms.count();
  return ss.str();
}

// -----------------------------------------------------------------------

EventRegistry::~EventRegistry()
{
  finalize();
}

EventRegistry &EventRegistry::instance()
{
  static EventRegistry instance;
  return instance;
}

void EventRegistry::initialize(std::string applicationName, std::string filePrefix, int rank, int size)
{
  auto initClock = Event::Clock::now();
  auto initTime  = std::chrono::system_clock::now();

  this->_applicationName = std::move(applicationName);
  this->_prefix          = std::move(filePrefix);
  this->_rank            = rank;
  this->_size            = size;
  this->_initTime        = initTime;
  this->_initClock       = initClock;

  if (_mode != Mode::Off) {
    startBackend();
  }

  _initialized = true;
  _finalized   = false;
}

void EventRegistry::setWriteQueueMax(std::size_t size)
{
  _writeQueueMax = size;
}

void EventRegistry::setMode(Mode mode)
{
  _mode = mode;
}

void EventRegistry::startBackend()
{
  PRECICE_ASSERT(_mode != Mode::Off, "The profiling is off.")
  if (_prefix.empty()) {
    _output.open(fmt::format(
        "{}-{}-{}.json",
        _applicationName, _rank, _size));
  } else {
    _output.open(fmt::format(
        "{}-{}-{}-{}.json",
        _prefix, _applicationName, _rank, _size));
  }

  // write header
  fmt::print(_output,
             R"({{
  "meta":{{
  "name" : "{}",
  "rank" : "{}",
  "size" : "{}",
  "unix_us" : "{}",
  "tinit": "{}"
  }},
  "events":[
    {{"et":"b","en":"_GLOBAL","ts":"0"}})",
             _applicationName,
             _rank,
             _size,
             std::chrono::duration_cast<std::chrono::microseconds>(_initTime.time_since_epoch()).count(),
             timepoint_to_string(_initTime));
  _output.flush();
  _writeQueue.clear();
}

void EventRegistry::stopBackend()
{
  PRECICE_ASSERT(_mode != Mode::Off, "The profiling is off.")
  // create end of global event
  auto now = Event::Clock::now();
  put(EventType::Stop, "_GLOBAL", now);
  // flush the queue
  flush();
  _output << "]}";
  _output.close();
}

void EventRegistry::finalize()
{
  if (_finalized)
    return;

  if (_mode != Mode::Off) {
    stopBackend();
  }

  _initialized = false;
  _finalized   = true;
}

void EventRegistry::clear()
{
  _writeQueue.clear();
}

void EventRegistry::put(PendingEvent pe)
{
  PRECICE_ASSERT(_mode != Mode::Off, "The profiling is off.")
  _writeQueue.push_back(std::move(pe));
  if (_writeQueueMax > 0 && _writeQueue.size() > _writeQueueMax) {
    flush();
  }
}

void EventRegistry::flush()
{
  if (_mode == Mode::Off) {
    return;
  }

  for (const auto &pe : _writeQueue) {
    auto msSinceInit = std::chrono::duration_cast<std::chrono::microseconds>(pe.clock - _initClock).count();

    if (pe.dname.empty()) {
      fmt::print(_output,
                 R"(,{{"et":"{}","en":"{}","ts":"{}"}})",
                 pe.type, pe.ename, msSinceInit);
    } else {
      fmt::print(_output,
                 R"(,{{"et":"{}","en":"{}","ts":"{}","dn":"{}","dv":"{}"}})",
                 pe.type, pe.ename, msSinceInit, pe.dname, pe.dvalue);
    }
  }
  _output.flush();
  _writeQueue.clear();
}

} // namespace precice::profiling
