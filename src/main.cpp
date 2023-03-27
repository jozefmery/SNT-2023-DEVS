
#include <devs/lib.hpp>
#include <iostream>

double delta_external(const double& s, const double&, const double& input) { return s + input; }

double delta_internal(const double& s) { return s * 2; }

double out(const double& state) { return state * 3; }

double ta(const double& s) { return s; }

auto create_atomic() {
    using namespace Devs::Model;
    return Atomic<double, double, double, double>{0.5, delta_external, delta_internal, out, ta};
}

auto create_compound() {
    using namespace Devs::Model;
    return Compound<double>{{{"component", create_atomic()}},
                            {{"component", {{"test model", {}}}}, {"test model", {{"component", {}}}}}};
}

int main() {
    auto simulator = Devs::Simulator<double>{"test model", create_compound(), 0.0, 5.0};
    simulator.schedule_model_input(2, 0.5);
    // simulator.add_model_output_listener([](const std::string& name, const double& time, const Devs::Dynamic& value) {
    //     std::cout << "Output from: " << name << " at " << time << ", value: " << double{value} << "\n";
    // });
    simulator.run();
    return 0;
}