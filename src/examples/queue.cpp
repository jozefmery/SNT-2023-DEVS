/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       31.03.2023
 */

#include <devs/lib.hpp>

namespace Examples {

Devs::Model::Compound<double> create_model() { return {{}, {}}; }

void queue() {
    auto simulator = Devs::Simulator<double>{"queue system", create_model(), 0.0, 100.0};
    simulator.run();
}
} // namespace Examples