#include <csv_co/reader.hpp>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace csv_co;

int main()
{
    using reader_type = reader<trim_policy::alltrim>;

    try {
        reader_type r(std::filesystem::path("smallpop.csv"));
        std::vector<cell_string> ram;
        ram.reserve(r.cols()*r.rows());
        r.valid().run( // check validity and run
        [](auto & s) {
            // ignore header fields
            static_assert(std::is_same_v<decltype(s), cell_string const&>);
        }
        ,[&ram](auto s) {
            // save value fields
            ram.push_back(std::move(s));
        });
        // population of Southborough,MA:
        std::cout << ram [0] << ',' << ram[1] << ':' << ram[3] << '\n';

    } catch (reader_type::exception const & e)
    {
        std::cout << e.what() << '\n';
    }
}

