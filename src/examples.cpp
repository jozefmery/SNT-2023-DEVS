/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       31.03.2023
 */

#include <devs/examples.hpp>
#include <devs/lib.hpp>

namespace Examples {

using Simulator = Devs::Simulator<double>;
using Compound = Devs::Model::Compound<double>;

namespace _impl {

Devs::Model::Atomic<Devs::Null, Devs::Null, Devs::Null> create_minimal_atomic() {
    return Devs::Model::Atomic<Devs::Null, Devs::Null, Devs::Null>{
        Devs::Null{},
        [](const Devs::Null& s, const double&, const Devs::Null&) { return s; },
        [](const Devs::Null& s) { return s; },
        [](const Devs::Null&) { return Devs::Null{}; },
        [](const Devs::Null&) { return 10; },
    };
}

Compound create_queue_model() { return {{}, {}}; }

} // namespace _impl

void minimal_atomic_simulation() {
    Simulator simulator{"minimal atomic model", _impl::create_minimal_atomic(), 0.0, 100.0};
    simulator.run();
}

void minimal_compound_simulation() {}

void traffic_light_simulation() {}

void queue_simulation() {
    // Simulator simulator{"queue system", _impl::create_queue_model(), 0.0, 100.0};
    // simulator.run();
}
} // namespace Examples