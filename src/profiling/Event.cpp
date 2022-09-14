#include "profiling/Event.hpp"
#include <sys/eventfd.h>
#include "profiling/EventUtils.hpp"
#include "utils/assertion.hpp"

namespace precice::profiling {

Event::Event(std::string eventName, bool autostart)
    : _name(std::move(eventName))
{
  _name = EventRegistry::instance().prefix + _name;
  if (autostart) {
    start();
  }
}

Event::Event(std::string eventName, FundamentalTag, bool autostart)
    : Event(std::move(eventName), autostart)
{
  _fundamental = true;
}

Event::~Event()
{
  if (_state == State::RUNNING) {
    stop();
  }
}

void Event::start()
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::STOPPED, _name);
  _state = State::RUNNING;

  if (EventRegistry::instance().accepting(toEventClass(_fundamental))) {
    EventRegistry::instance().put(EventType::Start, _name, timestamp);
  }
}

void Event::stop()
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::RUNNING, _name);
  _state = State::STOPPED;

  if (EventRegistry::instance().accepting(toEventClass(_fundamental))) {
    EventRegistry::instance().put(EventType::Stop, _name, timestamp);
  }
}

void Event::addData(const std::string &key, int value)
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::RUNNING, _name);

  if (EventRegistry::instance().accepting(toEventClass(_fundamental))) {
    EventRegistry::instance().put(EventType::Data, _name, timestamp, key, value);
  }
}

// -----------------------------------------------------------------------

ScopedEventPrefix::ScopedEventPrefix(std::string const &name)
{
  previousName = EventRegistry::instance().prefix;
  EventRegistry::instance().prefix += name;
}

ScopedEventPrefix::~ScopedEventPrefix()
{
  pop();
}

void ScopedEventPrefix::pop()
{
  EventRegistry::instance().prefix = previousName;
}

} // namespace precice::profiling
