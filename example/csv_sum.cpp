//
// Created by wiluite on 1/10/23.
//
#include <csv_co/reader.hpp>
#include <iostream>

int main() {

    using namespace csv_co;

    try {
       reader r(std::filesystem::path("smallpop.csv"));

        constexpr unsigned population_col = 3;
        auto sum = 0u;

        r.valid().run_lazy(
                [](auto) {}
                , [&sum](auto & s) {
                    static auto col {0u};
                    if (col++ == population_col) {
                        col = 0;
                        cell_string value;
                        s.read_value(value);
                        sum += std::stoi(value);
                    }
                });
        std::cout << "Total population is: " << sum << std::endl;
    } catch (reader<>::exception const & e)
    {
        std::cout << e.what() << '\n';
    }

}
