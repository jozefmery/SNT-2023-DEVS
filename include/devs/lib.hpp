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
namespace _impl { // declarations
template <typename T> class Box;
template <typename Time> class Calendar;
template <typename Time> class IOModel;
template <typename X, typename Y, typename S, typename Time> class AtomicImpl : public IOModel<Time> {
  public: // ctors, dtor
    explicit AtomicImpl(Calendar<Time>*, const Devs::Atomic<X, Y, S, Time>);
};
} // namespace _impl
//----------------------------------------------------------------------------------------------------------------------
using Action = std::function<void()>;
template <typename... Args> using Listener = std::function<void(Args...)>;

template <typename... Args> using Listeners = std::vector<Listener<Args...>>;

template <typename X, typename Y, typename S, typename Time = double> struct Atomic {

  public: // methods
    operator std::function<std::unique_ptr<Devs::_impl::IOModel<Time>>(Calendar<Time>*)>() {
        return [this](Calendar<Time>* p_calendar_) {
            return std::make_unique<Devs::_impl::AtomicImpl<X, Y, S, Time>>(p_calendar_, *this);
        }
    }

  public: // members
    std::string name;
    S s;
    std::function<S(const S&, const Time&, const X&)> delta_external;
    std::function<S(const S&)> delta_internal;
    std::function<Y(const S&)> out;
    std::function<Time(const S&)> ta;
};

using Influencers = std::unordered_map<std::string, std::vector<std::string>>;

// TODO
template <typename Time> struct Compound {
  public: // ctors, dtor
    Compound(const std::string name,
             const std::vector<std::function<std::unique_ptr<Devs::_impl::IOModel<Time>>>> models,
             const Influencers influencers)
        : name{name}, influencers{influencers},
          // TODO
          models{} {}

  public: // members
    std::string name;
    std::vector<std::function<std::unique_ptr<Devs::_impl::IOModel<Time>>>> models;
    Influencers influencers;
};

//----------------------------------------------------------------------------------------------------------------------
namespace _impl {

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
namespace _impl {
template <typename Time> class Event {

  public: // ctors, dtor
    explicit Event(const Time time, const Action action, const std::string description = "")
        : time_{time}, action_{action}, description_{description}, cancelled_{std::make_shared<bool>(false)} {}

  public: // methods
    std::string to_string(const bool with_description = true, const bool with_cancelled = false) const {
        std::stringstream s;
        s << "Event{ time = " << time();

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

    void add_time_advanced_listener(const Listener<const Time&, const Time&> listener) {
        time_advanced_listeners_.push_back(listener);
    }

    void add_event_scheduled_listener(const Listener<const Time&, const Event<Time>&> listener) {
        event_scheduled_listeners_.push_back(listener);
    }

    void add_event_action_executed_listeners_(const Listener<const Time&, const Event<Time>&> listener) {
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
    explicit IOModel(Calendar<Time>* p_calendar)
        : p_calendar_{p_calendar}, input_transformer_{IOModel::indentity_transformer}, input_listeners_{},
          output_listeners_{} {}

    virtual ~IOModel() = default;

  public: // static members
    static Dynamic identity_transformer(const Dynamic& input) { return input; }

  public: // methods
    virtual const std::string& name() const = 0;

    void add_output_listener(const Listener<const std::string&, const Time&, const Dynamic&> listener) {
        output_listeners_.push_back(listener);
    }

    void set_input_transformer(const std::function<Dynamic(const Dynamic&)> transformer) {
        input_transformer_ = transformer;
    }

    void input(const std::string& from, const Time& time, const Dynamic& value) const {
        schedule_event(Event<Time>{time,
                                   [this, from, value]() {
                                       invoke_listeners<const std::string&, const Dynamic&>(input_listeners_, from,
                                                                                            input_transformer_(value));
                                   },
                                   "input"});
    }

    void schedule_event(const Event<Time> event) const { p_calendar_->schedule_event(event); }

  protected: // methods
    const Time& calendar_time() const { return p_calendar_->time(); }

    void add_input_listener(const Listener<const std::string&, const Dynamic&> listener) {
        input_listeners_.push_back(listener);
    }

    void output(const Dynamic& value) const {
        invoke_listeners<const std::string&, const Time&, const Dynamic&>(output_listeners_, name(), calendar_time(),
                                                                          value);
    }

  private: // members
    Calendar<Time>* p_calendar_;
    std::function<Dynamic(const Dynamic&)> input_transformer_;
    Listeners<const std::string&, const Dynamic&> input_listeners_;
    Listeners<const std::string&, const Time&, const Dynamic&> output_listeners_;
};

template <typename X, typename Y, typename S, typename Time> class AtomicImpl : public IOModel<Time> {

  public: // ctors, dtor
    explicit AtomicImpl(Calendar<Time>* p_calendar, const Devs::Atomic<X, Y, S, Time> model)
        : IOModel<Time>{p_calendar}, model_{model}, last_transition_time_{}, state_transition_listeners_{},
          cancel_internal_transition_{} {
        this->add_input_listener(
            [this](const std::string& from, const Dynamic& input) { dynamic_input_listener(from, input); });
        schedule_internal_transition();
    }

  public: // methods
    void add_state_transition_listener(const Listener<const std::string&, const Time&, const S&, const S&> listener) {
        state_transition_listeners_.push_back(listener);
    }

  private: // methods
    const std::string& name() override { return model_->name; }

    const S& state() const { return model_.s; }

    void transition_state(const S state) {
        invoke_listeners<const std::string&, const Time&, const S&, const S&>(state_transition_listeners_, this->name(),
                                                                              this->calendar_time(), model_.s, state);
        model_.s = state;
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

    void update_time(const Time& transition_time) { last_transition_time_ = transition_time; }

    std::function<void()> get_internal_transition_action() {
        return [this]() {
            const auto out = internal_transition();
            this->output(out);

            schedule_internal_transition();
            update_time(this->calendar_time());
        };
    }

    void schedule_internal_transition() {
        const auto event =
            Devs::_impl::Event<Time>{time_advance(), get_internal_transition_action(), "internal transition"};
        cancel_internal_transition_ = event.get_cancel_callback();
        this->schedule_event(event);
    }

    void dynamic_input_listener(const std::string& from, const Dynamic& input) {
        try {
            input_listener(input);
        } catch (std::bad_cast&) {
            // TODO error message
            std::stringstream s;
            throw std::runtime_error(s.str());
        }
    }

    void input_listener(const X& input) {
        if (cancel_internal_transition_) {
            (*cancel_internal_transition_)();
        }

        external_transition(elapsed_since_last_transition(), input);
        schedule_internal_transition();
        update_time(this->calendar_time());
    }

    Time elapsed_since_last_transition() { return this->calendar_time() - last_transition_time_; }

  private: // members
    Devs::Atomic<X, Y, S, Time> model_;
    Time last_transition_time_;
    Listeners<const std::string&, const Time&, const S&, const S&> state_transition_listeners_;
    std::optional<std::function<void()>> cancel_internal_transition_;
};

// TODO
template <typename Time = double> class CompoundImpl : IOModel<Time> {

  public: // ctors, dtor
    CompoundImpl() : models_{} {}

  public: // methods
    const std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>>& models() { return models_; }

  private: // member
    std::unordered_map<std::string, std::unique_ptr<IOModel<Time>>> models_;
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
template <typename S, typename Time = double> class Base {

  public: // ctors, dtor
    explicit Base(std::ostream& stream = std::cout) : s_{stream} {}

    // polymorphic classes should always have a virtual destructor
    virtual ~Base() = default;

  public: // static functions
    static std::unique_ptr<Base<S, Time>> create(std::ostream& stream = std::cout) {
        return std::make_unique<Base<S, Time>>(stream);
    }

  public: // methods
    // sim
    virtual void on_sim_start(const Time&) {}
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
    static std::unique_ptr<Verbose<S, Time>> create(std::ostream& stream = std::cout) {
        return std::make_unique<Verbose<S, Time>>(stream);
    }

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
template <typename X, typename Y, typename Time = double> class Simulator {
  public: // ctors, dtor
    explicit Simulator(const Devs::_impl::model, const Time start_time, const Time end_time,
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
    void schedule_model_input(const Time& time, const Dynamic& value) { model_.input("", time, value); }

    void add_model_output_listener(const Listener<const std::string&, const Time&, const Dynamic&> listener) {
        model_.add_output_listener(listener);
    }

    void run() {
        p_printer_->on_sim_start(p_calendar_->time());
        while (p_calendar_->execute_next())
            ;
        p_printer_->on_sim_end(p_calendar_->time());
    }

  private: // members
    std::unique_ptr<Devs::_impl::Calendar<Time>> p_calendar_;
    Devs::_impl::CompoundImpl<Time> model_;
    std::unique_ptr<Devs::Printer::Base<Time>> p_printer_;
};
//----------------------------------------------------------------------------------------------------------------------
} // namespace Devs
