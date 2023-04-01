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

Compound create_model() { return {{}, {}}; }

// void schedule_customers(Simulator& simulator) {}

void queue() {
    auto simulator = Simulator{"queue system", create_model(), 0.0, 100.0};
    simulator.run();
}
} // namespace Examples