#pragma once

#include <chrono>
#include <cstddef>
#include <iosfwd>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Event.hpp"

namespace precice {
namespace utils {

/// Types of events
enum struct EventType : char {
  Start  = 'b',
  Pause  = 'p',
  Resume = 'r',
  Stop   = 'e',
  Data   = 'd'
};

/// An event that has been recorded and it waiting to be written to file
struct PendingEvent {
  PendingEvent(EventType t, const std::string &n, Event::Clock::time_point c)
      : type(static_cast<char>(t)), ename(n), clock(c) {}
  PendingEvent(EventType t, const std::string &en, Event::Clock::time_point c, const std::string &dn, int dv)
      : type(static_cast<char>(t)), ename(en), clock(c), dname(dn), dvalue(dv) {}

  char                     type;
  std::string              ename;
  Event::Clock::time_point clock;
  std::string              dname;
  int                      dvalue;
};

/** High level object that stores data of all events.
 *
 * Call EventRegistry::initialize at the beginning of your application and
 * EventRegistry::finalize at the end.
 *
 * Use \ref setWriteQueueMax() to adjust buffering behaviour.
 */
class EventRegistry {
public:
  ~EventRegistry();

  /// Deleted copy and move SMFs for singleton pattern
  EventRegistry(EventRegistry const &) = delete;
  EventRegistry(EventRegistry &&)      = delete;
  EventRegistry &operator=(EventRegistry const &) = delete;
  EventRegistry &operator=(EventRegistry &&) = delete;

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

  /// Sets the maximum size of the writequeue before calling flush(). Use 0 to flush on destruction.
  void setWriteQueueMax(std::size_t size);

  /// Sets the global end time and flushes buffers
  void finalize();

  /// Clears the registry.
  void clear();

  /// Records an event
  void put(PendingEvent pe);

  /// Records an event, takes ctor arguments
  template <typename... Args>
  void put(Args &&... args)
  {
    put(PendingEvent{std::forward<Args>(args)...});
  }

  /// Writes all recorded events to file and flushes the buffer.
  void flush();

  /// Currently active prefix. Changing that applies only to newly created events.
  std::string prefix;

private:
  /// The name of the current participant
  std::string _applicationName;

  /// The optional file prefix, may be empty
  std::string _prefix;

  /// The rank/number of parallel instance of the current program
  int _rank;

  /// The amount of parallel instances of the current program
  int _size;

  /// Private, empty constructor for singleton pattern
  EventRegistry() = default;

  std::vector<PendingEvent> _writeQueue;
  std::size_t               _writeQueueMax = 0;

  std::ofstream _output;

  bool _initialized = false;

  bool _finalized = false;

  /// The initial time clock, used to take runtime measurements.
  Event::Clock::time_point _initClock;

  /// The initial time, used to describe when the run started.
  std::chrono::system_clock::time_point _initTime;

  /// Create the file and starts the filestream
  void startBackend();

  /// Stops the global event, flushes the buffers and closes the filestream
  void stopBackend();
};

} // namespace utils
} // namespace precice
