#pragma once

#include <chrono>
#include <iosfwd>
#include <map>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>
#include "Event.hpp"

namespace precice {
namespace utils {

struct PendingEvent {
  PendingEvent(char t, const std::string &n, Event::Clock::time_point c)
      : type(t), ename(n), clock(c) {}
  PendingEvent(char t, const std::string &en, Event::Clock::time_point c, const std::string &dn, int dv)
      : type(t), ename(en), clock(c), dname(dn), dvalue(dv) {}

  char                     type;
  std::string              ename;
  Event::Clock::time_point clock;
  std::string              dname;
  int                      dvalue;
};

/// High level object that stores data of all events.
/** Call EventRegistry::initialize at the beginning of your application and
EventRegistry::finalize at the end. Event timings will be usable without calling this
function at all, but global timings as well as percentages do not work this way.  */
class EventRegistry {
public:
  /// Deleted copy operator for singleton pattern
  EventRegistry(EventRegistry const &) = delete;

  ~EventRegistry();

  /// Deleted assignment operator for singleton pattern
  void operator=(EventRegistry const &) = delete;

  /// Returns the only instance (singleton) of the EventRegistry class
  static EventRegistry &instance();

  /// Sets the global start time
  /**
   * @param[in] applicationName A name that is added to the logfile to distinguish different participants
   * @param[in] filePrefix A prefix for the file name.
   * @param[in] rank the current number of the parallel instance
   * @param[in] size the total number of a parallel instances
   */
  void initialize(std::string applicationName = "", std::string runName = "", int rank = 0, int size = 1);

  /// Sets the global end time
  void finalize();

  /// Clears the registry. needed for tests
  void clear();

  /// Records an event
  void put(PendingEvent pe);

  template <typename... Args>
  void put(Args &&... args)
  {
    put(PendingEvent{std::forward<Args>(args)...});
  }

  void flush();

  /// Currently active prefix. Changing that applies only to newly created events.
  std::string prefix;

private:
  /// A name that is added to the logfile to distinguish different participants
  std::string _applicationName;

  /// The optional file prefix, may be empty
  std::string _prefix;

  int _rank;

  int _size;

  /// Private, empty constructor for singleton pattern
  EventRegistry() = default;

  std::vector<PendingEvent> _writeQueue;

  std::ofstream _output;

  bool _initialized = false;

  bool _finalized = false;

  Event::Clock::time_point              _initClock;
  std::chrono::system_clock::time_point _initTime;

  void startBackend();
  void stopBackend();
};

} // namespace utils
} // namespace precice
