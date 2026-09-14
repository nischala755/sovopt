#include <sovereign/cli.hpp>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<std::string_view> arguments;
    for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
    return sovereign::run_cli(arguments,std::cout,std::cerr);
}
