/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       11.03.2023
 */
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------
namespace Devs {

namespace _impl {
// declarations
template <typename T> class Box;
template <typename Time> class Calendar;
template <typename Time> class IOModel;
template <typename Time>
using AbstractModelFactory =
    std::function<std::unique_ptr<Devs::_impl::IOModel<Time>>(const std::string, Devs::_impl::Calendar<Time>*)>;
template <typename X, typename Y, typename S, typename Time> class AtomicImpl;
template <typename Time> class CompoundImpl;

class IBox {

  public: // ctors, dtor
    // make class polymorphic
    virtual ~IBox() = default;

  public: // static functions
    template <typename T> static T get(const IBox& box) { return (dynamic_cast<const Box<T>&>(box)).value(); }
};

template <typename T> class Box : public IBox {

  public: // ctors, dtor
    Box(const T value) : value_{value} {}

  public: // static functions
    static std::shared_ptr<IBox> create(const T value) { return std::make_shared<Box<T>>(value); }

  public: // methods
    T value() const { return value_; }

  private: // members
    T value_;
};
} // namespace _impl
//----------------------------------------------------------------------------------------------------------------------

class Dynamic {
  public: // ctors, dtor
    template <typename T> Dynamic(const T value) : p_box_{Devs::_impl::Box<T>::create(value)} {}

  public: // methods
    template <typename T> operator T() const { return get<T>(); }
    template <typename T> T get() const { return Devs::_impl::IBox::get<T>(*p_box_); }

  private: // members
    std::shared_ptr<Devs::_impl::IBox> p_box_;
};
//----------------------------------------------------------------------------------------------------------------------
namespace Model {
template <typename X, typename Y, typename S, typename Time = double> struct Atomic {

  public: // methods
    operator Devs::_impl::AbstractModelFactory<Time>() {
        const auto copy = *this;
        return [copy](const std::string name, Devs::_impl::Calendar<Time>* p_calendar) {
            return std::make_unique<Devs::_impl::AtomicImpl<X, Y, S, Time>>(name, copy, p_calendar);
        };
    }

  public: // members
    S s;
    std::function<S(const S&, const Time&, const X&)> delta_external;
    std::function<S(const S&)> delta_internal;
    std::function<Y(const S&)> out;
    std::function<Time(const S&)> ta;
};

using Transformer = std::optional<std::function<Dynamic(const Dynamic&)>>;
using Influencers = std::unordered_map<std::string, Transformer>;

template <typename Time> struct Compound {
  public: // static functions
    static std::string fifo_selector(const std::vector<std::string>& names) { return names[0]; }

  public: // methods
    operator Devs::_impl::AbstractModelFactory<Time>() {
        const auto copy = *this;
        return [copy](const std::string name, Devs::_impl::Calendar<Time>* p_calendar) {
            return std::make_unique<Devs::_impl::CompoundImpl<Time>>(name, copy, p_calendar);
        };
    }

  public: // members
    std::unordered_map<std::string, Devs::_impl::AbstractModelFactory<Time>> components;
    std::unordered_map<std::string, Influencers> influencers;
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
    explicit Calendar(const Time start_time, const Time end_time)
        : CalendarBase<Time>{EventSorter<Time>{}}, time_{start_time}, end_time_{end_time}, time_advanced_listeners_{},
          event_scheduled_listeners_{}, event_action_executed_listeners_{} {}

  public: // methods
    const Time& time() const { return time_; }

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

        // using this-> is required for accesing base class methods in these cases
        this->push(event);
        invoke_listeners<const Time&, const Event<Time>&>(event_scheduled_listeners_, time(), event);
    }

    // returns whether an event has actually been executed
    bool execute_next(const std::function<std::string(const std::vector<std::string>&)> select,
                      const Time& time_epsilon) {

        const auto events = next(time_epsilon);

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

    void add_event_action_executed_listener(const Listener<const Time&, const Event<Time>&> listener) {
        event_action_executed_listeners_.push_back(listener);
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
        if (!next) {
            return {};
        }
        // create copy from reference before deleting
        const Event<Time> event = *next;
        this->pop();
        return event;
    }

    bool has_next_pending_event() { return next_pending_event_ref() != nullptr; }

    bool is_next_pending_event_concurrent(const Time& time, const Time& time_epsilon) {
        return has_next_pending_event() && std::abs(next_pending_event_ref()->time() - time) <= time_epsilon;
    }

    std::optional<Event<Time>> next_pending_concurrent_event(const Time& time, const Time& time_epsilon) {
        if (!is_next_pending_event_concurrent(time, time_epsilon)) {
            return {};
        }
        return next_pending_event();
    }

    std::vector<Event<Time>> next(const Time& time_epsilon) {
        const auto event = next_pending_event();
        if (!event) {
            return {};
        }
        std::vector<Event<Time>> concurrent_events{*event};
        while (const auto concurrent_event = next_pending_concurrent_event(event->time(), time_epsilon)) {
            concurrent_events.push_back(*concurrent_event);
        }

        return concurrent_events;
    }

    void execute_concurrent_events(std::vector<Event<Time>> events,
                                   const std::function<std::string(const std::vector<std::string>&)> select) {
        // empty handled in execute_next
        if (events.size() == 1) {
            execute_event_action(events[0]);
            return;
        }
        std::vector<std::string> names;
        for (const auto event : events) {
            names.push_back(event.model());
        }

        while (events.size() > 1) {
            const auto name = select(names);
            const auto name_it = std::find(names.begin(), names.end(), name);
            if (name_it == names.end()) {
                throw std::runtime_error(std::string("Invalid model name returned by select: ") + name);
            }
            const auto idx = std::distance(names.begin(), name_it);
            execute_event_action(events[idx]);
            events.erase(events.begin() + idx);
            names.erase(name_it);
        }

        execute_event_action(events[0]);
    }

    void execute_event_action(const Event<Time>& event) {
        event.action();
        invoke_listeners<const Time&, const Event<Time>&>(event_action_executed_listeners_, time(), event);
    }

    void advance_time(const Time& time) {
        invoke_listeners<const Time&, const Time&>(time_advanced_listeners_, time_, time);
        time_ = time;
    }

  private: // members
    Time time_;
    Time end_time_;
    Listeners<const Time&, const Time&> time_advanced_listeners_;
    Listeners<const Time&, const Event<Time>&> event_scheduled_listeners_;
    Listeners<const Time&, const Event<Time>&> event_action_executed_listeners_;
};

template <typename Time> class IOModel {

  public: // ctors, dtor
    explicit IOModel(const std::string name, Calendar<Time>* p_calendar)
        : name_{name}, p_calendar_{p_calendar}, input_listeners_{}, output_listeners_{}, state_transition_listeners_{} {
    }

  public: // methods
    const std::string& name() const { return name_; }

    void external_input(const Time& time, const Dynamic& value) const {
        schedule_event(Event<Time>{
            time,
            [this, &value]() { invoke_listeners<const std::string&, const Dynamic&>(input_listeners_, name(), value); },
            name(), "external input"});
    }

    void add_output_listener(const Listener<const std::string&, const Time&, const Dynamic&> listener) {
        output_listeners_.push_back(listener);
    }

    void add_state_transition_listener(
        const Listener<const std::string&, const Time&, const std::string&, const std::string&> listener) {
        state_transition_listeners_.push_back(listener);
    }

  protected: // methods
    void schedule_event(const Event<Time> event) const { p_calendar_->schedule_event(event); }

    void connect_influencer(IOModel<Time>& influencer,
                            const std::optional<std::function<Dynamic(const Dynamic&)>> maybe_transformer) const {
        if (influencer.name() == name()) {
            throw std::runtime_error("Model " + name() + " contains a forbidden self-influence loop");
        }
        const auto transformer = maybe_transformer.value_or([](const Dynamic& value) { return value; });
        influencer.add_output_listener(
            [this, transformer](const std::string& from, const Time& time, const Dynamic& value) {
                input_from_model(from, time, influencer_transform(value, from, transformer));
            });
    }

    void input_from_model(const std::string& from, const Time& time, const Dynamic& value) const {
        if (from == name()) {
            throw std::runtime_error("Model " + name() + " contains a forbidden self-influence loop");
        }
        schedule_event(Event<Time>{time,
                                   [this, from, &value]() {
                                       invoke_listeners<const std::string&, const Dynamic&>(input_listeners_, from,
                                                                                            value);
                                   },
                                   name(), "input"});
    }

    const Time& calendar_time() const { return p_calendar_->time(); }

    void output(const Dynamic& value) const {
        // using output events is redundant, invoke directly
        invoke_listeners<const std::string&, const Time&, const Dynamic&>(output_listeners_, name(), calendar_time(),
                                                                          value);
    }

    void state_transitioned(const std::string& prev, const std::string& next) {
        invoke_listeners<const std::string&, const Time&, const std::string&, const std::string&>(
            state_transition_listeners_, name(), calendar_time(), prev, next);
    }

    void add_input_listener(const Listener<const std::string&, const Dynamic&> listener) {
        input_listeners_.push_back(listener);
    }

  private: // methods
    Dynamic influencer_transform(const Dynamic& value, const std::string& influencer,
                                 const std::function<Dynamic(const Dynamic&)> transformer) {
        try {
            return transformer(value);
        } catch (const std::bad_cast&) {
            std::stringstream s{};
            s << "Invalid dynamic cast in transformer function for influencer " << influencer << " in model " << name();
            throw std::runtime_error(s.str());
        }
    }

  private: // members
    std::string name_;
    Calendar<Time>* p_calendar_;
    Listeners<const std::string&, const Dynamic&> input_listeners_;
    Listeners<const std::string&, const Time&, const Dynamic&> output_listeners_;
    Listeners<const std::string&, const Time&, const std::string&, const std::string&> state_transition_listeners_;
};

template <typename X, typename Y, typename S, typename Time> class AtomicImpl : public IOModel<Time> {

  public: // ctors, dtor
    explicit AtomicImpl(const std::string name, const Devs::Model::Atomic<X, Y, S, Time> model,
                        Calendar<Time>* p_calendar)
        : IOModel<Time>{name, p_calendar}, model_{model}, last_transition_time_{}, cancel_internal_transition_{} {
        if (name.empty()) {
            throw std::runtime_error("Component name should not be empty");
        }

        this->add_input_listener(
            [this](const std::string& from, const Dynamic& input) { dynamic_input_listener(from, input); });
        schedule_internal_transition();
    }

  private: // static functions
    std::string state_to_str(const S& state) {
        std::stringstream s;
        s << state;
        return s.str();
    }

  private: // methods
    const S& state() const { return model_.s; }

    void transition_state(const S state) {
        this->state_transitioned(state_to_str(model_.s), state_to_str(state));
        model_.s = state;
        update_last_transition_time();
    }

    Time time_advance() const { return model_.ta(state()); }

    Y internal_transition() {
        // get output from current state
        const auto out = model_.out(state());
        const auto new_state = model_.delta_internal(state());
        transition_state(new_state);
        return out;
    }

    void external_transition(const Time& elapsed, const X& input) {
        const auto new_state = model_.delta_external(state(), elapsed, input);
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
        const auto event = Devs::_impl::Event<Time>{time_advance(), get_internal_transition_action(), this->name(),
                                                    "internal transition"};
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

template <typename Time = double> class CompoundImpl : public IOModel<Time> {

  public: // ctors, dtor
    explicit CompoundImpl(const std::string name, const Devs::Model::Compound<Time> model, Calendar<Time>* p_calendar)
        : IOModel<Time>{name, p_calendar}, select_{model.select},
          components_{factories_to_components(model.components, p_calendar)} {
        connect_components(model.influencers);
    }

    // TODO
    // template <typename X, typename Y, typename S>
    // explicit CompoundImpl(const std::string name, const Devs::Model::Atomic<X, Y, S, Time> model,
    //                       Calendar<Time>* p_calendar)
    //     : IOModel<Time>{name, p_calendar}, model_{model}, components_{factories_to_components(p_calendar)} {}

  public: // methods
    const std::function<std::string(const std::vector<std::string>&)> select() const { return select_; }

    const std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>>& components() const { return components_; }

  private: // methods
    // IOModel<Time>& model_ref(const std::string& name) {
    //     // TODO
    //     auto it = components_.find(name);
    //     if (it == components_.end()) {
    //         std::stringstream s;
    //         s << "Model " << this->name() << " influenced by non-existing model: " << name;
    //         throw std::runtime_error(s.str());
    //     }

    //     return it->second;
    // }

    void connect_components(const std::unordered_map<std::string, Devs::Model::Influencers>& influencers) {
        // TODO
    }

    std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>>
    factories_to_components(const std::unordered_map<std::string, Devs::_impl::AbstractModelFactory<Time>>& factories,
                            Calendar<Time>* p_calendar) {

        if (factories.empty()) {
            throw std::runtime_error("Compound model " + this->name() + " has no components");
        }

        std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>> components{};
        for (const auto& [name, factory] : factories) {
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

// make sure infity/negative infinity works as expected
// https://stackoverflow.com/a/20016972/5150211
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE 754 required");
// shortcuts
constexpr float fINF = std::numeric_limits<float>::infinity();
constexpr double INF = std::numeric_limits<double>::infinity();
} // namespace Const
//----------------------------------------------------------------------------------------------------------------------
namespace Printer {
template <typename Time = double> class Base {

  public: // ctors, dtor
    explicit Base(std::ostream& stream = std::cout) : s_{stream} {}

    // polymorphic classes should always have a virtual destructor
    virtual ~Base() = default;

  public: // static functions
    static std::unique_ptr<Base<Time>> create(std::ostream& stream = std::cout) {
        return std::make_unique<Base<Time>>(stream);
    }

  public: // methods
    // sim
    virtual void on_sim_start(const Time&) {}
    virtual void on_sim_step(const Time&) {}
    virtual void on_sim_end(const Time&) {}
    // calendar/events
    virtual void on_time_advanced(const Time&, const Time&) {}
    virtual void on_event_scheduled(const Time&, const Devs::_impl::Event<Time>&) {}
    virtual void on_event_action_executed(const Time&, const Devs::_impl::Event<Time>&) {}
    // model
    virtual void on_model_state_transition(const std::string&, const Time&, const std::string&, const std::string&) {}

  protected: // members
    std::ostream& s_;
};

template <typename Time = double> class Verbose : public Base<Time> {

  public: // ctors, dtor
    explicit Verbose(std::ostream& stream = std::cout) : Base<Time>{stream} {}

  public: // static functions
    static std::unique_ptr<Verbose<Time>> create(std::ostream& stream = std::cout) {
        return std::make_unique<Verbose<Time>>(stream);
    }

  public: // methods
    // sim
    void on_sim_start(const Time& time) override { this->s_ << prefix(time) << "Starting simulation\n"; }
    void on_sim_step(const Time& time) override {
        this->s_ << prefix(time) << "---------------------"
                 << "\n";
    }
    void on_sim_end(const Time& time) override { this->s_ << prefix(time) << "Finished simulation\n"; }
    // calendar/event
    void on_time_advanced(const Time& prev, const Time& next) override {
        this->s_ << prefix(prev) << "Time: " << format_time(prev) << " -> " << format_time(next) << "\n";
    }
    void on_event_scheduled(const Time& time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << prefix(time) << "Event scheduled: " << event.to_string() << "\n";
    }
    void on_event_action_executed(const Time& time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << prefix(time) << "Event action executed: " << event.to_string() << "\n";
    }
    // model
    void on_model_state_transition(const std::string& name, const Time& time, const std::string& prev,
                                   const std::string& next) override {
        this->s_ << prefix(time) << "Model " << name << " state: " << prev << " -> " << next << "\n";
    }

  private: // methods
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
} // namespace Printer
//----------------------------------------------------------------------------------------------------------------------
template <typename Time = double> class Simulator {
  public: // ctors, dtor
    explicit Simulator(const std::string model_name, const Devs::Model::Compound<Time> model, const Time start_time,
                       const Time end_time,
                       std::unique_ptr<Printer::Base<Time>> printer = Printer::Verbose<Time>::create())
        : p_calendar_{std::make_unique<Devs::_impl::Calendar<Time>>(start_time, end_time)},
          model_{Devs::_impl::CompoundImpl<Time>{model_name, model, p_calendar_.get()}},
          p_printer_{std::move(printer)} {
        setup_event_listeners();
    }

  public: // methods
    void schedule_model_input(const Time& time, const Dynamic& value) { model_.external_input(time, value); }

    void
    add_model_output_listener(const Devs::_impl::Listener<const std::string&, const Time&, const Dynamic&> listener) {
        model_.add_output_listener(listener);
    }

    void run(const Time& time_epsilon = 0.001) {
        p_printer_->on_sim_start(p_calendar_->time());
        while (p_calendar_->execute_next(model_.select(), time_epsilon)) {

            p_printer_->on_sim_step(p_calendar_->time());
        }
        p_printer_->on_sim_end(p_calendar_->time());
    }

  private: // methods
    void setup_event_listeners() {
        p_calendar_->add_time_advanced_listener(
            [this](const Time& prev, const Time& next) { p_printer_->on_time_advanced(prev, next); });
        p_calendar_->add_event_scheduled_listener([this](const Time& time, const Devs::_impl::Event<Time>& event) {
            p_printer_->on_event_scheduled(time, event);
        });
        p_calendar_->add_event_action_executed_listener(
            [this](const Time& time, const Devs::_impl::Event<Time>& event) {
                p_printer_->on_event_action_executed(time, event);
            });
        // no need attach state transition listener to compound model as it has no state transitions (or state)
        for (auto& pair : model_.components()) {
            auto& p_component = pair.second;
            p_component->add_state_transition_listener(
                [this](const std::string& name, const Time& time, const std::string& prev, const std::string& next) {
                    p_printer_->on_model_state_transition(name, time, prev, next);
                });
        }
    }

  private: // members
    std::unique_ptr<Devs::_impl::Calendar<Time>> p_calendar_;
    Devs::_impl::CompoundImpl<Time> model_;
    std::unique_ptr<Devs::Printer::Base<Time>> p_printer_;
};
//----------------------------------------------------------------------------------------------------------------------
} // namespace Devs
