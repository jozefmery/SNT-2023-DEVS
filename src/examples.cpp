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

Atomic<Null, Null, Null> create_minimal_atomic() {
    // showcase the creation of a minimal model
    return Atomic<Null, Null, Null>{
        Null{},
        [](const Null& s, const TimeT&, const Null&) { return s; },
        [](const Null& s) { return s; },
        [](const Null&) { return Null{}; },
        [](const Null&) { return Devs::Const::INF; },
    };
}

Compound create_minimal_compound() {
    return Compound{
        {{"minimal atomic component", create_minimal_atomic()}}, // at least one component is required
        {},                                                      // provide no influencers
                                                                 // default to FIFO selector
    };
}

enum TrafficLightInput : int { TURN_OFF, TURN_ON, TOGGLE_POWER, BLINK, _ENUM_MEMBER_COUNT };

// TODO
// Compound create_queue_model() { return {{}, {}}; }

} // namespace _impl

void minimal_atomic_simulation() {
    Simulator simulator{"minimal atomic model", _impl::create_minimal_atomic(), 0.0, 1.0};
    simulator.run();
}

void minimal_compound_simulation() {
    Simulator simulator{"minimal compound model", _impl::create_minimal_compound(), 0.0, 1.0};
    simulator.run();
}

void traffic_light_simulation() {
    std::cout << _impl::TrafficLightInput::_ENUM_MEMBER_COUNT << "\n";
    Simulator simulator{"traffic light model", _impl::create_minimal_compound(), 0.0, 100.0};
    simulator.run();
}

void queue_simulation() {
    // Simulator simulator{"queue system", _impl::create_queue_model(), 0.0, 100.0};
    // simulator.run();
}
} // namespace Examples