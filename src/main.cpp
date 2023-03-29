/*
 *  Project:    SNT DEVS 2023
 *  Author:     Bc. Jozef Méry - xmeryj00@vutbr.cz
 *  Date:       31.03.2023
 */

#include <devs/lib.hpp>
#include <examples/queue.hpp>
#include <iostream>

// double delta_external(const double& s, const double&, const double& input) { return s + input; }

// double delta_internal(const double& s) { return s * 2; }

// double out(const double& state) { return state * 3; }

// double ta(const double& s) { return s; }

// auto create_atomic() {

//     return Devs::Model::Atomic<double, double, double, double>{0.5, delta_external, delta_internal, out, ta};
// }

// auto create_compound() {
//     return Devs::Model::Compound<double>{
//         {{"atomic", create_atomic()}},
//         {{"atomic", {{{}, [](const Devs::Dynamic& value) { return double{value} + 1.0; }}}}, {{}, {{"atomic", {}}}}},
//     };
// }

// auto create_compound2() {
//     return Devs::Model::Compound<double>{
//         {{"compound", create_compound()}},
//         {{"compound", {{{}, {}}}}, {{}, {{"compound", {}}}}},
//     };
// }

std::unordered_map<std::string, std::function<void()>> create_examples() { return {{"queue", Examples::queue}}; }

std::vector<std::string> get_args(int argc, char* argv[]) {
    std::vector<std::string> args{};

    for (int arg = 1; arg < argc; ++arg) {
        args.push_back(argv[arg]);
    }

    return args;
}

std::optional<std::vector<std::string>> parse_arguments(const std::vector<std::string>& args,

                                                        const std::vector<std::string>& example_names) {
    std::vector<std::string> examples_to_run{};

    for (const auto& arg : args) {
        if (arg == "-h" || arg == "--help") {
            return std::nullopt;
        }
        const auto it = std::find(example_names.begin(), example_names.end(), arg);

        if (it == example_names.end()) {
            std::cerr << "Invalid example name provided: " << arg << "\n";
            continue;
        }

        examples_to_run.push_back(arg);
    }

    return examples_to_run;
}

void print_help(const std::vector<std::string>& example_names) {
    std::cout << "Demo application for a DEVS simulation library (SNT 2023)\n"
              << "Usage: \n"
              << "    devs [-h | --help] <example>...\n\n";
    std::cout << "Available examples: \n";
    for (const auto& example : example_names) {
        std::cout << "  " << example << "\n";
    }
    std::cout << "\nAuthor: Jozef Méry\n";
}

void run_examples(const std::unordered_map<std::string, std::function<void()>> examples,
                  const std::vector<std::string>& example_names) {
    if (example_names.empty()) {

        std::cout << "No examples provided for running...\n";
    }

    for (const auto& example : example_names) {
        std::cout << "Running example: " << example << "\n";
        examples.at(example)();
        std::cout << "Finished example: " << example << "\n";
        std::cout << "--------------------\n";
    }
}

int main(int argc, char* argv[]) {

    try {
        const auto args = get_args(argc, argv);
        const auto examples = create_examples();
        std::vector<std::string> example_names{};
        for (const auto& example : examples) {
            example_names.push_back(example.first);
        }

        const auto examples_to_run = parse_arguments(args, example_names);

        if (!examples_to_run) {
            print_help(example_names);
            return 0;
        }

        run_examples(examples, *examples_to_run);

    } catch (std::runtime_error& e) {

        std::cerr << "Runtime error: " << e.what() << "\n";
        return 1;

    } catch (...) {
        std::cerr << "Unknown exception crashed the application\n";
        return 1;
    }
    return 0;
}