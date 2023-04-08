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

TimeT ta(const State& s) { return s.remaining; }

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
        simulator.model().external_input(rand_time(), input, "Model input: " + TrafficLight::input_to_str(input));
    }

    simulator.model().add_output_listener([](const std::string&, const TimeT&, const Devs::Dynamic& value) {
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

namespace Time {
constexpr auto SECOND = 1.0;
constexpr auto MINUTE = 60.0 * SECOND;
constexpr auto HOUR = 60.0 * MINUTE;
constexpr auto EPS = 0.001;
} // namespace Time

struct TimeParameters {
    TimeT duration() const {
        assert(end >= start);
        return end - start;
    }

    TimeT duration_seconds() const { return duration() / Time::SECOND; }
    TimeT duration_minutes() const { return duration_seconds() / 60.0; }
    TimeT duration_hours() const { return duration_minutes() / 60.0; }

    double normalize_rate(const double rate) const { return rate / duration(); }

  public: // members
    TimeT start;
    TimeT end;
};

struct CustomerParameters {
    double arrival_rate;
    double age_verify_chance;
    double product_counter_chance;
};

struct SelfServiceParameters {
  public: // members
    double service_rate;
};

struct ProductCounterParameters {
  public: // members
    size_t servers;
    double service_rate;
};

struct CheckoutParameters {
  public: // members
    size_t servers;
    double service_rate;
    double error_chance;
    double error_handle_rate;
};

struct SelfCheckoutParameters {
    // do not inherit from CheckoutParameters so that brace initializers work by default
  public: // members
    size_t servers;
    double service_rate;
    double error_chance;
    double error_handle_rate;
    double age_verify_rate;
};

struct Parameters {
  public: // members
    TimeParameters time;
    CustomerParameters customer;
    ProductCounterParameters product_counter;
    SelfServiceParameters self_service;
    CheckoutParameters checkout;
    SelfCheckoutParameters self_checkout;
};

class Customer {
  public: // static functions
    static Customer create_random(const double age_verify_chance, const double product_counter_chance,
                                  std::function<double()> generator = Devs::Random::uniform()) {

        return Customer{generator() < age_verify_chance, generator() < product_counter_chance};
    }

  public: // members
    bool age_verify;
    bool product_counter;
    bool payment = true;
};

struct Server {
  public: // methods
    bool idle() const { return current_customer == std::nullopt; }

    bool busy() const { return !idle(); }

  public: // members
    std::optional<Customer> current_customer;
    TimeT remaining;
    TimeT total_busy_time;
    TimeT total_error_time;
};

class Servers {
  public: // ctors, dtor
    Servers(const size_t servers, const std::function<double()> gen_service_time,
            const std::function<std::optional<TimeT>()> gen_error)
        : gen_service_time_{gen_service_time}, gen_error_{gen_error}, servers_{servers, Server{{}, 0.0, 0.0, 0.0}},
          queue_{} {}

  public: // methods
    bool has_waiting_customer() const { return !queue_.empty(); }

    bool all_servers_idle() const {
        for (const auto& server : servers_) {
            if (server.busy()) {
                return false;
            }
        }
        return true;
    }

    bool idle() const { return !has_waiting_customer() && all_servers_idle(); }

    std::optional<size_t> idle_server_idx() const {
        for (auto server = servers_.begin(); server != servers_.end(); server += 1) {
            if (server->idle()) {
                return std::distance(servers_.begin(), server);
            }
        }
        return std::nullopt;
    }

    std::optional<size_t> next_ready_server_idx() const {
        std::optional<size_t> min_idx{std::nullopt};
        TimeT min{Devs::Const::INF};

        for (auto server = servers_.begin(); server != servers_.end(); server += 1) {
            if (server->busy() && server->remaining < min) {
                min = server->remaining;
                min_idx = std::distance(servers_.begin(), server);
            }
        }
        return min_idx;
    }

    const Customer* next_ready_customer_ref() const {
        const auto idx = next_ready_server_idx();
        if (idx == std::nullopt) {
            return nullptr;
        }

        return std::addressof(*servers_[*idx].current_customer);
    }

    std::optional<TimeT> remaining_to_next_ready() const {
        const auto next = next_ready_server_idx();
        if (next) {

            return servers_[*next].remaining;
        }

        return next;
    }

    void assign_customer_to_server(const Customer customer, const size_t server_idx) {
        if (server_idx >= servers_.size()) {
            throw std::runtime_error("Invalid index in assign_customer_to_server");
        }
        auto& server = servers_[server_idx];
        const auto error_time = gen_error_().value_or(0.0);
        // customer error handling is part of the "busy" phase
        // include the error time in the overall remaining time
        const auto remaining = gen_service_time_() + error_time;
        server.current_customer = customer;
        server.remaining = remaining;
        server.total_busy_time += remaining;
        server.total_error_time += error_time;
    }

    void finish_serving_customer(const size_t server_idx) {
        if (server_idx >= servers_.size()) {
            throw std::runtime_error("Invalid index in finish_serving_customer");
        }
        auto& server = servers_[server_idx];
        server.current_customer = std::nullopt;
        server.remaining = 0.0;
    }

    void add_customer(const Customer customer) {
        if (const auto server_idx = idle_server_idx()) {
            assign_customer_to_server(customer, *server_idx);
            return;
        }
        queue_.push(customer);
    }

    std::optional<Customer> next_customer() {
        if (!has_waiting_customer()) {
            return std::nullopt;
        }
        Customer customer = queue_.front();
        queue_.pop();
        return customer;
    }

    void advance_time(const TimeT delta) {
        for (auto& server : servers_) {
            if (server.busy()) {
                server.remaining -= delta;
            }
        }
    }

    const std::vector<Server>& servers() const { return servers_; }

    size_t queue_size() const { return queue_.size(); }

    std::vector<double> server_busy_ratios(const TimeT duration) const {
        std::vector<double> ratios{};
        for (const auto& server : servers_) {
            ratios.push_back(server.total_busy_time / duration);
        }
        return ratios;
    }

    std::vector<double> server_error_ratios(const TimeT duration) const {
        std::vector<double> ratios{};
        for (const auto& server : servers_) {
            ratios.push_back(server.total_error_time / duration);
        }
        return ratios;
    }

    std::vector<double> server_error_to_busy_ratios() const {
        std::vector<double> ratios{};
        for (const auto& server : servers_) {
            ratios.push_back(server.total_error_time / server.total_busy_time);
        }
        return ratios;
    }

    double total_busy_ratio(const TimeT duration) const {
        const auto ratios = server_busy_ratios(duration);
        if (ratios.size() == 0) {
            return 0.0;
        }
        double sum{};
        for (const auto ratio : ratios) {
            sum += ratio;
        }
        return sum / ratios.size();
    }

    double total_error_ratio(const TimeT duration) const {
        const auto ratios = server_error_ratios(duration);
        if (ratios.size() == 0) {
            return 0.0;
        }
        double sum{};
        for (const auto ratio : ratios) {
            sum += ratio;
        }
        return sum / ratios.size();
    }

    double total_error_busy_ratio() const {
        const auto ratios = server_error_to_busy_ratios();
        if (ratios.size() == 0) {
            return 0.0;
        }
        double sum{};
        for (const auto ratio : ratios) {
            sum += ratio;
        }
        return sum / ratios.size();
    }

  private: // members
    std::function<double()> gen_service_time_;
    std::function<std::optional<TimeT>()> gen_error_;
    std::vector<Server> servers_;
    std::queue<Customer> queue_;
};

namespace ProductCounter {

class State : public Servers {
  public: // ctors, dtor
    State(const ProductCounterParameters& parameters)
        : Servers{parameters.servers, Devs::Random::exponential(parameters.service_rate),
                  // no error in product counter
                  []() { return std::nullopt; }},
          passthrough_{} {}

  public: // friends
    friend std::ostream& operator<<(std::ostream& os, const State& state) {
        for (size_t i = 0; i < state.servers().size(); ++i) {
            const auto& server = state.servers()[i];
            if (server.busy()) {
                os << "busy: " << server.remaining;
            } else {
                os << "idle";
            }
            os << " | ";
        }
        return os << "Q: " << state.queue_size() << " PQ: " << state.passthrough_queue_size();
    }

  public: // methods
    bool idle() const { return Servers::idle() && !has_passthrough_customer(); }

    void add_passthrough_customer(const Customer customer) { passthrough_.push(customer); }

    size_t passthrough_queue_size() const { return passthrough_.size(); }

    bool pop_passthrough_customer() {
        if (!has_passthrough_customer()) {
            return false;
        }
        passthrough_.pop();
        return true;
    }

    const Customer* next_passthrough_customer_ref() const {
        if (!has_passthrough_customer()) {
            return nullptr;
        }
        return std::addressof(passthrough_.front());
    }

    bool has_passthrough_customer() const { return !passthrough_.empty(); }

  private: // members
    std::queue<Customer> passthrough_;
};

State delta_external(const State& state_prev, const TimeT& elapsed, const Customer& customer) {
    // create copy
    State state = state_prev;
    state.advance_time(elapsed);
    if (customer.product_counter) {
        state.add_customer(customer);
    } else {
        state.add_passthrough_customer(customer);
    }

    return state;
}

void delta_internal_finish_serving(State& state) {
    const auto finished_idx = state.next_ready_server_idx();
    if (finished_idx == std::nullopt) {
        throw std::runtime_error("Expected at least one busy server in ProductCounter during internal transition");
    }
    const auto delta = state.servers()[*finished_idx].remaining;
    state.finish_serving_customer(*finished_idx);
    state.advance_time(delta);
}
void delta_internal_next_customer(State& state) {
    // no need to check more than once as only one server may finish during an internal delta
    if (const auto customer = state.next_customer()) {
        const auto idle_idx = state.idle_server_idx();
        if (idle_idx == std::nullopt) {
            throw std::runtime_error("Expected at least one idle server in ProductCounter during internal transition");
        }
        state.assign_customer_to_server(*customer, *idle_idx);
    }
}

State delta_internal(const State& state_prev) {
    State state = state_prev;

    if (state.idle()) {
        throw std::runtime_error("Internal delta in ProductCounter while idle");
    }

    if (state.pop_passthrough_customer()) {
        return state;
    }

    delta_internal_finish_serving(state);
    delta_internal_next_customer(state);

    return state;
}

Customer next_finished_customer(const State& state) {
    const auto customer_ptr = state.next_ready_customer_ref();
    if (customer_ptr == nullptr) {
        throw std::runtime_error("Expected at least one served customer in ProductCounter during output");
    }
    return *customer_ptr;
}

Customer out(const State& state) {
    if (state.idle()) {
        throw std::runtime_error("Out in ProductCounter while idle");
    }

    if (const auto passthrough = state.next_passthrough_customer_ref()) {
        return *passthrough;
    }

    return next_finished_customer(state);
}

TimeT ta(const State& state) {
    if (state.idle()) {
        return Devs::Const::INF;
    }

    if (state.has_passthrough_customer()) {
        return 0.0;
    }

    if (const auto remaining = state.remaining_to_next_ready()) {
        return *remaining;
    }

    throw std::runtime_error("Expected at least one busy server in ProductCounter during time advance");
}

Atomic<Customer, Customer, State> create_model(const Parameters& parameters) {
    return Atomic<Customer, Customer, State>{State{parameters.product_counter}, delta_external, delta_internal, out,
                                             ta};
}

} // namespace ProductCounter

namespace SelfService {
// TODO
class State {
  public: // ctors, dtor
    State(const SelfServiceParameters&) {}

  public: // friends
    // TODO
    friend std::ostream& operator<<(std::ostream& os, const State&) { return os; }
};
} // namespace SelfService

namespace Checkout {
// TODO
}

namespace SelfCheckout {
// TODO
}

Compound create_model(const Parameters& parameters) {

    // TODO
    return {{{"product counter", ProductCounter::create_model(parameters)}},
            {{"product counter", {{{}, {}}}}, {{}, {{"product counter", {}}}}}};
}

void setup_inputs_outputs(Simulator& simulator, const Parameters parameters) {

    const auto arrival_delay = Devs::Random::exponential(parameters.customer.arrival_rate);

    auto arrival_time{parameters.time.start + arrival_delay()};

    while (arrival_time <= parameters.time.end) {
        simulator.model().external_input(arrival_time,
                                         Queue::Customer::create_random(parameters.customer.age_verify_chance,
                                                                        parameters.customer.product_counter_chance),
                                         "customer arrival");
        arrival_time += arrival_delay();
    }

    simulator.model().add_output_listener([](const std::string&, const TimeT& time, const Devs::Dynamic&) {
        std::cout << "Customer left the system at " << time << "\n";
    });
}

void print_stats(Simulator& simulator, const TimeT duration) {
    const auto product_counter_state =
        simulator.model().components()->at("product counter")->state()->value<ProductCounter::State>();
    std::cout << "Queue stats: \n";
    std::cout << "Product counter: \n";
    std::cout << "Idle: " << (1 - product_counter_state.total_busy_ratio(duration)) * 100 << "\n";
    std::cout << "Busy: " << product_counter_state.total_busy_ratio(duration) * 100 << "\n";
    std::cout << "Error: " << product_counter_state.total_error_ratio(duration) * 100 << "\n";
    std::cout << "Error/Busy: " << product_counter_state.total_error_busy_ratio() * 100 << "\n";
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
    using namespace _impl::TrafficLight;
    constexpr auto start_time = 0.0;
    constexpr auto end_time = 100.0;
    Simulator simulator{"traffic light model", create_model(), start_time, end_time};
    setup_inputs_outputs(simulator, start_time, end_time);
    simulator.run();
}

void queue_simulation() {
    using namespace _impl::Queue;
    // simulation time window ;
    const TimeParameters time_params{0.0, 10 * Time::MINUTE};
    // queue parameters
    const auto parameters = Parameters{
        time_params,
        {time_params.normalize_rate(100 * time_params.duration_hours()), 0.5, 0.5},
        {2, time_params.normalize_rate(50 * time_params.duration_hours())},
        {time_params.normalize_rate(100 * time_params.duration_hours())},
        {
            2,
            time_params.normalize_rate(20 * time_params.duration_hours()),
            0.05,
            time_params.normalize_rate(10 * time_params.duration_hours()),
        },
        {4, time_params.normalize_rate(12 * time_params.duration_hours()), 0.3,
         time_params.normalize_rate(30 * time_params.duration_hours()),
         time_params.normalize_rate(30 * time_params.duration_hours())},
    };

    Simulator simulator{
        "shop queue system", create_model(parameters), time_params.start, time_params.end, 0.001,
        // TODO remove ?
        // Devs::Printer::Base<TimeT>::create()
    };
    setup_inputs_outputs(simulator, parameters);
    simulator.run();
    print_stats(simulator, time_params.duration());
}
} // namespace Examples