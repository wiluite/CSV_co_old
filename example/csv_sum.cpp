//
// Created by wiluite on 1/10/23.
//
#include <csv_co/reader.hpp>
#include <iostream>

using namespace csv_co;

int main() {

    try {
        reader r(std::filesystem::path("smallpop.csv"));
        auto sum = 0u;
        r.valid().run_lazy(
                [](auto) {}
                , [&](auto & s) {
                    static auto col {0u};
                    if (col++ == 3) {
                        col = 0;
                        cell_string value;
                        s.read_value(value);
                        sum += std::stoi(value);
                    }
                });
        std::cout << sum << std::endl;
    } catch (reader<>::exception const & e)
    {
        std::cout << e.what() << '\n';
    }

}
