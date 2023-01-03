#include <csv_co/reader.hpp>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace csv_co;

int main()
{
    try
    {
        static char const trimming_chars [] = "\r";
        reader<trim_policy::trimming<trimming_chars>> r (std::filesystem::path ("uspop.csv"));
        std::vector<std::vector<cell_string>> matrix (r.rows());
        auto const cols = r.cols();
        for (auto & elem : matrix) elem.resize(cols);

        auto c_row{0u};
        auto c_col{0u};

        r.run(
                [&](auto & s){ matrix[c_row][c_col++] = s;},[&] { c_row++; c_col = 0;}
        );

        // population of Selma, Al
        std::cout << "Population of " << matrix[6][0] << ',' << matrix[6][1] << ": " << matrix[6][2] << '\n';
#if 0
        // Print all table

        static_assert(std::is_same_v<typename decltype(r)::trim_policy_type, trim_policy::trimming<trimming_chars>>);
        for (auto &row : matrix)
        {
            for (auto & elem : row)
            {
                std::cout << elem << ' ';
            }
            std::cout << '\n';
        }
#endif

    } catch (std::exception const & e)
    {
        std::cout << e.what() << std::endl;
    }

}

