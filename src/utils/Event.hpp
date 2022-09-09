#pragma once

#include <chrono>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "logging/Logger.hpp"

namespace precice {
namespace utils {

/// Represents an event that can be started and stopped.
/** Additionally to the duration there is a special property that can be set for a event.
A property is a a key-value pair with a numerical value that can be used to trace certain events,
like MPI calls in an event. It is intended to be set by the user. */
class Event {
public:
  enum class State : int {
    STOPPED = 0,
    RUNNING = 1,
    PAUSED  = 2,
  };

  /// Default clock type. All other chrono types are derived from it.
  using Clock = std::chrono::steady_clock;

  /// An Event can't be copied.
  Event(const Event &other) = delete;

  /// Name used to identify the timer. Events of the same name are accumulated to
  std::string name;

  /// Creates a new event and starts it, unless autostart = false
  Event(std::string eventName, bool autostart = true);

  /// Stops the event if it's running and report its times to the EventRegistry
  ~Event();

  /// Starts or restarts a stoped event.
  void start();

  /// Pauses a running event.
  void pause();

  /// Resumes a paused event.
  void resume();

  /// Stops a running event.
  void stop();

  /// Adds named integer data, associated to an event.
  void addData(const std::string &key, int value);

private:
  std::string _name;
  State       _state = State::STOPPED;
};

/// Class that changes the prefix in its scope
class ScopedEventPrefix {
public:
  ScopedEventPrefix(const std::string &name);

  ~ScopedEventPrefix();

  void pop();

private:
  std::string previousName = "";
};

} // namespace utils
} // namespace precice
