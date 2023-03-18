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
//----------------------------------------------------------------------------------------------------------------------
using Action = std::function<void()>;
namespace _impl {
template <typename Time> class Event {

  public: // ctors, dtor
    explicit Event(const Time time, const Action action, const std::string name = "")
        : time_{time}, action_{action}, name_{name}, cancelled_{std::make_shared<bool>(false)} {}

  public: // methods
    auto to_string(const bool omit_cancelled = false, const bool omit_empty_name = true) const {
        std::stringstream s;
        s << "Event{ time = " << time_;

        if (!name_.empty() || !omit_empty_name) {
            s << ", name = " << name_;
        }

        if (!omit_cancelled) {
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
    std::string name_;
    std::shared_ptr<bool> cancelled_;
};

template <typename Time> class EventSorter {
  public:
    // a sooner event should get priority, FIFO otherwise
    bool operator()(const Event<Time>& l, const Event<Time>& r) { return l.time() > r.time(); }
};

template <typename Time>
using CalendarBase = std::priority_queue<Event<Time>, std::vector<Event<Time>>, EventSorter<Time>>;

template <typename Time> class Calendar : private CalendarBase<Time> {

  public: // ctors, dtor
    explicit Calendar() : CalendarBase<Time>{EventSorter<Time>{}} {}

  public: // methods
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

    // using this-> is required for accesing base class methods in this case
    auto schedule_event(const Event<Time> event) { this->push(event); }

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
};
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
    virtual void on_start(const Time) {}
    virtual void on_end(const Time) {}
    virtual void on_time_advance(const Time, const Time) {}
    virtual void on_event_schedule(const Time, const Devs::_impl::Event<Time>&) {}
    virtual void on_event_schedule_in_past(const Time, const Devs::_impl::Event<Time>&) {}
    virtual void on_event_execution(const Devs::_impl::Event<Time>&) {}
    virtual void on_internal_transition(const Time, const S&, const S&) {}
    virtual void on_external_transition(const Time, const S&, const S&) {}

  protected: // members
    std::ostream& s_;
};

template <typename S, typename Time = double> class Verbose : public Base<S, Time> {

  public: // ctors, dtor
    explicit Verbose(std::ostream& stream = std::cout) : Base<S, Time>{stream} {}

  public: // static functions
    static auto create(std::ostream& stream = std::cout) { return std::make_unique<Verbose<S, Time>>(stream); }

  public: // methods
    void on_start(const Time time) override { this->s_ << "[T = " << time << "] Starting simulation\n"; }
    void on_end(const Time time) override { this->s_ << "[T = " << time << "] Finished simulation\n"; }
    void on_time_advance(const Time prev, const Time next) override {
        this->s_ << "[T = " << prev << "] Advancing time to " << next << "\n";
    }
    void on_event_schedule(const Time time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << "[T = " << time << "] Scheduling event: " << event.to_string(true) << "\n";
    }
    void on_event_schedule_in_past(const Time time, const Devs::_impl::Event<Time>& event) override {
        this->s_ << "[T = " << time << "] ERROR: Attempted to schedule event in the past: " << event.to_string()
                 << "\n";
    }
    void on_event_execution(const Devs::_impl::Event<Time>& event) override {
        this->s_ << "[T = " << event.time() << "] Executing action of event: " << event.to_string(true) << "\n";
    }
    void on_internal_transition(const Time time, const S& prev, const S& next) override {
        this->s_ << "[T = " << time << "] Internal state transition from " << prev << " to " << next << "\n";
    }
    void on_external_transition(const Time time, const S& prev, const S& next) override {
        this->s_ << "[T = " << time << "] External state transition from " << prev << " to " << next << "\n";
    }
};
} // namespace Printer
//----------------------------------------------------------------------------------------------------------------------
template <typename X, typename Y, typename S, typename Time = double> class Atomic {

  public: // ctors, dtor
    explicit Atomic(const S initial_state, const std::function<S(const S&, const Time&, const X&)> delta_external,
                    const std::function<S(const S&)> delta_internal, const std::function<Y(const S&)> out,
                    const std::function<Time(const S&)> ta)
        : s_{initial_state}, delta_external_{delta_external}, delta_internal_{delta_internal}, out_{out}, ta_{ta} {}

  public: // methods
    auto state() const { return s_; }

    auto time_advance() { return ta_(s_); }

    auto internal_transition() {
        // get output from current state
        const auto out = out_(s_);
        s_ = delta_internal_(s_);
        return out;
    }

    auto external_transition(const Time elapsed, const X input) { s_ = delta_external_(s_, elapsed, input); }

  private: // members
    S s_;
    std::function<S(const S&, const Time&, const X&)> delta_external_;
    std::function<S(const S&)> delta_internal_;
    std::function<Y(const S&)> out_;
    std::function<Time(const S&)> ta_;
};

template <typename X, typename Y, typename S, typename Time = double> class Simulator {
  public: // ctors, dtor
    explicit Simulator(const Devs::Atomic<X, Y, S, Time> model,
                       const std::vector<std::function<void(const Y&)>> output_listeners, const Time start_time,
                       const Time end_time,
                       std::unique_ptr<Printer::Base<S, Time>> printer = Printer::Verbose<S, Time>::create())
        : time_{start_time}, end_time_{end_time}, elapsed_{}, model_{model}, output_listeners_{output_listeners},
          calendar_{}, printer_{std::move(printer)}, cancel_internal_transition_{} {}

  public: // methods
    auto to_string() const {
        std::stringstream s;
        s << "Simulator{ time = " << time_ << ", calendar = " << calendar_.to_string() << " }";
        return s.str();
    }

    auto schedule_event(const Devs::_impl::Event<Time> event) {
        if (event.time() < time_) {
            printer_->on_event_schedule_in_past(time_, event);
            return;
        }
        printer_->on_event_schedule(time_, event);
        calendar_.schedule_event(event);
    }

    auto schedule_model_input(const Time time, const X input) {
        const auto event =
            Devs::_impl::Event<Time>{time, get_external_transition_action(time, input), "external input"};
        schedule_event(event);
    }

    auto run() {

        std::optional<Devs::_impl::Event<Time>> event{};
        printer_->on_start(time_);

        schedule_internal_transition();

        while (event = calendar_.next()) {
            if (event->time() > end_time_) {
                time_advance(end_time_);
                break;
            }
            time_advance(event->time());
            printer_->on_event_execution(*event);
            event->action();
        }

        printer_->on_end(time_);
    }

  private: // methods
    auto invoke_output_listeners(const Y& out) const {
        for (const auto listener : output_listeners_) {
            listener(out);
        }
    }

    auto get_external_transition_action(const Time time, const X input) {
        return [this, time, input]() {
            if (cancel_internal_transition_) {
                (*cancel_internal_transition_)();
            }
            const auto prev = model_.state();
            model_.external_transition(elapsed_, input);
            const auto next = model_.state();
            printer_->on_external_transition(time_, prev, next);
            schedule_internal_transition();
        };
    }

    auto get_internal_transition_action() {
        return [this]() {
            const auto prev = model_.state();
            const auto out = model_.internal_transition();
            const auto next = model_.state();

            invoke_output_listeners(out);

            printer_->on_internal_transition(time_, prev, next);
            schedule_internal_transition();
        };
    }

    void schedule_internal_transition() {
        const auto event =
            Devs::_impl::Event<Time>{model_.time_advance(), get_internal_transition_action(), "internal transition"};
        cancel_internal_transition_ = event.get_cancel_callback();
        schedule_event(event);
    }

    auto time_advance(const Time to) {
        printer_->on_time_advance(time_, to);
        elapsed_ = to - time_;
        time_ = to;
    }

  private: // members
    Time time_;
    Time end_time_;
    Time elapsed_;
    Devs::Atomic<X, Y, S, Time> model_;
    std::vector<std::function<void(const Y&)>> output_listeners_;
    Devs::_impl::Calendar<Time> calendar_;
    std::unique_ptr<Printer::Base<S, Time>> printer_;
    std::optional<std::function<void()>> cancel_internal_transition_;
};
//----------------------------------------------------------------------------------------------------------------------
} // namespace Devs
