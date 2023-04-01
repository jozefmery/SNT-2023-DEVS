/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       31.03.2023
 */

#include <devs/examples.hpp>
#include <devs/lib.hpp>

namespace Examples {

using TimeT = double;
using Simulator = Devs::Simulator<TimeT>;
template <typename X, typename Y, typename S> using Atomic = Devs::Model::Atomic<X, Y, S, TimeT>;
using Compound = Devs::Model::Compound<TimeT>;
using Devs::Null;

namespace _impl {

// showcase the creation of minimal models
Atomic<Null, Null, Null> create_minimal_atomic_model() {
    return Atomic<Null, Null, Null>{
        Null{},
        [](const Null& s, const TimeT&, const Null&) { return s; },
        [](const Null& s) { return s; },
        [](const Null&) { return Null{}; },
        [](const Null&) { return Devs::Const::INF; },
    };
}

Compound create_minimal_compound_model() {
    return Compound{
        {{"minimal atomic component", create_minimal_atomic_model()}}, // at least one component is required
        {},                                                            // provide no influencers
                                                                       // default to FIFO selector
    };
}

namespace TrafficLight {

enum class Color : int { GREEN, YELLOW, RED };
enum class Input : int { POWER_OFF, POWER_ON, POWER_TOGGLE, MODE_NORMAL, MODE_BLINK, MODE_TOGGLE, _ENUM_MEMBER_COUNT };
using Output = std::optional<Color>;

enum class Mode : int { NORMAL, BLINK };

std::string color_to_str(const Color& color) {
    switch (color) {
    case Color::GREEN:
        return "green";
    case Color::YELLOW:
        return "yellow";
    case Color::RED:
        return "red";
    default:
        std::stringstream s;
        s << "Unhandled TrafficLight::Color enum class value in color_to_str: " << static_cast<int>(color);
        throw std::runtime_error(s.str());
    }
}

std::string mode_to_str(const Mode& mode) {
    switch (mode) {
    case Mode::NORMAL:
        return "normal";
    case Mode::BLINK:
        return "blink";
    default:
        std::stringstream s;
        s << "Unhandled TrafficLight::Mode enum class value in mode_to_str: " << static_cast<int>(mode);
        throw std::runtime_error(s.str());
    }
}

struct State {
  public: // friends
    friend std::ostream& operator<<(std::ostream& os, const State& state) {
        os << std::boolalpha;
        const auto mode = state.powered() ? mode_to_str(*state.mode) : "{}";
        const auto color = state.color ? color_to_str(*state.color) : "{}";
        const auto next_color = state.next_color ? color_to_str(*state.next_color) : "{}";

        return os << "{ powered = " << state.powered() << ", mode = " << mode << ", remaining = " << state.remaining
                  << ", color = " << color << ", next_color = " << next_color << " }";
    }

  public: // methods
    // calculated state
    // the presence of a selected mode indicates the "power" status
    bool powered() const { return mode != std::nullopt; }

  public: // members
    std::optional<Mode> mode;
    TimeT remaining;
    std::optional<Color> color;
    std::optional<Color> next_color;
};

State identity_state(const State& s, const TimeT& elapsed) {
    return {s.mode, s.remaining - elapsed, s.color, s.next_color};
}

TimeT normal_mode_color_duration(const Color& color) {
    switch (color) {
    case Color::GREEN:
        return 13.0;
    case Color::YELLOW:
        return 1.0;
    case Color::RED:
        return 8.0;
    default:
        std::stringstream s;
        s << "Unhandled TrafficLight::Color enum class value in normal_mode_color_duration: "
          << static_cast<int>(color);
        throw std::runtime_error(s.str());
    }
}

TimeT blink_mode_color_duration(const std::optional<Color>& color) {
    if (color) {
        if (*color != Color::YELLOW) {
            throw std::runtime_error("Unexpected color in blink_mode_color_duration");
        }
        return 1.0;
    }
    return 1.0;
}

State initial_normal_mode_state() {
    const auto initial_color = Color::RED;
    return {Mode::NORMAL, normal_mode_color_duration(initial_color), initial_color, Color::YELLOW};
}

State initial_blink_mode_state() {
    const auto initial_color = Color::YELLOW;
    return {Mode::BLINK, blink_mode_color_duration(initial_color), initial_color, {}};
}

State handle_power_off(const State&, const TimeT&) { return {{}, Devs::Const::INF, {}, {}}; }

State handle_power_on(const State& s, const TimeT& elapsed) {
    if (s.powered()) {
        return identity_state(s, elapsed);
    }
    return initial_normal_mode_state();
}

State handle_power_toggle(const State& s, const TimeT& elapsed) {
    if (s.powered()) {
        return handle_power_off(s, elapsed);
    }
    return handle_power_on(s, elapsed);
}

State handle_mode_normal(const State& s, const TimeT& elapsed) {
    // mode switching does not work when powered off
    if (!s.powered()) {
        return handle_power_off(s, elapsed);
    }
    if (*s.mode == Mode::NORMAL) {
        return identity_state(s, elapsed);
    }
    return initial_normal_mode_state();
}

State handle_mode_blink(const State& s, const TimeT& elapsed) {
    // mode switching does not work when powered off
    if (!s.powered()) {
        return handle_power_off(s, elapsed);
    }
    if (*s.mode == Mode::BLINK) {
        return identity_state(s, elapsed);
    }
    return initial_blink_mode_state();
}

State handle_mode_toggle(const State& s, const TimeT& elapsed) {
    // mode switching does not work when powered off
    if (!s.powered()) {
        return handle_power_off(s, elapsed);
    }

    if (*s.mode == Mode::NORMAL) {
        return handle_mode_blink(s, elapsed);
    }
    return handle_mode_normal(s, elapsed);
}

std::unordered_map<Input, std::function<State(const State&, const TimeT&)>> messages_handlers() {

    return {{Input::POWER_OFF, handle_power_off},        {Input::POWER_ON, handle_power_on},
            {Input ::POWER_TOGGLE, handle_power_toggle}, {Input::MODE_NORMAL, handle_mode_normal},
            {Input::MODE_BLINK, handle_mode_blink},      {Input::MODE_TOGGLE, handle_mode_toggle}};
}

State delta_external(const State& s, const TimeT& elapsed, const Input& message) {
    static auto handlers = messages_handlers();
    const auto it = handlers.find(message);
    if (it == handlers.end()) {
        std::cerr << "WARNING: Ignoring unhandled message, code: " << static_cast<int>(message) << "\n";
        return s;
    }
    assert(elapsed <= ta(s));
    return it->second(s, elapsed);
}

State delta_internal(const State& s) {
    // TODO
    return s;
}

Output out(const State& s) { return s.color; }

TimeT ta(const State& s) { return s.remaining; }
} // namespace TrafficLight

Atomic<TrafficLight::Input, TrafficLight::Output, TrafficLight::State> create_traffic_light_model() {

    return Atomic<TrafficLight::Input, TrafficLight::Output, TrafficLight::State>{
        TrafficLight::initial_normal_mode_state(), TrafficLight::delta_external, TrafficLight::delta_internal,
        TrafficLight::out, TrafficLight::ta};
}

void schedule_random_traffic_light_inputs(Simulator&) {
    // TODO
}

Compound create_queue_model() {
    // TODO
    return create_minimal_compound_model();
}

void schedule_random_queue_inputs(Simulator&) {
    // TODO
}

} // namespace _impl

void minimal_atomic_simulation() {
    Simulator simulator{"minimal atomic model", _impl::create_minimal_atomic_model(), 0.0, 1.0};
    simulator.run();
}

void minimal_compound_simulation() {
    Simulator simulator{"minimal compound model", _impl::create_minimal_compound_model(), 0.0, 1.0};
    simulator.run();
}

void traffic_light_simulation() {
    Simulator simulator{"traffic light model", _impl::create_traffic_light_model(), 0.0, 100.0};
    _impl::schedule_random_traffic_light_inputs(simulator);
    simulator.run();
}

void queue_simulation() {
    // TODO ?
    Simulator simulator{"queue system", _impl::create_queue_model(), 0.0, 100.0};
    _impl::schedule_random_queue_inputs(simulator);
    simulator.run();
}
} // namespace Examples