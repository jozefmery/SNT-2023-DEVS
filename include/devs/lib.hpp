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

template <typename X, typename Y, typename S, typename Time = double> struct Atomic {
    S s;
    std::function<S(const S&, const Time&, const X&)> delta_external;
    std::function<S(const S&)> delta_internal;
    std::function<Y(const S&)> out;
    std::function<Time(const S&)> ta;
};

template <typename X, typename Y, typename S, typename Time = double>
auto create_atomic(const S init_s, const std::function<S(const S&, const Time&, const X&)> delta_external,
                   const std::function<S(const S&)> delta_internal, const std::function<Y(const S&)> out,
                   const std::function<Time(const S&)> ta) {
    return Atomic<X, Y, S, Time>{init_s, delta_external, delta_internal, out, ta};
}

} // namespace Model

namespace Sim {

using Action = std::function<void()>;

template <typename Time> struct Event {
    Time time;
    Action action;
};

template <typename Time> auto event_to_string(const Event<Time>& event) {
    std::stringstream s;
    s << "Event{ time = " << event.time << " }";
    return s.str();
}

template <typename Time> struct CompareEvent {
    bool operator()(const Event<Time>& l, const Event<Time>& r) { return l.time < r.time; }
};

template <typename Time>
using Calendar = std::priority_queue<Event<Time>, std::vector<Event<Time>>, CompareEvent<Time>>;

template <typename Time> auto calendar_to_string(Calendar<Time> calendar) {
    // pass calendar as value as items are deleted when traversing

    if (calendar.empty()) {
        return std::string("{}");
    }

    std::stringstream s;
    s << "{ ";
    for (; !calendar.empty(); calendar.pop()) {

        const auto event = calendar.top();
        s << event_to_string(event);
        if (calendar.size() > 1) {
            s << ", ";
        }
    }
    s << " }";
    return s.str();
}

template <typename Time = double> struct Context {
    Time time{};
    Calendar<Time> calendar{CompareEvent<Time>{}};
};

template <typename Time = double> auto create_context() { return Context<Time>{}; }

template <typename Time> auto context_to_string(const Context<Time>& context) {
    std::stringstream s;
    s << "Context{ time = " << context.time << ", calendar = " << calendar_to_string(context.calendar) << " }";
    return s.str();
}

template <typename Time> auto advance_time(const Time to, Context<Time>& context) { context.time = to; }

template <typename Time> auto schedule_event(const Time at, const Action action, Context<Time>& context) {
    context.calendar.push({at, action});
}

template <typename X, typename Y, typename S, typename Time>
void simulate(Context<Time> context, Model::Atomic<X, Y, S, Time> model) {}
} // namespace Sim
} // namespace Devs
