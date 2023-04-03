/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       31.03.2023
 */
//----------------------------------------------------------------------------------------------------------------------
#include <devs/examples.hpp>
#include <devs/lib.hpp>
#include <queue>
#include <set>
//----------------------------------------------------------------------------------------------------------------------

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

std::string input_to_str(const Input input) {
    switch (input) {
    case Input::POWER_OFF:
        return "Power OFF";
    case Input::POWER_ON:
        return "Power ON";
    case Input::POWER_TOGGLE:
        return "Power TOGGLE";
    case Input::MODE_NORMAL:
        return "Mode NORMAL";
    case Input::MODE_BLINK:
        return "Mode BLINK";
    case Input::MODE_TOGGLE:
        return "Mode TOGGLE";
    default:
        std::stringstream s;
        s << "Unhandled TrafficLight::Input enum class value in input_to_str: " << static_cast<int>(input);
        throw std::runtime_error(s.str());
    }
}

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
            throw std::runtime_error("Unexpected color in blink_mode_color_duration: " + color_to_str(*color));
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

Color invert_color_normal_mode(const Color color) {
    // RED/GREEN inverter
    if (color == Color::RED) {
        return Color::GREEN;
    }
    return Color::RED;
}

Color next_color_normal_mode(const State& s) {
    // whenever s.color is YELLOW, s.next_color is RED/GREEN (i.e.: the color currently transitioning to),
    // thus the next next_color should again be YELLOW
    if (s.color == Color::YELLOW) {
        return Color::YELLOW;
    }
    return invert_color_normal_mode(*s.color);
}

State delta_internal_normal_mode(const State& s) {
    return {s.mode, normal_mode_color_duration(*s.next_color), s.next_color, next_color_normal_mode(s)};
}

std::optional<Color> next_color_blink_mode(const State& s) {
    if (s.color == Color::YELLOW) {
        return Color::YELLOW;
    }
    return {};
}

State delta_internal_blink_mode(const State& s) {

    return {s.mode, blink_mode_color_duration(s.next_color), s.next_color, next_color_blink_mode(s)};
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
    if (!s.powered()) {
        throw std::runtime_error("Internal delta should not happen while not powered");
    }
    if (*s.mode == Mode::NORMAL) {

        if (!s.color) {
            throw std::runtime_error("Missing color in state during normal internal transition");
        }

        if (!s.next_color) {
            throw std::runtime_error("Missing next_color in state during normal internal transition");
        }

        return delta_internal_normal_mode(s);
    }

    return delta_internal_blink_mode(s);
}

Output out(const State& s) { return s.next_color; }

TimeT ta(const State& s) { return s.remaining; }

Atomic<TrafficLight::Input, TrafficLight::Output, TrafficLight::State> create_model() {

    return Atomic<TrafficLight::Input, TrafficLight::Output, TrafficLight::State>{
        TrafficLight::initial_normal_mode_state(), TrafficLight::delta_external, TrafficLight::delta_internal,
        TrafficLight::out, TrafficLight::ta};
}

void setup_inputs_outputs(Simulator& simulator, const TimeT& start_time, const TimeT& end_time) {
    const auto input_count = Devs::Random::poisson(20)();
    const auto rand_time = Devs::Random::uniform(start_time, end_time, {});
    const auto rand_input = Devs::Random::uniform_int(0, static_cast<int>(TrafficLight::Input::_ENUM_MEMBER_COUNT) - 1);

    for (int i = 0; i < input_count; ++i) {
        const auto input = static_cast<TrafficLight::Input>(rand_input());
        simulator.schedule_model_input(rand_time(), input, "Model input: " + TrafficLight::input_to_str(input));
    }

    simulator.add_model_output_listener([](const std::string&, const TimeT&, const Devs::Dynamic& value) {
        const auto color = value.value<TrafficLight::Output>();

        std::cout << "Traffic light output: ";
        if (color) {

            std::cout << "changed color to: " << TrafficLight::color_to_str(*color) << "\n";
            return;
        }
        std::cout << "turned off all lights"
                  << "\n";
    });
}
} // namespace TrafficLight

namespace Queue {

class Customer {
  public: // ctors, dtor
    explicit Customer(const bool age_verify, const bool extra_counter)
        : payment_{true}, age_verify_{age_verify}, extra_counter_{extra_counter} {}

  public: // static functions
    static Customer create_random(const double age_verify_chance = 0.5, const double extra_counter_chance = 0.5,
                                  std::function<double()> generator = Devs::Random::uniform()) {

        return Customer{generator() < age_verify_chance, generator() < extra_counter_chance};
    }

  private: // members
    bool payment_;
    bool age_verify_;
    bool extra_counter_;
};

namespace ExtraCounter {

class State {
  public: // ctors, dtor
    State() : queue_{} {}

  public: // methods
    bool has_waiting_customer() const { return !queue_.empty(); }

    void add_customer(const Customer customer) { queue_.push(customer); }

    Customer next_customer() {
        if (!has_waiting_customer()) {
            throw std::runtime_error("Counter has no waiting customers");
        }
        Customer customer = queue_.front();
        queue_.pop();
        return customer;
    }

  private: // members
    std::queue<Customer> queue_;
};

Atomic<Customer, Customer, State> create_model() {
    // TODO
    return Atomic<Customer, Customer, State>{State{}, [](const State& s, const TimeT&, const Customer&) { return s; },
                                             [](const State& s) { return s; },
                                             [](const State&) {
                                                 return Customer{false, false};
                                             },
                                             [](const State&) { return Devs::Const::INF; }};
}

} // namespace ExtraCounter

namespace Checkout {
// TODO
}

namespace SelfCheckout {
// TODO
}

Compound create_model() {

    // TODO
    return create_minimal_compound_model();
}

void setup_inputs_outputs(Simulator& simulator, const TimeT start_time, const TimeT end_time,
                          const double mean_expected_arrival_rate) {
    const auto sim_duration = end_time - start_time;
    // randomize arrival rate using the poisson distribution
    const auto actual_arrival_rate = Devs::Random::poisson(mean_expected_arrival_rate)(); // arrivals / per simulation
    const auto normalized_arrival_rate =
        static_cast<double>(actual_arrival_rate) / sim_duration; // arrivals / time unit (second in this case)
    const auto gen_arrival_interval = Devs::Random::exponential(normalized_arrival_rate);

    auto arrival_time{start_time + gen_arrival_interval()};

    for (int i = 0; i < actual_arrival_rate; ++i) {
        simulator.schedule_model_input(arrival_time, Queue::Customer::create_random(), "customer arrival");
        arrival_time += gen_arrival_interval();
    }

    simulator.add_model_output_listener([](const std::string&, const TimeT& time, const Devs::Dynamic&) {
        std::cout << "Customer left the system at " << time << "\n";
    });
}
} // namespace Queue

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
    constexpr auto start_time = 0.0;
    constexpr auto end_time = 100.0;
    Simulator simulator{"traffic light model", _impl::TrafficLight::create_model(), start_time, end_time};
    _impl::TrafficLight::setup_inputs_outputs(simulator, start_time, end_time);
    simulator.run();
}

void queue_simulation() {
    // time units
    constexpr auto SECOND = 1.0;
    constexpr auto MINUTE = 60.0 * SECOND;
    constexpr auto HOUR = 60.0 * MINUTE;
    // simulation time window
    constexpr auto start_time = 0.0;
    constexpr auto end_time = HOUR;
    // queue parameters
    constexpr auto mean_expected_arrival_rate = 10; // per simulation
    // TODO

    Simulator simulator{"shop queue system", _impl::Queue::create_model(), start_time, end_time};
    _impl::Queue::setup_inputs_outputs(simulator, start_time, end_time, mean_expected_arrival_rate);
    simulator.run();
}
} // namespace Examples