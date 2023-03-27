
#include <devs/lib.hpp>
#include <iostream>

auto delta_external(const double& s, const double&, const double& input) { return s + input; }

auto delta_internal(const double& s) { return s * 2; }

auto out(const double&) { return -1; }

auto ta(const double& s) { return s; }

auto create_atomic() {
    using namespace Devs::Model;
    return Atomic<double, int, double, double>{0.5, delta_external, delta_internal, out, ta};
}

auto create_compound() {
    using namespace Devs::Model;
    return Compound<double>{{{"component", create_atomic()}}, {}};
}

int main() {
    auto simulator = Devs::Simulator{"test model", create_compound(), 0.0, 5.0};
    simulator.run();
    return 0;
}