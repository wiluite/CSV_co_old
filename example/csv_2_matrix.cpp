#include <csv_co/reader.hpp>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace csv_co;

int main()
{
    try
    {
        reader<trim_policy::alltrim> r (std::filesystem::path ("smallpop.csv"));
        std::vector<std::vector<cell_string>> matrix (r.rows());
        auto const cols = r.cols();
        for (auto & elem : matrix) elem.resize(cols);

        auto c_row {-1}; // will be incremented automatically
        auto c_col {0u};

        // ignore header fields, obtain value fields, and trace rows:
        r.run(
                [](auto){}
                ,[&](auto s){ matrix[c_row][c_col++] = s; }
                ,[&] { c_row++; c_col = 0; }
        );

        // population of Southborough,MA:
        std::cout << matrix[0][0] << ',' << matrix[0][1] << ':' << matrix[0][3] << '\n';
#if 0
        // Print all table

        static_assert(std::is_same_v<typename decltype(r)::trim_policy_type, trim_policy::alltrim>);
        for (auto const & row : matrix)
        {
            for (auto const & elem : row)
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

