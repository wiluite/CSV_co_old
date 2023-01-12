//
// Created by wiluite on 1/9/23.
//
#include <csv_co/reader.hpp>
#include <iostream>

using namespace csv_co;
int main(int argc, char ** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: ./lazybench <csv_file>\n";
        return EXIT_FAILURE;
    }

    std::size_t num_exp = 5;
    std::size_t accum_times = 0;

    try
    {

        auto cells{0u};
        auto rows {0u};

        auto save_num_exp = num_exp;
        while (num_exp--)
        {
            auto const begin = std::chrono::high_resolution_clock::now();
            cells = rows = 0;
            reader r (std::filesystem::path {argv[1]});
            r.run_lazy([&cells](auto & s){ ++cells; }, [&rows]{ ++rows; });
            auto const end = std::chrono::high_resolution_clock::now();
            accum_times += std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count();
        }

        std::cout << "Rows:  " << rows << '\n' << "Cells: " << cells << '\n' << "Execution time: " <<
        accum_times/save_num_exp << "ms" << '\n';

    } catch (reader<>::exception const & e)
    {
        std::cout << e.what() << std::endl;
    }

}
