/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       11.03.2023
 */
#pragma once
//----------------------------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------------------------
using Action = std::function<void()>;
template <typename... Args> using Listener = std::function<void(Args...)>;

template <typename... Args> using Listeners = std::vector<Listener<Args...>>;

template <typename X, typename Y, typename S, typename Time = double> struct Atomic {

  public: // ctors, dtor
    explicit Atomic(const S initial_state, const std::function<S(const S&, const Time&, const X&)> delta_external,
                    const std::function<S(const S&)> delta_internal, const std::function<Y(const S&)> out,
                    const std::function<Time(const S&)> ta)
        : s{initial_state}, delta_external{delta_external}, delta_internal{delta_internal}, out{out}, ta{ta} {}

  public: // members
    S s;
    std::function<S(const S&, const Time&, const X&)> delta_external;
    std::function<S(const S&)> delta_internal;
    std::function<Y(const S&)> out;
    std::function<Time(const S&)> ta;
};
//----------------------------------------------------------------------------------------------------------------------
namespace _impl {
template <typename Time> class Event {

  public: // ctors, dtor
    explicit Event(const Time time, const Action action, const std::string description = "")
        : time_{time}, action_{action}, description_{description}, cancelled_{std::make_shared<bool>(false)} {}

  public: // methods
    auto to_string(const bool description = true, const bool cancelled = false) const {
        std::stringstream s;
        s << "Event{ time = " << time_;

        if (description) {
            s << ", description = " << description_;
        }

        if (cancelled) {
            s << std::boolalpha; // write boolean as true/false
            s << ", cancelled = " << *cancelled_;
        }
        s << " }";
        return s.str();
    }

    auto is_cancelled() const { return *cancelled_; }

    auto get_cancel_callback() const {
        // allow cancelling "remotely", as there is no sensible way to traverse a std::priority_queue
        // use a shared pointer for proper cancelling even if the object is moved around
        auto copy = cancelled_;
        return [copy]() { *copy = true; };
    }

    auto time() const { return time_; }

    auto action() const { action_(); }

  private: // members
    Time time_;
    Action action_;
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
    auto& time() const { return time_; }

    auto to_string() const {
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

    auto schedule_event(const Event<Time> event) {
        if (event.time() < time()) {
            std::stringstream s;
            s << "Attempted to schedule an event (" << event.to_string(true)
              << ") in the past (current time: " << time() << ")";
            throw std::runtime_error(s.str());
        }

        // using this-> is required for accesing base class methods in these cases
        this->push(event);
        invoke_listeners<const Time&, const Event<Time>&>(event_scheduled_listeners_, time(), event);
    }

    // returns whether an event has actually been executed
    bool execute_next() {

        const auto event = next();

        if (!event) {
            return false;
        }

        if (event->time() > end_time_) {
            // always finish at the ending time
            advance_time(end_time_);
            return false;
        }

        advance_time(event->time());
        execute_event_action(*event);
        return true;
    }

    auto add_time_advanced_listener(const Listener<const Time&, const Time&> listener) {
        time_advanced_listeners_.push_back(listener);
    }

    auto add_event_scheduled_listener(const Listener<const Time&, const Event<Time>&> listener) {
        event_scheduled_listeners_.push_back(listener);
    }

    auto add_event_action_executed_listeners_(const Listener<const Time&, const Event<Time>&> listener) {
        event_action_executed_listeners_.push_back(listener);
    }

  private: // methods
    std::optional<Event<Time>> next() {

        // ignore cancelled events
        while (!this->empty() && this->top().is_cancelled()) {
            this->pop();
        }

        if (this->empty()) {
            return {};
        }
        // save event as pop does not return removed element, return later
        const auto event = this->top();
        this->pop();
        return event;
    }

    auto execute_event_action(const Event<Time>& event) {
        event.action();
        invoke_listeners<const Time&, const Event<Time>&>(event_action_executed_listeners_, time(), event);
    }

    auto advance_time(const Time& time) {
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

template <typename X, typename Y, typename Time> class IOModel {

  public: // ctors, dtor
    explicit IOModel(const std::string name, Calendar<Time>* p_calendar)
        : name_{name}, p_calendar_{p_calendar}, input_listeners_{}, output_listeners_{} {
        if (name.empty()) {
            throw std::runtime_error("Model name should not be empty");
        }
    }

  public: // methods
    auto& name() const { return name_; }

    auto add_output_listener(const Listener<const std::string&, const Time&, const Y&> listener) {
        output_listeners_.push_back(listener);
    }

    auto input(const Time& time, const X value) const {
        // TODO event description
        schedule_event(Event<Time>{time,
                                   [this, value]() {
                                       invoke_listeners<const std::string&, const Time&, const X&>(
                                           input_listeners_, name(), calendar_time(), value);
                                   },
                                   ""});
    }

    auto schedule_event(const Event<Time> event) const { p_calendar_->schedule_event(event); }

  protected: // methods
    auto calendar_time() const { return p_calendar_->time(); }

    auto add_input_listener(const Listener<const std::string&, const Time&, const X&> listener) {
        input_listeners_.push_back(listener);
    }

    auto output(const Y& value) const {
        invoke_listeners<const std::string&, const Time&, const Y&>(output_listeners_, name(), calendar_time(), value);
    }

  private: // members
    std::string name_;
    Calendar<Time>* p_calendar_;
    Listeners<const std::string&, const Time&, const X&> input_listeners_;
    Listeners<const std::string&, const Time&, const Y&> output_listeners_;
};

template <typename X, typename Y, typename S, typename Time> class AtomicImpl : public IOModel<X, Y, Time> {

  public: // ctors, dtor
    AtomicImpl(const std::string name, Calendar<Time>* p_calendar, const Devs::Atomic<X, Y, S, Time> model)
        : IOModel<X, Y, Time>{name, p_calendar}, model_{model}, last_transition_time_{}, state_transition_listeners_{},
          cancel_internal_transition_{} {
        this->add_input_listener([this](const std::string&, const Time&, const X& input) { input_listener(input); });
        schedule_internal_transition();
    }

  public: // methods
    auto add_state_transition_listener(const Listener<const std::string&, const Time&, const S&, const S&> listener) {
        state_transition_listeners_.push_back(listener);
    }

  private: // methods
    auto state() const { return model_.s; }

    auto transition_state(const S state) {
        invoke_listeners<const std::string&, const Time&, const S&, const S&>(state_transition_listeners_, this->name(),
                                                                              this->calendar_time(), model_.s, state);
        model_.s = state;
    }

    auto time_advance() const { return model_.ta(state()); }

    auto internal_transition() {
        // get output from current state
        const auto out = model_.out(state());
        const auto new_state = model_.delta_internal(state());
        transition_state(new_state);
        return out;
    }

    auto external_transition(const Time& elapsed, const X& input) {
        const auto new_state = model_.delta_external(state(), elapsed, input);
        transition_state(new_state);
    }

    auto update_time(const Time& transition_time) { last_transition_time_ = transition_time; }

    auto get_internal_transition_action() {
        return [this]() {
            const auto out = internal_transition();
            this->output(out);

            schedule_internal_transition();
            update_time(this->calendar_time());
        };
    }

    void schedule_internal_transition() {
        // TODO event description
        const auto event = Devs::_impl::Event<Time>{time_advance(), get_internal_transition_action(), ""};
        cancel_internal_transition_ = event.get_cancel_callback();
        this->schedule_event(event);
    }

    auto input_listener(const X& input) {
        if (cancel_internal_transition_) {
            (*cancel_internal_transition_)();
        }

        external_transition(elapsed_since_last_transition(), input);
        schedule_internal_transition();
        update_time(this->calendar_time());
    }

    auto elapsed_since_last_transition() { return this->calendar_time() - last_transition_time_; }

  private: // members
    Devs::Atomic<X, Y, S, Time> model_;
    Time last_transition_time_;
    Listeners<const std::string&, const Time&, const S&, const S&> state_transition_listeners_;
    std::optional<std::function<void()>> cancel_internal_transition_;
};

// TODO
// class Compound : IOModel {

//   private: // member
//     std::unordered_map<std::string, > models_;
// };

} // namespace _impl
//----------------------------------------------------------------------------------------------------------------------
namespace Const {

// make sure infity/negative infinity works as expected
// https://stackoverflow.com/a/20016972/5150211
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE 754 required");
// shortcuts
constexpr auto fINF = std::numeric_limits<float>::infinity();
constexpr auto INF = std::numeric_limits<double>::infinity();
} // namespace Const
//----------------------------------------------------------------------------------------------------------------------
namespace Printer {
template <typename S, typename Time = double> class Base {

  public: // ctors, dtor
    explicit Base(std::ostream& stream = std::cout) : s_{stream} {}

  public: // static functions
    static auto create(std::ostream& stream = std::cout) { return std::make_unique<Base<S, Time>>(stream); }

  public: // methods
    // sim
    virtual void on_sim_start(const Time&) {
        // cannot use auto return type on virtual methods
    }
    virtual void on_sim_end(const Time&) {}
    // calendar/events
    virtual void on_time_advanced(const Time&, const Time&) {}
    virtual void on_event_scheduled(const Time&, const Devs::_impl::Event<Time>&) {}
    virtual void on_event_action_executed(const Time&, const Devs::_impl::Event<Time>&) {}
    // model
    virtual void on_model_state_transition(const std::string&, const Time&, const S&, const S&) {}

  protected: // members
    std::ostream& s_;
};

template <typename S, typename Time = double> class Verbose : public Base<S, Time> {

  public: // ctors, dtor
    explicit Verbose(std::ostream& stream = std::cout) : Base<S, Time>{stream} {}

  public: // static functions
    static auto create(std::ostream& stream = std::cout) { return std::make_unique<Verbose<S, Time>>(stream); }

  public: // methods
    // sim
    void on_sim_start(const Time& time) override { this->s_ << prefix(time) << "Starting simulation\n"; }
    void on_sim_end(const Time& time) override { this->s_ << prefix(time) << "Finished simulation\n"; }
    // calendar/event
    void on_time_advanced(const Time& prev, const Time& next) override {
        this->s_ << prefix(prev) << "Time: " << format_time(prev) << " -> " << format_time(next) << "\n";
    }
    // model
    void on_model_state_transition(const std::string& name, const Time& time, const S& prev, const S& next) override {
        this->s_ << prefix(time) << "Model " << name << " state: " << prev << " -> " << next << "\n";
    }

  private: // methods
    auto prefix(const Time& time) {
        std::stringstream s;
        s << "[T = " << format_time(time) << "] ";
        return s.str();
    }

    auto format_time(const Time& time) {
        std::stringstream s;
        s << std::fixed << std::setprecision(1);
        s << time;
        return s.str();
    }
};
} // namespace Printer
//----------------------------------------------------------------------------------------------------------------------
template <typename X, typename Y, typename S, typename Time = double> class Simulator {
  public: // ctors, dtor
    explicit Simulator(const Devs::Atomic<X, Y, S, Time> model, const Time start_time, const Time end_time,
                       std::unique_ptr<Printer::Base<S, Time>> printer = Printer::Verbose<S, Time>::create())
        : p_calendar_{std::make_unique<Devs::_impl::Calendar<Time>>(start_time, end_time)},
          model_{Devs::_impl::AtomicImpl<X, Y, S, Time>{"test", p_calendar_.get(), model}}, // TODO remove test
          p_printer_{std::move(printer)} {
        p_calendar_->add_time_advanced_listener(
            [this](const Time& prev, const Time& next) { p_printer_->on_time_advanced(prev, next); });
        model_.add_state_transition_listener(
            [this](const std::string& name, const Time& time, const S& prev, const S& next) {
                p_printer_->on_model_state_transition(name, time, prev, next);
            });
        // TODO add listeners
    }

  public: // methods
    auto run() {
        p_printer_->on_sim_start(p_calendar_->time());
        while (p_calendar_->execute_next())
            ;
        p_printer_->on_sim_end(p_calendar_->time());
    }

  private: // members
    std::unique_ptr<Devs::_impl::Calendar<Time>> p_calendar_;
    Devs::_impl::AtomicImpl<X, Y, S, Time> model_;
    std::unique_ptr<Devs::Printer::Base<S, Time>> p_printer_;
};
//----------------------------------------------------------------------------------------------------------------------
} // namespace Devs
