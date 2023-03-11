
#include <devs/lib.hpp>
#include <iostream>

auto delta_external(const double& s, const double&, const double&) { return s + 1.0; }

auto delta_internal(const double& s) { return s + 0.1; }

auto out(const double&) { return -1; }

auto ta(const double& s) { return s; }

auto create_model() {
    return Devs::Model::create_atomic<double, int, double, double>(2.0, delta_external, delta_internal, out, ta);
}

int main() {

    const auto model = create_model();
    auto context = Devs::Sim::create_context();
    Devs::Sim::schedule_event(
        3.0, []() {}, context);
    std::cout << Devs::Sim::context_to_string(context) << std::endl;
}