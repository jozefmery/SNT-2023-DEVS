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
#include <variant>
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
    bool self_service = true;
    // TODO
    bool checkout = false;
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
    Servers(const std::string& name, const size_t servers, const std::function<double()> gen_service_time,
            const std::function<std::optional<TimeT>()> gen_error)
        : name_{name}, gen_service_time_{gen_service_time}, gen_error_{gen_error},
          servers_{servers, Server{{}, 0.0, 0.0, 0.0}}, queue_{}, queue_occupancy_sum_{} {
        if (servers == 0) {
            throw std::runtime_error("Number of server set to 0");
        }
    }

  public: // friends
    friend std::ostream& operator<<(std::ostream& os, const Servers& state) {
        os << "| ";
        for (size_t i = 0; i < state.servers().size(); ++i) {
            const auto& server = state.servers()[i];
            if (server.busy()) {
                os << "busy: " << server.remaining;
            } else {
                os << "idle";
            }
            os << " | ";
        }
        return os << "Q: " << state.queue_size();
    }

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

    TimeT gen_error_time() { return gen_error_().value_or(0.0); }

    TimeT gen_service_time() { return gen_service_time_(); }

    void assign_customer_to_server(const Customer customer, const size_t server_idx, const TimeT service_time) {
        if (server_idx >= servers_.size()) {
            throw std::runtime_error("Invalid index in assign_customer_to_server");
        }
        auto& server = servers_[server_idx];
        const auto error_time = gen_error_time();
        // customer error handling is part of the "busy" phase
        // include the error time in the overall remaining time
        const auto remaining = service_time + error_time;
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

    void add_customer(const Customer customer, const TimeT service_time) {
        if (const auto server_idx = idle_server_idx()) {
            assign_customer_to_server(customer, *server_idx, service_time);
            return;
        }
        queue_.push(customer);
    }

    std::optional<Customer> next_customer() const {
        if (!has_waiting_customer()) {
            return std::nullopt;
        }
        return queue_.front();
    }

    void pop_customer() { queue_.pop(); }

    void advance_time(const TimeT delta) {
        for (auto& server : servers_) {
            if (server.busy()) {
                server.remaining -= delta;
            }
        }
        queue_occupancy_sum_ += delta * static_cast<TimeT>(queue_.size());
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

    double average_queue_size(const TimeT duration) const { return queue_occupancy_sum_ / duration; }

    const std::string& name() const { return name_; }

  private: // members
    std::string name_;
    std::function<double()> gen_service_time_;
    std::function<std::optional<TimeT>()> gen_error_;
    std::vector<Server> servers_;
    std::queue<Customer> queue_;
    TimeT queue_occupancy_sum_;
};

namespace CustomerCoordinator {
constexpr auto MODEL_NAME = "customer coordinator";
}

namespace ProductCounter {
constexpr auto MODEL_NAME = "product counter";
}

namespace SelfService {
constexpr auto MODEL_NAME = "self service";
}

namespace Checkout {
constexpr auto MODEL_NAME = "checkout";
}

namespace SelfCheckout {
constexpr auto MODEL_NAME = "self checkout";
}

namespace CustomerOutput {
constexpr auto MODEL_NAME = "customer output";
}

namespace CustomerCoordinator {

struct TargetedCustomer {
    Customer customer;
    std::string target;
};

enum class Queries { CHECKOUT_QUEUE_SIZES };

struct CheckoutQueueSizeResponse {
    std::string from;
    size_t queue_size;
};

using Message = std::variant<TargetedCustomer, Queries, CheckoutQueueSizeResponse>;

class State {
  public: // ctors, dtor
    State(const std::string name)
        : name_{name}, customers_{}, awaiting_responses_{false}, checkout_response_{}, self_checkout_response_{} {}

  public: // friends
    friend std::ostream& operator<<(std::ostream& os, const State& state) {
        return os << "customers: " << state.customer_count();
    }

  public: // methods
    bool has_customers() const { return !customers_.empty(); }

    const Customer* next_customer_ref() const {
        if (!has_customers()) {
            return nullptr;
        }

        return std::addressof(customers_.front());
    }

    bool next_customer_to_product_counter() const {
        const auto next = next_customer_ref();
        if (next == nullptr) {
            return false;
        }
        // product counter is the first, no need to check other properties
        return next->product_counter;
    }

    bool next_customer_to_self_service() const {
        const auto next = next_customer_ref();
        if (next == nullptr) {
            return false;
        }
        // self service is second
        return !next_customer_to_product_counter() && next->self_service;
    }

    bool next_customer_to_checkout() const {
        const auto next = next_customer_ref();
        if (next == nullptr) {
            return false;
        }
        // checkout is last
        return !next_customer_to_self_service() && next->checkout;
    }

    bool next_customer_should_exit() const { return !next_customer_to_checkout(); }

    bool should_send_checkout_query() const { return next_customer_to_checkout() && !awaiting_responses_; }

    bool awaiting_responses() const { return awaiting_responses_; }

    void receive_response_from_checkout(const CheckoutQueueSizeResponse response) {
        checkout_response_ = response;
        awaiting_responses_ = !self_checkout_response_received();
    }

    void receive_response_from_self_checkout(const CheckoutQueueSizeResponse response) {
        checkout_response_ = response;
        awaiting_responses_ = !checkout_response_received();
    }

    void await_responses() { awaiting_responses_ = true; }

    void clear_responses() {
        checkout_response_ = std::nullopt;
        self_checkout_response_ = std::nullopt;
    }

    const std::optional<CheckoutQueueSizeResponse>& checkout_response() const { return checkout_response_; }
    const std::optional<CheckoutQueueSizeResponse>& self_checkout_response() const { return self_checkout_response_; }

    bool checkout_response_received() const { return checkout_response() != std::nullopt; }

    bool self_checkout_response_received() const { return self_checkout_response() != std::nullopt; }

    bool responses_received() const { return checkout_response_received() && self_checkout_response_received(); }

    void add_customer(const Customer customer) { customers_.push(customer); }

    void pop_customer() { customers_.pop(); }

    size_t customer_count() const { return customers_.size(); }

    const std::string& name() const { return name_; }

  private: // members
    std::string name_;
    std::queue<Customer> customers_;
    bool awaiting_responses_;
    std::optional<CheckoutQueueSizeResponse> checkout_response_;
    std::optional<CheckoutQueueSizeResponse> self_checkout_response_;
};

void delta_external_add_customer(State& state, const TargetedCustomer& tc) {
    if (tc.target != state.name()) {
        throw std::runtime_error("Unexpected target " + tc.target + " in external delta of CustomerCoordinator");
    }
    state.add_customer(tc.customer);
}

void delta_external_receive_response(State& state, const CheckoutQueueSizeResponse& response) {
    if (!state.awaiting_responses()) {
        throw std::runtime_error(
            "Received CheckoutQueueSizeResponse in external delta of CustomerCoordinator when not awaiting");
    }
    if (response.from == Checkout::MODEL_NAME) {
        if (state.checkout_response_received()) {
            throw std::runtime_error(
                "Received response from checkout multiple times in external delta of CustomerCoordinator");
        }
        state.receive_response_from_checkout(response);
        return;
    }
    if (response.from == SelfCheckout::MODEL_NAME) {
        if (state.self_checkout_response_received()) {
            throw std::runtime_error(
                "Received response from self checkout multiple times in external delta of CustomerCoordinator");
        }
        state.receive_response_from_self_checkout(response);
        return;
    }

    throw std::runtime_error("Unexpected response from " + response.from + " in external delta of CustomerCoordinator");
}

State delta_external(const State& prev_state, const TimeT&, const Message& message) {

    if (std::holds_alternative<Queries>(message)) {
        throw std::runtime_error("Unexpected Query message in external delta of CustomerCoordinator");
    }

    State state = prev_state; // create copy
    if (const TargetedCustomer* tc = std::get_if<TargetedCustomer>(std::addressof(message))) {
        delta_external_add_customer(state, *tc);
        return state;
    }

    if (const CheckoutQueueSizeResponse* response = std::get_if<CheckoutQueueSizeResponse>(std::addressof(message))) {
        delta_external_receive_response(state, *response);
        return state;
    }

    throw std::runtime_error("Unexpected variant in external delta of CustomerCoordinator");
}

State delta_internal(const State& state_prev) {
    State state = state_prev;
    if (!state.has_customers()) {
        throw std::runtime_error("Unexpected internal delta in CustomerCoordinator when there are no customers");
    }

    if (state.awaiting_responses()) {
        throw std::runtime_error("Unexpected internal delta in CustomerCoordinator when awaiting responses");
    }

    if (state.should_send_checkout_query()) {
        state.await_responses();
        return state;
    }

    if (state.responses_received()) {
        state.clear_responses();
    }

    state.pop_customer();
    return state;
}

Message out_responses_received(const State& state) {
    if (!state.next_customer_to_checkout()) {
        throw std::runtime_error("Unexpected customer in out_responses_received of CustomerCoordinator");
    }
    // prefer checkout over self-checkout when even
    if (state.checkout_response()->queue_size <= state.self_checkout_response()->queue_size) {
        return TargetedCustomer{*state.next_customer_ref(), Checkout::MODEL_NAME};
    }
    return TargetedCustomer{*state.next_customer_ref(), SelfCheckout::MODEL_NAME};
}

Message out_target_customer(const State& state) {
    if (state.next_customer_to_checkout()) {
        throw std::runtime_error("Unexpected checkout customer in out_target_customer of CustomerCoordinator");
    }
    if (state.next_customer_to_product_counter()) {
        return TargetedCustomer{*state.next_customer_ref(), ProductCounter::MODEL_NAME};
    }
    if (state.next_customer_to_self_service()) {
        return TargetedCustomer{*state.next_customer_ref(), SelfService::MODEL_NAME};
    }
    if (state.next_customer_should_exit()) {
        return TargetedCustomer{*state.next_customer_ref(), CustomerOutput::MODEL_NAME};
    }
    throw std::runtime_error("Unexpected customer in out_target_customer of CustomerCoordinator");
}

Message out(const State& state_prev) {
    State state = state_prev;
    if (!state.has_customers()) {
        throw std::runtime_error("Unexpected output in CustomerCoordinator when there are no customers");
    }

    if (state.awaiting_responses()) {
        throw std::runtime_error("Unexpected output in CustomerCoordinator when awaiting responses");
    }

    if (state.should_send_checkout_query()) {
        return Queries::CHECKOUT_QUEUE_SIZES;
    }

    if (state.responses_received()) {
        return out_responses_received(state);
    }

    return out_target_customer(state);
}

TimeT ta(const State& state) {
    if (state.awaiting_responses()) {

        return Devs::Const::INF;
    }

    if (state.has_customers()) {
        return 0.0;
    }
    return Devs::Const::INF;
}

Atomic<Message, Message, State> create_model() {
    return Atomic<Message, Message, State>{State{MODEL_NAME}, delta_external, delta_internal, out, ta};
}
} // namespace CustomerCoordinator

namespace ProductCounter {
using State = Servers;

State delta_external(const State& state_prev, const TimeT& elapsed, const CustomerCoordinator::Message& message) {
    // create copy
    State state = state_prev;
    // advance time before potentially adding a new customer
    state.advance_time(elapsed);
    const CustomerCoordinator::TargetedCustomer* tc =
        std::get_if<CustomerCoordinator::TargetedCustomer>(std::addressof(message));
    if (tc != nullptr && tc->target == state.name()) {
        const auto customer = tc->customer;
        if (!customer.product_counter) {
            throw std::runtime_error("Unexpected customer in product counter");
        }
        state.add_customer(customer, state.gen_service_time());
    }
    // ignore other messages

    return state;
}

void delta_internal_finish_serving(State& state) {
    const auto finished_idx = state.next_ready_server_idx();
    if (finished_idx == std::nullopt) {
        throw std::runtime_error("Expected at least one busy server in ProductCounter during internal transition");
    }
    const auto delta = state.servers()[*finished_idx].remaining;
    // advance time before serving for correct queue occupancy
    state.advance_time(delta);
    state.finish_serving_customer(*finished_idx);
}
void delta_internal_next_customer(State& state) {
    // no need to check more than once as only one server may finish during an internal delta
    if (const auto customer = state.next_customer()) {
        state.pop_customer();
        const auto idle_idx = state.idle_server_idx();
        if (idle_idx == std::nullopt) {
            throw std::runtime_error("Expected at least one idle server in ProductCounter during internal transition");
        }
        state.assign_customer_to_server(*customer, *idle_idx, state.gen_service_time());
    }
}

State delta_internal(const State& state_prev) {
    State state = state_prev;

    if (state.idle()) {
        throw std::runtime_error("Internal delta in ProductCounter while idle");
    }

    delta_internal_finish_serving(state);
    delta_internal_next_customer(state);

    return state;
}

CustomerCoordinator::Message next_finished_customer(const State& state) {
    const auto customer_ptr = state.next_ready_customer_ref();
    if (customer_ptr == nullptr) {
        throw std::runtime_error("Expected at least one served customer in ProductCounter during output");
    }
    auto customer = *customer_ptr;
    customer.product_counter = false; // product counter served
    return CustomerCoordinator::TargetedCustomer{customer, CustomerCoordinator::MODEL_NAME};
}

CustomerCoordinator::Message out(const State& state) {
    if (state.idle()) {
        throw std::runtime_error("Out in ProductCounter while idle");
    }

    return next_finished_customer(state);
}

TimeT ta(const State& state) {
    if (state.idle()) {
        return Devs::Const::INF;
    }

    if (const auto remaining = state.remaining_to_next_ready()) {
        return *remaining;
    }

    throw std::runtime_error("Expected at least one busy server in ProductCounter during time advance");
}

Atomic<CustomerCoordinator::Message, CustomerCoordinator::Message, State>
create_model(const ProductCounterParameters& parameters) {
    return Atomic<CustomerCoordinator::Message, CustomerCoordinator::Message, State>{
        State{MODEL_NAME, parameters.servers, Devs::Random::exponential(parameters.service_rate),
              // no error in product counter
              []() { return std::nullopt; }},
        delta_external, delta_internal, out, ta};
}

} // namespace ProductCounter

namespace SelfService {

struct CustomerState {
  public: // members
    Customer customer;
    TimeT remaining;
};

class State {
  public: // ctors, dtor
    State(const std::string name, const SelfServiceParameters& parameters)
        : name_{name}, gen_service_time_{Devs::Random::exponential(parameters.service_rate)}, customers_{} {}

  public: // friends
    friend std::ostream& operator<<(std::ostream& os, const State& state) {
        return os << "customers: " << state.customer_count();
    }

  public: // methods
    const std::string& name() const { return name_; }

    size_t customer_count() const { return customers_.size(); }

    bool has_customer() const { return !customers_.empty(); }

    void add_customer(const Customer customer) { customers_.push_back({customer, gen_service_time_()}); }

    void pop_next_ready_customer() {
        if (const auto idx = next_ready_idx()) {
            customers_.erase(customers_.begin() + *idx);
        }
    }

    void advance_time(const TimeT delta) {
        for (auto& customer : customers_) {
            customer.remaining -= delta;
        }
    }

    std::optional<size_t> next_ready_idx() const {
        std::optional<size_t> min_idx{std::nullopt};
        TimeT min{Devs::Const::INF};

        for (auto customer = customers_.begin(); customer != customers_.end(); customer += 1) {
            if (customer->remaining < min) {
                min = customer->remaining;
                min_idx = std::distance(customers_.begin(), customer);
            }
        }
        return min_idx;
    }

    std::optional<TimeT> remaining_to_next_ready() const {
        const auto idx = next_ready_idx();
        if (idx == std::nullopt) {
            return std::nullopt;
        }

        return customers_[*idx].remaining;
    }

    void advance_time_to_next_ready() {
        if (has_customer()) {
            advance_time(*remaining_to_next_ready());
        }
    }

    const Customer* next_ready_customer_ref() const {
        const auto idx = next_ready_idx();
        if (idx == std::nullopt) {
            return nullptr;
        }

        return std::addressof(customers_[*idx].customer);
    }

  private: // members
    std::string name_;
    std::function<double()> gen_service_time_;
    std::vector<CustomerState> customers_;
};

State delta_external(const State& state_prev, const TimeT& elapsed, const CustomerCoordinator::Message& message) {
    // create copy
    State state = state_prev;
    // advance time before potentially adding a new customer
    state.advance_time(elapsed);
    const CustomerCoordinator::TargetedCustomer* tc =
        std::get_if<CustomerCoordinator::TargetedCustomer>(std::addressof(message));
    if (tc != nullptr && tc->target == state.name()) {
        const auto customer = tc->customer;
        if (!customer.self_service) {
            throw std::runtime_error("Unexpected customer in self service");
        }
        state.add_customer(customer);
    }
    // ignore other messages
    return state;
}

State delta_internal(const State& state_prev) {
    State state = state_prev;
    if (!state.has_customer()) {
        throw std::runtime_error("Unexpected internal delta in SelfService while empty");
    }
    state.advance_time_to_next_ready();
    state.pop_next_ready_customer();
    return state;
}

CustomerCoordinator::Message out(const State& state) {
    if (!state.has_customer()) {
        throw std::runtime_error("Unexpected output in SelfService while empty");
    }
    auto customer = *state.next_ready_customer_ref();
    customer.self_service = false; // self service done
    return CustomerCoordinator::TargetedCustomer{customer, CustomerCoordinator::MODEL_NAME};
}

TimeT ta(const State& state) {
    if (!state.has_customer()) {
        return Devs::Const::INF;
    }
    return *state.remaining_to_next_ready();
}

Atomic<CustomerCoordinator::Message, CustomerCoordinator::Message, State>
create_model(const SelfServiceParameters& parameters) {
    return Atomic<CustomerCoordinator::Message, CustomerCoordinator::Message, State>{
        State{MODEL_NAME, parameters}, delta_external, delta_internal, out, ta};
}
} // namespace SelfService

namespace Checkout {

std::function<std::optional<TimeT>()> error_generator(const double error_chance, const double error_handle_rate) {
    return [error_chance, gen_time = Devs::Random::exponential(error_handle_rate),
            rand = Devs::Random::uniform()]() -> std::optional<TimeT> {
        if (rand() < error_chance) {
            return gen_time();
        }
        return std::nullopt;
    };
}

class State : public Servers {
  public: // ctors, dtor
    State(const std::string name, const CheckoutParameters& parameters)
        : Servers{name, parameters.servers, Devs::Random::exponential(parameters.service_rate),
                  error_generator(parameters.error_chance, parameters.error_handle_rate)},
          sending_response_{false} {}

  public: // friends
    // TODO
    friend std::ostream& operator<<(std::ostream& os, const State&) { return os; }

  private: // members
    bool sending_response_;
};

State delta_external(const State& state, const TimeT&, const Customer&) {
    // TODO
    return state;
}

State delta_internal(const State& state) {
    // TODO
    return state;
}

Customer out(const State&) {
    // TODO
    return Customer{false, false};
}

TimeT ta(const State&) {
    // TODO
    return Devs::Const::INF;
}

Atomic<Customer, Customer, State> create_model(const Parameters& parameters) {
    return Atomic<Customer, Customer, State>{State{MODEL_NAME, parameters.checkout}, delta_external, delta_internal,
                                             out, ta};
}
} // namespace Checkout

namespace SelfCheckout {
// TODO
class State {
  public: // ctors, dtor
    State(const SelfCheckoutParameters&) {}

  public: // friends
    // TODO
    friend std::ostream& operator<<(std::ostream& os, const State&) { return os; }
};

State delta_external(const State& state, const TimeT&, const Customer&) {
    // TODO
    return state;
}

State delta_internal(const State& state) {
    // TODO
    return state;
}

Customer out(const State&) {
    // TODO
    return Customer{false, false};
}

TimeT ta(const State&) {
    // TODO
    return Devs::Const::INF;
}

Atomic<Customer, Customer, State> create_model(const Parameters& parameters) {
    return Atomic<Customer, Customer, State>{State{parameters.self_checkout}, delta_external, delta_internal, out, ta};
}
} // namespace SelfCheckout

namespace CustomerOutput {

class State {
  public: // ctors, dtor
    State(const std::string name) : name_{name}, customers_{} {}

  public: // friends
    friend std::ostream& operator<<(std::ostream& os, const State& state) {
        return os << "customers: " << state.customer_count();
    }

  public: // methods
    size_t customer_count() const { return customers_.size(); }

    bool has_customers() const { return !customers_.empty(); }

    void add_customer(const Customer customer) { customers_.push(customer); }

    void pop_customer() { customers_.pop(); }

    std::optional<Customer> next_customer() const {
        if (!has_customers()) {
            return std::nullopt;
        }
        return customers_.front();
    }

    const std::string& name() const { return name_; }

  private: // membersS
    std::string name_;
    std::queue<Customer> customers_;
};

State delta_external(const State& prev_state, const TimeT&, const CustomerCoordinator::Message& message) {
    State state = prev_state;
    const CustomerCoordinator::TargetedCustomer* tc =
        std::get_if<CustomerCoordinator::TargetedCustomer>(std::addressof(message));
    if (tc != nullptr && tc->target == state.name()) {
        state.add_customer(tc->customer);
    }
    // ignore other messages
    return state;
}

State delta_internal(const State& prev_state) {
    State state = prev_state;
    if (!state.has_customers()) {
        std::runtime_error("Unexpected internal transition in CustomerOutput when empty");
    }
    state.pop_customer();
    return state;
}

Customer out(const State& state) {
    if (const auto customer = state.next_customer()) {
        return *customer;
    }
    throw std::runtime_error("Unexpected output in CustomerOutput when empty");
}

TimeT ta(const State& state) {
    if (state.has_customers()) {
        return 0.0;
    }
    return Devs::Const::INF;
}

Atomic<CustomerCoordinator::Message, Customer, State> create_model() {
    return Atomic<CustomerCoordinator::Message, Customer, State>{State{MODEL_NAME}, delta_external, delta_internal, out,
                                                                 ta};
}
} // namespace CustomerOutput

std::unordered_map<std::string, Devs::Model::AbstractModelFactory<TimeT>> components(const Parameters& parameters) {
    return {{CustomerCoordinator::MODEL_NAME, CustomerCoordinator::create_model()},
            {ProductCounter::MODEL_NAME, ProductCounter::create_model(parameters.product_counter)},
            {CustomerOutput::MODEL_NAME, CustomerOutput::create_model()},
            {SelfService::MODEL_NAME, SelfService::create_model(parameters.self_service)}};
}

Devs::Dynamic customer_to_message(const Devs::Dynamic& customer) {
    return CustomerCoordinator::Message{
        CustomerCoordinator::TargetedCustomer{customer, CustomerCoordinator::MODEL_NAME}};
}

std::unordered_map<std::optional<std::string>, Devs::Model::Influencers> influencers() {
    // TODO
    return {
        {{}, {{CustomerOutput::MODEL_NAME, {}}}}, // setup output
        {CustomerOutput::MODEL_NAME, {{CustomerCoordinator::MODEL_NAME, {}}}},
        {ProductCounter::MODEL_NAME, {{CustomerCoordinator::MODEL_NAME, {}}}},
        {SelfService::MODEL_NAME, {{CustomerCoordinator::MODEL_NAME, {}}}},
        {CustomerCoordinator::MODEL_NAME,
         {{{}, customer_to_message}, // setup input
          {ProductCounter::MODEL_NAME, {}},
          {SelfService::MODEL_NAME, {}}}},
    };
}

Compound create_model(const Parameters& parameters) { return {components(parameters), influencers()}; }

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
        simulator.model().components()->at(ProductCounter::MODEL_NAME)->state()->value<ProductCounter::State>();
    std::cout << "Queue stats:\n";
    std::cout << "Product counter:\n";
    std::cout << "Servers:              " << product_counter_state.servers().size() << "\n";
    std::cout << "Average queue size:   " << product_counter_state.average_queue_size(duration) << "\n";
    std::cout << "Idle:                 " << (1 - product_counter_state.total_busy_ratio(duration)) * 100 << "\n";
    std::cout << "Busy:                 " << product_counter_state.total_busy_ratio(duration) * 100 << "\n";
    std::cout << "Error:                " << product_counter_state.total_error_ratio(duration) * 100 << "\n";
    std::cout << "Error/Busy:           " << product_counter_state.total_error_busy_ratio() * 100 << "\n";
    std::cout << "--------------------------------------\n";
    // TODO
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

void queue_simulation_small() {
    using namespace _impl::Queue;
    // simulation time window ;
    const TimeParameters time_params{0.0, 10 * Time::MINUTE};
    // queue parameters
    const auto parameters = Parameters{
        time_params,
        {time_params.normalize_rate(100 * time_params.duration_hours()), 0.5, 0.75},
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

    Simulator simulator{"shop queue system", create_model(parameters), time_params.start, time_params.end};
    setup_inputs_outputs(simulator, parameters);
    simulator.run();
    print_stats(simulator, time_params.duration());
}

void queue_simulation_large() {

    // TODO
    // using namespace _impl::Queue;
    // // simulation time window ;
    // const TimeParameters time_params{0.0, 10 * Time::MINUTE};
    // // queue parameters
    // const auto parameters = Parameters{
    //     time_params,
    //     {time_params.normalize_rate(100 * time_params.duration_hours()), 0.5, 0.5},
    //     {2, time_params.normalize_rate(50 * time_params.duration_hours())},
    //     {time_params.normalize_rate(100 * time_params.duration_hours())},
    //     {
    //         2,
    //         time_params.normalize_rate(20 * time_params.duration_hours()),
    //         0.05,
    //         time_params.normalize_rate(10 * time_params.duration_hours()),
    //     },
    //     {4, time_params.normalize_rate(12 * time_params.duration_hours()), 0.3,
    //      time_params.normalize_rate(30 * time_params.duration_hours()),
    //      time_params.normalize_rate(30 * time_params.duration_hours())},
    // };

    // Simulator simulator{
    //     "shop queue system", create_model(parameters), time_params.start, time_params.end, Time::EPS,
    //     Devs::Printer::Base<TimeT>::create()
    // };
    // setup_inputs_outputs(simulator, parameters);
    // simulator.run();
    // print_stats(simulator, time_params.duration());
}
} // namespace Examples