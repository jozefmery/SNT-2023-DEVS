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

// make sure infity/negative infinity works as expected
// https://stackoverflow.com/a/20016972/5150211
static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");
static_assert(std::numeric_limits<double>::is_iec559, "IEEE 754 required");
// shortcuts
constexpr auto fINF = std::numeric_limits<float>::infinity();
constexpr auto INF = std::numeric_limits<double>::infinity();

namespace Model {

template <typename X, typename Y, typename S, typename Time = double> class Atomic {

  public: // ctors, dtor
    explicit Atomic(const S initial_state, const std::function<S(const S&, const Time&, const X&)> delta_external,
                    const std::function<S(const S&)> delta_internal, const std::function<Y(const S&)> out,
                    const std::function<Time(const S&)> ta)
        : s_{initial_state}, delta_external_{delta_external}, delta_internal_{delta_internal}, out_{out}, ta_{ta} {}

  public: // methods
    auto time_advance() { return ta_(s_); }

    auto internal_transition() {
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

} // namespace Model

namespace Sim {

template <typename Time = double> class Event {

  public: // aliases
    using Action = std::function<void()>;

  public: // ctors, dtor
    explicit Event(const Time time, const Action action, const std::string name = "")
        : time_{time}, action_{action}, name_{name}, cancelled_{std::make_shared<bool>(false)} {}

  public: // methods
    auto to_string(const bool omit_empty_name = true) const {
        std::stringstream s;
        s << "Event{ time = " << time_;

        if (!name_.empty() || !omit_empty_name) {
            s << ", name = " << name_;
        }

        s << std::boolalpha; // write boolean as true/false
        s << ", cancelled = " << *cancelled_ << " }";
        return s.str();
    }

    auto is_cancelled() const { return *cancelled_; }

    auto get_cancel_callback() const {
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

template <typename Time = double> class EventSorter {
  public:
    bool operator()(const Event<Time>& l, const Event<Time>& r) { return l.time() > r.time(); }
};

template <typename Time = double>
using CalendarBase = std::priority_queue<Event<Time>, std::vector<Event<Time>>, EventSorter<Time>>;

template <typename Time = double> class Calendar : public CalendarBase<Time> {

  public: // ctors, dtor
    explicit Calendar() : CalendarBase<Time>{EventSorter<Time>{}} {}

  public: // methods
    auto to_string() const {
        // pass calendar as value as items are deleted when traversing
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

    auto schedule_event(const Event<Time> event) { this->push(event); }

    std::optional<Event<Time>> next() {

        // ignore cancelled events
        while (!this->empty() && this->top().is_cancelled()) {
            this->pop();
        }

        if (this->empty()) {
            return {};
        }
        const auto event = this->top();
        this->pop();
        return event;
    }
};

template <typename Time = double> class Printer {

  public: // ctors, dtor
    explicit Printer(std::ostream& stream = std::cout) : s_{stream} {}

  public: // methods
    virtual void on_start(const Time) {}
    virtual void on_end(const Time) {}
    virtual void on_time_advance(const Time, const Time) {}
    virtual void on_event_schedule(const Time, const Event<Time>&) {}
    virtual void on_event_execution(const Event<Time>&) {}

  protected: // members
    std::ostream& s_;
};

template <typename Time = double> class VerbosePrinter : public Printer<Time> {

  public: // ctors, dtor
    explicit VerbosePrinter(std::ostream& stream = std::cout) : Printer<Time>{stream} {}

  public: // methods
    void on_start(const Time time) override { this->s_ << "[T = " << time << "] Starting simulation...\n"; }
    void on_end(const Time time) override { this->s_ << "[T = " << time << "] Finished simulation\n"; }
    void on_time_advance(const Time prev, const Time next) override {
        this->s_ << "[T = " << prev << "] Advancing time to " << next << "\n";
    }
    void on_event_schedule(const Time time, const Event<Time>& event) override {
        this->s_ << "[T = " << time << "] Scheduling event: " << event.to_string() << "\n";
    }
    void on_event_execution(const Event<Time>& event) override {
        this->s_ << "[T = " << event.time() << "] Executing action of event: " << event.to_string() << "\n";
    }
};

template <typename X, typename Y, typename S, typename Time = double> class Simulator {

  public: // ctors, dtor
    explicit Simulator(const Devs::Model::Atomic<X, Y, S, Time> model, const Time end_time,
                       std::unique_ptr<Printer<Time>> printer = std::make_unique<VerbosePrinter<Time>>())
        : time_{}, end_time_{end_time}, model_{model}, calendar_{}, printer_{std::move(printer)},
          cancel_internal_transition_{} {}

  public: // methods
    auto to_string() const {
        std::stringstream s;
        s << "Simulator{ time = " << time_ << ", calendar = " << calendar_.to_string() << " }";
        return s.str();
    }

    auto schedule_event(const Event<Time> event) {
        // TODO handle event scheduled in the past
        printer_->on_event_schedule(time_, event);
        calendar_.schedule_event(event);
    }

    auto schedule_model_input(const X input) {
        // TODO
    }

    auto run() {

        std::optional<Event<Time>> event{};
        printer_->on_start(time_);

        schedule_internal_transition();

        while (event = calendar_.next()) {
            if (event->time() > end_time_) {
                advance_time(end_time_);
                break;
            }
            advance_time(event->time());
            printer_->on_event_execution(*event);
            event->action();
        }

        printer_->on_end(time_);
    }

  private: // methods
    void schedule_internal_transition() {
        const auto time = model_.time_advance();
        auto event = Event<Time>{time,
                                 [this]() {
                                     // TODO save output
                                     const auto out = model_.internal_transition();
                                     schedule_internal_transition();
                                 },
                                 "internal transition"};
        cancel_internal_transition_ = event.get_cancel_callback();
        schedule_event(event);
    }

    auto advance_time(const Time to) {
        printer_->on_time_advance(time_, to);
        time_ = to;
    }

  private: // members
    Time time_;
    Time end_time_;
    Devs::Model::Atomic<X, Y, S, Time> model_;
    Calendar<Time> calendar_;
    std::unique_ptr<Printer<Time>> printer_;
    std::optional<std::function<void()>> cancel_internal_transition_;
};
} // namespace Sim
} // namespace Devs
