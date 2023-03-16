/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       11.03.2023
 */
#pragma once
// --- standard includes ---
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
// ---

namespace Devs {

// make sure infity works as expected
// https://stackoverflow.com/a/20016972/5150211
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE 754 required");
// shortcuts
constexpr auto fINF = std::numeric_limits<float>::infinity();
constexpr auto INF = std::numeric_limits<double>::infinity();

namespace Model {

template <typename X, typename S, typename Time = double>
using DeltaExternalFn = std::function<S(const S&, const Time&, const X&)>;

template <typename S> using DeltaInternalFn = std::function<S(const S&)>;

template <typename Y, typename S> using OutFn = std::function<Y(const S&)>;

template <typename S, typename Time = double> using TimeAdvanceFn = std::function<Time(const S&)>;

template <typename X, typename Y, typename S, typename Time = double> class Atomic {
    S s;
    DeltaExternalFn<X, S, Time> delta_external;
    DeltaInternalFn<S> delta_internal;
    OutFn<Y, S> out;
    TimeAdvanceFn<S, Time> ta;
};

} // namespace Model

namespace Sim {

template <typename Time = double> class Event {

  public: // aliases
    using Action = std::function<void()>;

  public: // ctors, dtor
    explicit Event(const Time time, const Action action)
        : time_{time}, action_{action}, cancelled_{std::make_shared<bool>(false)} {}

  public: // methods
    auto to_string() const {
        std::stringstream s;
        s << std::boolalpha; // write boolean as true/false
        s << "Event{ time = " << time_ << ", cancelled = " << *cancelled_ << " }";
        return s.str();
    }

    auto is_cancelled() const { return *cancelled_; }

    auto cancel() { *cancelled_ = true; }

    auto get_cancel_callback() const {
        auto copy = cancelled_;
        return [copy]() { *copy = true; };
    }

    auto time() const { return time_; }

    auto action() const { action_(); }

  private: // members
    Time time_;
    Action action_;
    std::shared_ptr<bool> cancelled_;
};

template <typename Time = double> class EventSorter {
  public:
    bool operator()(const Event<Time>& l, const Event<Time>& r) { return l.time() > r.time(); }
};

template <typename Time = double>
using CalendarBase = std::priority_queue<Event<Time>, std::vector<Event<Time>>, EventSorter<Time>>;

template <typename Time = double> class Calendar : public CalendarBase<Time> {

  public: // ctors, dtor
    explicit Calendar() : CalendarBase<Time>{EventSorter<Time>{}} {}

  public: // methods
    auto to_string() const {
        // pass calendar as value as items are deleted when traversing
        auto copy = *this;

        std::stringstream s;
        s << "|";
        for (; !copy.empty(); copy.pop()) {

            const auto event = copy.top();
            s << event.to_string();
            if (copy.size() > 1) {
                s << " | ";
            }
        }
        s << "|";
        return s.str();
    }

    std::optional<Event<Time>> next() {

        // ignore cancelled events
        while (!this->empty() && this->top().is_cancelled()) {
            this->pop();
        }

        if (this->empty()) {
            return {};
        }
        const auto event = this->top();
        this->pop();
        return event;
    }
};

template <typename Time = double> class Printer {

  public: // ctors, dtor
    explicit Printer(std::ostream& stream = std::cout) : s_{stream} {}

  public: // methods
    auto on_start(const Time time) { s_ << "[T = " << time << "] Starting simulation...\n"; }
    auto on_end(const Time time) { s_ << "[T = " << time << "] Finished simulation\n"; }
    auto on_time_advance(const Time prev, const Time next) {
        s_ << "[T = " << prev << "] Advancing time to " << next << "\n";
    }

    auto on_event_execution(const Event<Time>& event) {
        s_ << "[T = " << event.time() << "] Executing event " << event.to_string() << "\n";
    }

  private: // members
    std::ostream& s_;
};

template <typename Time = double> class Simulator {

  public: // ctors, dtor
    explicit Simulator(const Printer<Time> printer = Printer<Time>{}) : time_{}, calendar_{}, printer_{printer} {}

  public: // methods
    auto to_string() const {
        std::stringstream s;
        s << "Simulator{ time = " << time_ << ", calendar = " << calendar_.to_string() << " }";
        return s.str();
    }

    auto schedule_event(const Event<Time> event) { calendar_.push(event); }

    auto run() {

        printer_.on_start(time_);

        std::optional<Event<Time>> event{};

        while (event = calendar_.next()) {
            printer_.on_time_advance(time_, event->time());
            advance_time(event->time());
            printer_.on_event_execution(*event);
            event->action();
        }

        printer_.on_end(time_);
    }

  private: // methods
    auto advance_time(const Time to) { time_ = to; }

  private: // members
    Time time_;
    Calendar<Time> calendar_;
    Printer<Time> printer_;
};
} // namespace Sim
} // namespace Devs
