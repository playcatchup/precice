#include "utils/Event.hpp"
#include "utils/EventUtils.hpp"
#include "utils/assertion.hpp"

namespace precice {
namespace utils {

Event::Event(const std::string &eventName, bool autostart)
    : _name(eventName)
{
  _name = EventRegistry::instance().prefix + _name;
  if (autostart) {
    start();
  }
}

Event::~Event()
{
  if (_state == State::PAUSED) {
    resume();
  }
  if (_state == State::RUNNING) {
    stop();
  }
}

void Event::start()
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::STOPPED, _name);
  _state = State::RUNNING;

  EventRegistry::instance().put('b', _name, timestamp);
}

void Event::pause()
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::RUNNING, _name);
  _state = State::PAUSED;

  EventRegistry::instance().put('p', _name, timestamp);
}

void Event::resume()
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::PAUSED, _name);
  _state = State::RUNNING;

  EventRegistry::instance().put('r', _name, timestamp);
}

void Event::stop()
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::RUNNING, _name);
  _state = State::STOPPED;

  EventRegistry::instance().put('e', _name, timestamp);
}

void Event::addData(const std::string &key, int value)
{
  auto timestamp = Clock::now();
  PRECICE_ASSERT(_state == State::RUNNING, _name);

  EventRegistry::instance().put('d', _name, timestamp, key, value);
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

} // namespace utils
} // namespace precice
