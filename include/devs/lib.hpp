/*
 *  Project:    SNT-DEVS-2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       10.04.2023
 */
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------
namespace Devs {
namespace Random {
using Engine = std::mt19937_64;

namespace _impl {
inline Engine seeded_engine(const std::optional<int> seed) { return Engine{seed ? *seed : std::random_device{}()}; }

template <typename Ret, typename Dist> std::function<Ret()> generator(const std::optional<int> seed, Dist dist) {
    return [engine = seeded_engine(seed), dist = std::move(dist)]() mutable { return dist(engine); };
}
} // namespace _impl

template <typename T = double>
std::function<T()> uniform(const T from = 0.0, const T to = 1.0, const std::optional<int> seed = {}) {
    return _impl::generator<T>(seed, std::uniform_real_distribution<T>{from, to});
}

template <typename T = int>
std::function<T()> uniform_int(const T from, const T to, const std::optional<int> seed = {}) {
    return _impl::generator<T>(seed, std::uniform_int_distribution<T>{from, to});
}

inline std::function<int()> poisson(const double mean, const std::optional<int> seed = {}) {
    return _impl::generator<int>(seed, std::poisson_distribution<>{mean});
}

inline std::function<double()> exponential(const double rate, const std::optional<int> seed = {}) {
    return _impl::generator<double>(seed, std::exponential_distribution<>{rate});
}

template <typename T = double> T rand() {
    static auto generator = uniform<T>();
    return generator();
}

} // namespace Random

namespace _impl {
// declarations
template <typename T> class Box;
template <typename Time> class Calendar;
template <typename Time> class IOModel;
template <typename X, typename Y, typename S, typename Time> class AtomicImpl;
template <typename Time> class CompoundImpl;

class IBox {
  public: // ctors, dtor
    // make class polymorphic
    virtual ~IBox() = default;

  public: // static functions
    template <typename T> static T& ref(IBox& box) { return (dynamic_cast<Box<T>&>(box)).ref(); }
    template <typename T> static T value(const IBox& box) { return (dynamic_cast<const Box<T>&>(box)).value(); }

  public: // methods
    virtual std::unique_ptr<IBox> copy() const = 0;
};

template <typename T> class Box : public IBox {
  public: // ctors, dtor
    Box(const T value) : value_{value} {}

  public: // static functions
    static std::unique_ptr<IBox> create(const T value) { return std::make_unique<Box<T>>(value); }

  public: // methods
    std::unique_ptr<IBox> copy() const override { return create(value_); }
    T& ref() { return value_; }
    T value() const { return value_; }

  private: // members
    T value_;
};
} // namespace _impl
//----------------------------------------------------------------------------------------------------------------------
struct Null {
    // empty type
  public: // friends
    // << required for state printing
    friend std::ostream& operator<<(std::ostream& os, const Null&) { return os << "{}"; }
};

class Dynamic {
  public: // ctors, dtor
          // implicit wrapping for any copyable type
    template <typename T> Dynamic(const T value) : p_box_{Devs::_impl::Box<T>::create(value)} {}
    // always create unique
    Dynamic(const Dynamic& other) : p_box_{other.p_box_->copy()} {}

  public: // methods
    template <typename T> T& ref() { return Devs::_impl::IBox::ref<T>(*p_box_); }
    template <typename T> T value() const { return Devs::_impl::IBox::value<T>(*p_box_); }
    template <typename T> operator T() const { return value<T>(); }

  private: // members
    std::unique_ptr<Devs::_impl::IBox> p_box_;
};
//----------------------------------------------------------------------------------------------------------------------
namespace Model {
template <typename Time>
using AbstractModelFactory =
    std::function<std::unique_ptr<Devs::_impl::IOModel<Time>>(const std::string, Devs::_impl::Calendar<Time>*)>;

template <typename X, typename Y, typename S, typename Time = double> struct Atomic {

  public: // methods
    operator AbstractModelFactory<Time>() {
        const auto copy = *this;
        return [copy](const std::string name, Devs::_impl::Calendar<Time>* p_calendar) {
            return std::make_unique<Devs::_impl::AtomicImpl<X, Y, S, Time>>(name, copy, p_calendar);
        };
    }

  public: // members
    S s;
    std::function<S(S, const Time&, const X&)> delta_external;
    std::function<S(S)> delta_internal;
    std::function<Y(const S&)> out;
    std::function<Time(const S&)> ta;
};

using Transformer = std::optional<std::function<Dynamic(const Dynamic&)>>;
using Influencers = std::unordered_map<std::optional<std::string>, Transformer>;

template <typename Time = double> struct Compound {
  public: // static functions
    static std::string fifo_selector(const std::vector<std::string>& names) {
        // guaranteed to have at least two names
        assert(names.size() >= 2);
        return names[0];
    }

  public: // methods
    operator AbstractModelFactory<Time>() {
        const auto copy = *this;
        return [copy](const std::string name, Devs::_impl::Calendar<Time>* p_calendar) {
            return std::make_unique<Devs::_impl::CompoundImpl<Time>>(name, copy, p_calendar);
        };
    }

  public: // members
    std::unordered_map<std::string, AbstractModelFactory<Time>> components;
    std::unordered_map<std::optional<std::string>, Influencers> influencers;
    std::function<std::string(const std::vector<std::string>&)> select = fifo_selector;
};
} // namespace Model
//----------------------------------------------------------------------------------------------------------------------
namespace _impl {
// aliases
using Action = std::function<void()>;
template <typename... Args> using Listener = std::function<void(Args...)>;
template <typename... Args> using Listeners = std::vector<Listener<Args...>>;

template <typename Time> class Event {

  public: // ctors, dtor
    explicit Event(const Time time, const Action action, const std::string model, const std::string description = "")
        : time_{time}, action_{action}, model_{model}, description_{description},
          cancelled_{std::make_shared<bool>(false)} {}

  public: // methods
    std::string to_string(const bool with_description = true, const bool with_cancelled = false) const {
        std::stringstream s;
        s << "Event{ time = " << time();

        s << ", model = " << model();

        if (with_description) {
            s << ", description = " << description();
        }

        if (with_cancelled) {
            s << std::boolalpha; // write boolean as true/false
            s << ", cancelled = " << is_cancelled();
        }
        s << " }";
        return s.str();
    }

    const std::string& description() const { return description_; }

    bool is_cancelled() const { return *cancelled_; }

    std::function<void()> get_cancel_callback() const {
        // allow cancelling "remotely", as there is no sensible way to traverse a std::priority_queue
        // use a shared pointer for proper cancelling even if the object is moved around
        auto copy = cancelled_;
        return [copy]() { *copy = true; };
    }

    const Time& time() const { return time_; }

    void action() const { action_(); }

    const std::string& model() const { return model_; }

  private: // members
    Time time_;
    Action action_;
    std::string model_;
    std::string description_;
    std::shared_ptr<bool> cancelled_;
};

template <typename Time> class EventSorter {
  public:
    // a sooner event should get priority, FIFO otherwise
    bool operator()(const Event<Time>& l, const Event<Time>& r) { return l.time() > r.time(); }
};

template <typename... Args> void invoke_listeners(const Listeners<Args...> listeners, Args... args) {
    for (const auto listener : listeners) {
        listener(args...);
    }
}

template <typename Time>
using CalendarBase = std::priority_queue<Event<Time>, std::vector<Event<Time>>, EventSorter<Time>>;

template <typename Time> class Calendar : private CalendarBase<Time> {

  public: // ctors, dtor
    explicit Calendar(const Time start_time, const Time end_time, const Time epsilon)
        : CalendarBase<Time>{EventSorter<Time>{}}, time_{start_time}, end_time_{end_time}, epsilon_{epsilon},
          time_advanced_listeners_{}, event_scheduled_listeners_{}, executing_event_action_listeners_{} {}

  public: // methods
    const Time& time() const { return time_; }
    const Time& end_time() const { return end_time_; }

    std::string to_string() const {
        // create queue copy as items are deleted when traversing
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

    void schedule_event(const Event<Time> event) {
        if (event.time() < time()) {
            std::stringstream s;
            s << "Attempted to schedule an event (" << event.to_string() << ") in the past (current time: " << time()
              << ")";
            throw std::runtime_error(s.str());
        }

        // using this-> is required for accessing base class methods in these cases
        this->push(event);
        invoke_listeners<const Time&, const Event<Time>&>(event_scheduled_listeners_, time(), event);
    }

    // returns whether an event has actually been executed
    bool execute_next(const std::function<std::string(const std::vector<std::string>&)> select) {

        const auto events = next();

        if (events.empty()) {
            return false;
        }

        const auto time = events[0].time();

        if (time > end_time_) {
            // always finish at the ending time
            advance_time(end_time_);
            return false;
        }

        advance_time(time);
        execute_concurrent_events(std::move(events), select);
        return true;
    }

    void add_time_advanced_listener(const Listener<const Time&, const Time&> listener) {
        time_advanced_listeners_.push_back(listener);
    }

    void add_event_scheduled_listener(const Listener<const Time&, const Event<Time>&> listener) {
        event_scheduled_listeners_.push_back(listener);
    }

    void add_executing_event_action_listener(const Listener<const Time&, const Event<Time>&> listener) {
        executing_event_action_listeners_.push_back(listener);
    }

  private: // static functions
    static size_t select_index(const std::vector<std::string>& names,
                               const std::function<std::string(const std::vector<std::string>&)>& select) {
        const auto name = select(names);
        const auto name_it = std::find(names.begin(), names.end(), name);
        if (name_it == names.end()) {
            throw std::runtime_error(std::string("Invalid model name returned by select: ") + name);
        }
        return std::distance(names.begin(), name_it);
    }

  private: // methods
    void pop_cancelled_events() {
        while (!this->empty() && this->top().is_cancelled()) {
            this->pop();
        }
    }

    const Event<Time>* next_pending_event_ref() {
        pop_cancelled_events();
        if (this->empty()) {
            return nullptr;
        }
        return std::addressof(this->top());
    }

    std::optional<Event<Time>> next_pending_event() {
        const auto next = next_pending_event_ref();
        if (next == nullptr) {
            return {};
        }
        // create copy from reference before deleting
        const Event<Time> event = *next;
        this->pop();
        return event;
    }

    bool has_next_pending_event() { return next_pending_event_ref() != nullptr; }

    bool is_next_pending_event_concurrent(const Time& time) {
        return has_next_pending_event() && std::abs(next_pending_event_ref()->time() - time) <= epsilon_;
    }

    std::optional<Event<Time>> next_pending_concurrent_event(const Time& time) {
        if (!is_next_pending_event_concurrent(time)) {
            return {};
        }
        return next_pending_event();
    }

    std::vector<Event<Time>> next() {
        const auto event = next_pending_event();
        if (!event) {
            return {};
        }
        std::vector<Event<Time>> concurrent_events{*event};
        while (const auto concurrent_event = next_pending_concurrent_event(event->time())) {
            concurrent_events.push_back(*concurrent_event);
        }

        return concurrent_events;
    }

    void execute_concurrent_events(std::vector<Event<Time>> events,
                                   const std::function<std::string(const std::vector<std::string>&)> select) {

        // map events to model names
        std::vector<std::string> names;
        for (const auto event : events) {
            names.push_back(event.model());
        }

        while (!events.empty()) {
            const auto idx{names.size() > 1 ? select_index(names, select) : 0};
            const auto& event{events[idx]};
            // check if other concurrent events did not cancel this event
            if (!event.is_cancelled()) {
                execute_event_action(event);
                // push possible newly created events
                while (const auto new_concurrent = next_pending_concurrent_event(event.time())) {
                    events.push_back(*new_concurrent);
                    names.push_back(new_concurrent->model());
                }
            }
            events.erase(events.begin() + idx);
            names.erase(names.begin() + idx);
        }
    }

    void execute_event_action(const Event<Time>& event) {
        invoke_listeners<const Time&, const Event<Time>&>(executing_event_action_listeners_, time(), event);
        event.action();
    }

    void advance_time(const Time& time) {
        if (std::abs(time - time_) > epsilon_) {
            invoke_listeners<const Time&, const Time&>(time_advanced_listeners_, time_, time);
            time_ = time;
        }
    }

  private: // members
    Time time_;
    Time end_time_;
    Time epsilon_;
    Listeners<const Time&, const Time&> time_advanced_listeners_;
    Listeners<const Time&, const Event<Time>&> event_scheduled_listeners_;
    Listeners<const Time&, const Event<Time>&> executing_event_action_listeners_;
};

template <typename Time> class IOModel {

  public: // ctors, dtor
    explicit IOModel(const std::string name, Calendar<Time>* p_calendar)
        : state_transition_listeners_{}, name_{name}, p_calendar_{p_calendar}, input_listeners_{}, output_listeners_{} {
        if (name.empty()) {
            throw std::runtime_error("Model name should not be empty");
        }
    }

    virtual ~IOModel() = default;

  public: // methods
    const std::string& name() const { return name_; }

    virtual const std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>>* components() const = 0;
    virtual std::optional<Dynamic> state() const = 0;
    virtual const std::function<std::string(const std::vector<std::string>&)> select() const = 0;
    virtual void add_state_transition_listener(
        const Listener<const std::string&, const Time&, const std::string&, const std::string&> listener) = 0;

    virtual void sim_started(const Listener<const std::string&, const Time&, const std::string&> listener) const = 0;
    virtual void sim_ended(const Listener<const std::string&, const Time&, const std::string&> listener) const = 0;

    void input_from_influencer(const std::string& from, const Time& time, const Dynamic& value,
                               const Devs::Model::Transformer& transformer) const {
        if (from == name()) {
            throw std::runtime_error("Model " + name() + " contains a forbidden self-influence loop");
        }
        schedule_event(Event<Time>{time,
                                   [this, from, value, transformer]() {
                                       invoke_input_listeners(from, influencer_transform(from, value, transformer));
                                   },
                                   name(), "influencer input"});
    }

    // directly invoked input
    // useful for setting up compound input listeners
    void direct_input(const std::string& from, const Dynamic& value,
                      const Devs::Model::Transformer& transformer) const {
        invoke_input_listeners(from, influencer_transform(from, value, transformer));
    }

    void external_input(const Time& time, const Dynamic& value, const std::string& description) const {
        schedule_event(
            Event<Time>{time, [this, value]() { invoke_input_listeners(name(), value); }, name(), description});
    }

    void add_output_listener(const Listener<const std::string&, const Time&, const Dynamic&> listener) {
        output_listeners_.push_back(listener);
    }

  protected: // methods
    void schedule_event(const Event<Time> event) const { p_calendar_->schedule_event(event); }

    const Time& calendar_time() const { return p_calendar_->time(); }

    void output(const Dynamic& value) const {
        // using output events is redundant, invoke directly
        // when necessary, the listener sets up input events
        invoke_output_listeners(value);
    }

    void state_transitioned(const std::string& prev, const std::string& next) const {
        if (prev != next) {
            invoke_listeners<const std::string&, const Time&, const std::string&, const std::string&>(
                state_transition_listeners_, name(), calendar_time(), prev, next);
        }
    }

    void add_input_listener(const Listener<const std::string&, const Dynamic&> listener) {
        input_listeners_.push_back(listener);
    }

    Dynamic influencer_transform(const std::string& influencer, const Dynamic& value,
                                 const std::optional<std::function<Dynamic(const Dynamic&)>> transformer) const {
        try {
            if (transformer) {
                return (*transformer)(value);
            }
            return value;
        } catch (const std::bad_cast&) {
            std::stringstream s{};
            s << "Invalid dynamic cast in transformer function for influencer " << influencer << " in model " << name();
            throw std::runtime_error(s.str());
        }
    }

    void invoke_input_listeners(const std::string& from, const Dynamic& value) const {
        try {
            invoke_listeners<const std::string&, const Dynamic&>(input_listeners_, from, value);
        } catch (std::bad_cast&) {
            throw std::runtime_error("Invalid type cast in input listener of model " + name());
        }
    }

    void invoke_output_listeners(const Dynamic& value) const {
        try {
            invoke_listeners<const std::string&, const Time&, const Dynamic&>(output_listeners_, name(),
                                                                              calendar_time(), value);
        } catch (std::bad_cast&) {
            throw std::runtime_error("Invalid type cast in output listener of model " + name());
        }
    }

  protected: // members
    Listeners<const std::string&, const Time&, const std::string&, const std::string&> state_transition_listeners_;

  private: // members
    std::string name_;
    Calendar<Time>* p_calendar_;
    Listeners<const std::string&, const Dynamic&> input_listeners_;
    Listeners<const std::string&, const Time&, const Dynamic&> output_listeners_;
};

template <typename X, typename Y, typename S, typename Time> class AtomicImpl : public IOModel<Time> {

  public: // ctors, dtor
    explicit AtomicImpl(const std::string name, const Devs::Model::Atomic<X, Y, S, Time> model,
                        Calendar<Time>* p_calendar)
        : IOModel<Time>{name, p_calendar}, model_{model}, last_transition_time_{}, cancel_internal_transition_{} {

        this->add_input_listener(
            [this](const std::string& from, const Dynamic& input) { dynamic_input_listener(from, input); });
        schedule_internal_transition();
    }

  private: // static functions
    static std::string state_to_str(const S& state) {
        std::stringstream s;
        s << state;
        return s.str();
    }

  private: // methods
    const std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>>* components() const override {
        return nullptr;
    };

    std::optional<Dynamic> state() const override { return model_.s; }

    const S& atomic_state() const { return model_.s; }

    void sim_started(const Listener<const std::string&, const Time&, const std::string&> listener) const override {
        listener(this->name(), this->calendar_time(), state_to_str(atomic_state()));
    }

    void sim_ended(const Listener<const std::string&, const Time&, const std::string&> listener) const override {
        listener(this->name(), this->calendar_time(), state_to_str(atomic_state()));
    }

    const std::function<std::string(const std::vector<std::string>&)> select() const override {
        // use fifo selector in an atomic simulation
        return Devs::Model::Compound<Time>::fifo_selector;
    }

    void add_state_transition_listener(
        const Listener<const std::string&, const Time&, const std::string&, const std::string&> listener) override {
        this->state_transition_listeners_.push_back(listener);
    }

    void transition_state(const S new_state) {
        this->state_transitioned(state_to_str(atomic_state()), state_to_str(new_state));
        model_.s = new_state;
        update_last_transition_time();
    }

    Time time_advance() const { return model_.ta(atomic_state()); }

    Time internal_transition_time() const { return this->calendar_time() + time_advance(); }

    Y internal_transition() {
        // get output from current state
        const auto out = model_.out(atomic_state());
        const auto new_state = model_.delta_internal(atomic_state());
        transition_state(new_state);
        return out;
    }

    void external_transition(const Time& elapsed, const X& input) {
        const auto new_state = model_.delta_external(atomic_state(), elapsed, input);
        transition_state(new_state);
    }

    void update_last_transition_time() { last_transition_time_ = this->calendar_time(); }

    std::function<void()> get_internal_transition_action() {
        return [this]() {
            const auto out = internal_transition();
            this->output(out);
            schedule_internal_transition();
        };
    }

    void schedule_internal_transition() {
        const auto event = Devs::_impl::Event<Time>{internal_transition_time(), get_internal_transition_action(),
                                                    this->name(), "internal transition"};
        cancel_internal_transition_ = event.get_cancel_callback();
        this->schedule_event(event);
    }

    void dynamic_input_listener(const std::string& from, const Dynamic& input) {
        try {
            input_listener(input);
        } catch (const std::bad_cast&) {
            std::stringstream s;
            s << "The output type of model " << from << " is not compatible with the input type of model "
              << this->name();
            throw std::runtime_error(s.str());
        }
    }

    void input_listener(const X& input) {
        if (cancel_internal_transition_) {
            (*cancel_internal_transition_)();
        }

        external_transition(elapsed_since_last_transition(), input);
        schedule_internal_transition();
    }

    Time elapsed_since_last_transition() { return this->calendar_time() - last_transition_time_; }

  private: // members
    Devs::Model::Atomic<X, Y, S, Time> model_;
    Time last_transition_time_;
    std::optional<std::function<void()>> cancel_internal_transition_;
};

template <typename Time> class CompoundImpl : public IOModel<Time> {

  public: // ctors, dtor
    explicit CompoundImpl(const std::string name, const Devs::Model::Compound<Time> model, Calendar<Time>* p_calendar)
        : IOModel<Time>{name, p_calendar}, select_{model.select},
          components_{factories_to_components(model.components, p_calendar)} {
        connect_components(model.influencers);
    }

  public: // methods
    const std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>>* components() const override {
        return std::addressof(components_);
    }

    std::optional<Dynamic> state() const override { return {}; };

    const std::function<std::string(const std::vector<std::string>&)> select() const override { return select_; }

    void add_state_transition_listener(
        const Listener<const std::string&, const Time&, const std::string&, const std::string&> listener) override {
        for (auto& [_, component] : components_) {
            component->add_state_transition_listener(listener);
        }
    }

  private: // methods
    void sim_started(const Listener<const std::string&, const Time&, const std::string&> listener) const override {
        for (auto& [_, component] : components_) {
            component->sim_started(listener);
        }
    }
    void sim_ended(const Listener<const std::string&, const Time&, const std::string&> listener) const override {
        for (auto& [_, component] : components_) {
            component->sim_ended(listener);
        }
    }

    IOModel<Time>* model_ref(const std::string& name) {
        auto it = components_.find(name);
        if (it == components_.end()) {
            return nullptr;
        }

        return it->second.get();
    }
    void connect_component_output_listener(const std::string& name,
                                           const Listener<const std::string&, const Time&, const Dynamic&> listener) {
        const auto p_model = model_ref(name);
        if (p_model == nullptr) {
            std::stringstream s;
            s << "Connecting to non-existing component: " << name;
            throw std::runtime_error(s.str());
        }
        p_model->add_output_listener([listener](const std::string& from, const Time& time, const Dynamic& value) {
            listener(from, time, value);
        });
    }

    void connect_compound_output_influencers(const Devs::Model::Influencers& influencers) {
        for (const auto& [name, transformer] : influencers) {
            if (name == std::nullopt) {
                throw std::runtime_error("Compound model " + this->name() + " cannot influence itself");
            }
            connect_component_output_listener(
                *name, [this, transformer](const std::string& from, const Time&, const Dynamic& value) {
                    this->output(this->influencer_transform(from, value, transformer));
                });
        }
    }

    void connect_component_to_compound_input(const IOModel<Time>* p_component,
                                             const Devs::Model::Transformer& transformer) {
        this->add_input_listener([this, p_component, transformer](const std::string&, const Dynamic& value) {
            p_component->direct_input(this->name(), value, transformer);
        });
    }

    void connect_component_influencers(const std::string& component_name, const Devs::Model::Influencers& influencers) {
        const auto p_component = model_ref(component_name);
        if (p_component == nullptr) {
            std::stringstream s;
            s << "Defining influencers for non-existing component: " << component_name;
            throw std::runtime_error(s.str());
        }

        for (const auto& [influencer, transformer] : influencers) {
            if (influencer == std::nullopt) {
                connect_component_to_compound_input(p_component, transformer);
                continue;
            }

            if (component_name == *influencer) {
                throw std::runtime_error("Component " + component_name + " contains a forbidden self-influence loop");
            }

            connect_component_output_listener(
                *influencer,
                [p_component, transformer](const std::string& from, const Time& time, const Dynamic& value) {
                    p_component->input_from_influencer(from, time, value, transformer);
                });
        }
    }

    void connect_components(
        const std::unordered_map<std::optional<std::string>, Devs::Model::Influencers>& model_influencers) {
        for (const auto& [component, influencers] : model_influencers) {
            if (component) {
                connect_component_influencers(*component, influencers);
                continue;
            }
            connect_compound_output_influencers(influencers);
        }
    }

    std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>>
    factories_to_components(const std::unordered_map<std::string, Devs::Model::AbstractModelFactory<Time>>& factories,
                            Calendar<Time>* p_calendar) {

        if (factories.empty()) {
            throw std::runtime_error("Compound model " + this->name() + " has no components");
        }

        std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>> components{};
        for (const auto& [name, factory] : factories) {
            if (name == this->name()) {
                throw std::runtime_error("Component and compound model name collision: " + name);
            }
            components[name] = factory(name, p_calendar);
        }

        return components;
    }

  private: // member
    std::function<std::string(const std::vector<std::string>&)> select_;
    std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>> components_;
};

} // namespace _impl
//----------------------------------------------------------------------------------------------------------------------
namespace Const {

// make sure infinity/negative infinity works as expected
// see: https://stackoverflow.com/a/20016972/5150211
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE 754 required");
// shortcuts
constexpr float fINF = std::numeric_limits<float>::infinity();
constexpr double INF = std::numeric_limits<double>::infinity();
} // namespace Const
//----------------------------------------------------------------------------------------------------------------------
namespace Printer {

// this enum is a limited selection, full list here:
// https://en.wikipedia.org/wiki/ANSI_escape_code#SGR_(Select_Graphic_Rendition)_parameters
enum class TextDecoration : int {
    NONE = 0,
    FONT_BOLD = 1,
    FONT_LIGHT = 2,
    ITALIC = 3,
    UNDERLINE = 4,
    STRIKE = 9,
    FG_BLACK = 30,
    FG_RED = 31,
    FG_GREEN = 32,
    FG_YELLOW = 33,
    FG_BLUE = 34,
    FG_MAGENTA = 35,
    FG_CYAN = 36,
    FG_WHITE = 37,
    FG_BRIGHT_BLACK = 90,
    FG_BRIGHT_RED = 91,
    FG_BRIGHT_GREEN = 92,
    FG_BRIGHT_YELLOW = 93,
    FG_BRIGHT_BLUE = 94,
    FG_BRIGHT_MAGENTA = 95,
    FG_BRIGHT_CYAN = 96,
    FG_BRIGHT_WHITE = 97,
    //
    BG_BLACK = 40,
    BG_RED = 41,
    BG_GREEN = 42,
    BG_YELLOW = 43,
    BG_BLUE = 44,
    BG_MAGENTA = 45,
    BG_CYAN = 46,
    BG_WHITE = 47,
    BG_BRIGHT_BLACK = 100,
    BG_BRIGHT_RED = 101,
    BG_BRIGHT_GREEN = 102,
    BG_BRIGHT_YELLOW = 103,
    BG_BRIGHT_BLUE = 104,
    BG_BRIGHT_MAGENTA = 105,
    BG_BRIGHT_CYAN = 106,
    BG_BRIGHT_WHITE = 107
};

constexpr auto START_STYLE = "\033[";
constexpr auto END_STYLE = "\033[m";

using Decorations = std::vector<TextDecoration>;

inline std::ostream& operator<<(std::ostream& os, const std::vector<TextDecoration>& decorations) {
    os << START_STYLE;
    for (size_t i = 0; i < decorations.size(); ++i) {

        os << std::to_string(static_cast<int>(decorations[i]));
        if (i < decorations.size() - 1) {
            os << ";";
        }
    }
    return os << "m";
}

template <typename Time, typename Step = std::uint64_t> class Base {

  public: // ctors, dtor
    explicit Base(std::ostream& stream = std::cout) : s_{stream} {}

    // polymorphic classes should always have a virtual destructor
    virtual ~Base() = default;

  public: // static functions
    static std::unique_ptr<Base<Time, Step>> create(std::ostream& stream = std::cout) {
        return std::make_unique<Base<Time, Step>>(stream);
    }

  public: // methods
    // calendar/events
    virtual void on_time_advanced(const Time&, const Time&) {}
    virtual void on_event_scheduled(const Time&, const Devs::_impl::Event<Time>&) {}
    virtual void on_executing_event_action(const Time&, const Devs::_impl::Event<Time>&) {}
    // model
    virtual void on_model_state_transition(const std::string&, const Time&, const std::string&, const std::string&) {}
    virtual void on_sim_start(const std::string&, const Time&, const std::string&) {}
    virtual void on_sim_step(const Time&, const Step&) {}
    virtual void on_sim_end(const std::string&, const Time&, const std::string&) {}

  protected: // members
    std::ostream& s_;
};

template <typename Time, typename Step = std::uint64_t> class PlainVerbose : public Base<Time, Step> {

  public: // ctors, dtor
    explicit PlainVerbose(std::ostream& stream = std::cout) : Base<Time, Step>{stream} {}

  public: // static functions
    static std::unique_ptr<PlainVerbose<Time, Step>> create(std::ostream& stream = std::cout) {
        return std::make_unique<PlainVerbose<Time, Step>>(stream);
    }

  public: // methods
    // calendar/event
    void on_time_advanced(const Time& prev, const Time& next) override {
        this->s_ << prefix(prev) << "Time: " << format_time(prev) << " -> " << format_time(next) << "\n";
    }
    void on_event_scheduled(const Time& time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << prefix(time) << "Event scheduled: " << event.to_string() << "\n";
    }
    void on_executing_event_action(const Time& time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << prefix(time) << "Executing event action: " << event.to_string() << "\n";
    }
    // model
    void on_model_state_transition(const std::string& name, const Time& time, const std::string& prev,
                                   const std::string& next) override {
        this->s_ << prefix(time) << "Model " << name << " state: " << prev << " -> " << next << "\n";
    }

    void on_sim_start(const std::string& name, const Time& time, const std::string& state) override {
        this->s_ << prefix(time) << "Model " << name << " initial state: " << state << "\n";
    }
    void on_sim_step(const Time& time, const Step& step) override {
        this->s_ << prefix(time) << "Step " << std::to_string(step)
                 << " -------------------------------------------------------------"

                 << "\n";
    }
    void on_sim_end(const std::string& name, const Time& time, const std::string& state) override {
        this->s_ << prefix(time) << "Model " << name << " ending state: " << state << "\n";
    }

  protected: // methods
    std::string prefix(const Time& time) {
        std::stringstream s;
        s << "[T = " << format_time(time) << "] ";
        return s.str();
    }

    std::string format_time(const Time& time) {
        std::stringstream s;
        s << std::fixed << std::setprecision(1);
        s << time;
        return s.str();
    }
};

template <typename Time, typename Step = std::uint64_t> class ColoredVerbose : public PlainVerbose<Time, Step> {

  public: // ctors, dtor
    explicit ColoredVerbose(std::ostream& stream = std::cout) : PlainVerbose<Time, Step>{stream} {}

  public: // static functions
    static std::unique_ptr<ColoredVerbose<Time, Step>> create(std::ostream& stream = std::cout) {
        return std::make_unique<ColoredVerbose<Time, Step>>(stream);
    }

  public: // methods
    // calendar/event
    void on_time_advanced(const Time& prev, const Time& next) override {
        this->s_ << prefix(prev);
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << "Time: " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_RED, TextDecoration::STRIKE};
        this->s_ << this->format_time(prev) << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << " -> " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_GREEN};
        this->s_ << this->format_time(next) << END_STYLE << "\n";
    }
    void on_event_scheduled(const Time& time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << prefix(time);
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << "Event scheduled: " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_CYAN};
        this->s_ << event.to_string() << END_STYLE << "\n";
    }
    void on_executing_event_action(const Time& time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << prefix(time);
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << "Executing event action: " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_CYAN};
        this->s_ << event.to_string() << END_STYLE << "\n";
    }
    // model
    void on_model_state_transition(const std::string& name, const Time& time, const std::string& prev,
                                   const std::string& next) override {
        this->s_ << prefix(time);
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << "Model " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_GREEN};
        this->s_ << name << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << " state: " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_RED, TextDecoration::STRIKE};
        this->s_ << prev << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << " -> " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_GREEN};
        this->s_ << next << END_STYLE << "\n";
    }

    void on_sim_start(const std::string& name, const Time& time, const std::string& state) override {
        this->s_ << prefix(time);
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << "Model " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_GREEN};
        this->s_ << name << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << " initial state: " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_GREEN};
        this->s_ << state << END_STYLE << "\n";
    }

    void on_sim_step(const Time& time, const Step& step) override {
        this->s_ << prefix(time);
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_MAGENTA};
        this->s_ << "Step " << step << " -------------------------------------------------------------\n" << END_STYLE;
    }

    void on_sim_end(const std::string& name, const Time& time, const std::string& state) override {
        this->s_ << prefix(time);
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << "Model " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_GREEN};
        this->s_ << name << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_WHITE};
        this->s_ << " ending state: " << END_STYLE;
        this->s_ << Decorations{TextDecoration::FONT_BOLD, TextDecoration::FG_BRIGHT_GREEN};
        this->s_ << state << END_STYLE << "\n";
    }

  protected: // methods
    std::string prefix(const Time& time) {
        std::stringstream s;
        s << Decorations{TextDecoration::FG_WHITE, TextDecoration::FONT_BOLD};
        s << "[" << END_STYLE;
        s << Decorations{TextDecoration::FG_YELLOW, TextDecoration::FONT_BOLD};
        s << "T = " << this->format_time(time) << END_STYLE;
        s << Decorations{TextDecoration::FG_WHITE, TextDecoration::FONT_BOLD};
        s << "] " << END_STYLE;
        return s.str();
    }
};
} // namespace Printer
//----------------------------------------------------------------------------------------------------------------------
template <typename Time = double, typename Step = std::uint64_t> class Simulator {
  public: // ctors, dtor
    explicit Simulator(
        const std::string model_name, const Devs::Model::AbstractModelFactory<Time> model, const Time start_time,
        const Time end_time, const Time& time_epsilon = 0.001,
        std::unique_ptr<Printer::Base<Time, Step>> printer = Printer::ColoredVerbose<Time, Step>::create())
        : p_calendar_{std::make_unique<Devs::_impl::Calendar<Time>>(start_time, end_time, time_epsilon)},
          p_printer_{std::move(printer)} {
        setup_calendar_listeners();
        p_model_ = model(model_name, p_calendar_.get());
        setup_model_listeners();
    }

  public: // methods
    _impl::IOModel<Time>& model() { return *p_model_; }

    void sim_started() const {

        p_model_->sim_started([&](const std::string& name, const Time& time, const std::string& state) {
            p_printer_->on_sim_start(name, time, state);
        });
    }

    void sim_ended() {

        p_model_->sim_ended([&](const std::string& name, const Time& time, const std::string& state) {
            p_printer_->on_sim_end(name, time, state);
        });
    }

    void run() {
        Step step{1};
        sim_started();
        while (p_calendar_->execute_next(p_model_->select())) {
            p_printer_->on_sim_step(p_calendar_->time(), step);
            ++step;
        }
        sim_ended();
    }

  private: // methods
    void setup_calendar_listeners() {
        p_calendar_->add_time_advanced_listener(
            [this](const Time& prev, const Time& next) { p_printer_->on_time_advanced(prev, next); });
        p_calendar_->add_event_scheduled_listener([this](const Time& time, const Devs::_impl::Event<Time>& event) {
            p_printer_->on_event_scheduled(time, event);
        });
        p_calendar_->add_executing_event_action_listener(
            [this](const Time& time, const Devs::_impl::Event<Time>& event) {
                p_printer_->on_executing_event_action(time, event);
            });
    }

    void setup_model_listeners() {
        p_model_->add_state_transition_listener(
            [this](const std::string& name, const Time& time, const std::string& prev, const std::string& next) {
                p_printer_->on_model_state_transition(name, time, prev, next);
            });
    }

  private: // members
    std::unique_ptr<Devs::_impl::Calendar<Time>> p_calendar_;
    std::unique_ptr<Devs::Printer::Base<Time, Step>> p_printer_;
    std::unique_ptr<Devs::_impl::IOModel<Time>> p_model_;
};
//----------------------------------------------------------------------------------------------------------------------
} // namespace Devs
