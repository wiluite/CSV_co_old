#define BOOST_UT_DISABLE_MODULE
#include "ut.hpp"
#include <csv_co/reader.hpp>

int main()
{
    using namespace boost::ut;
    using namespace csv_co;

#if defined (WIN32)
    cfg<override> ={.colors={.none="", .pass="", .fail=""}};
#endif

    "Simple string functions"_test = []
    {

        using namespace string_functions;

        csv_field_string s = "\n\t \r123 456 \t\r\n ";
        alltrim(s);
        expect(s == R"(123 456)");

        s = "\n\t \r \t\r\n ";
        expect (is_vain(s));

        s = R"(""Christmas Tree"" is bad food)";
        unique_quote(s, double_quote());
        expect (s== R"("Christmas Tree" is bad food)");

    };

    "Special [del_last_quote] function"_test = []
    {

        using namespace string_functions;

        csv_field_string s = R"(qwerty")";
        del_last_quote(s, R"(")");
        expect (s == "qwerty");

        s = "qwerty\"\t\n \r";
        del_last_quote(s, R"(")");
        expect (s == "qwerty\t\n \r");

        s = "qwerty\"\t\n~\r";
        del_last_quote(s, R"(")");
        expect(s == "qwerty\"\t\n~\r");

    };

    "Reader callback calculates cells"_test = []
    {

        auto cells{0u};
        reader r
                ("1,2,3\n4,5,6\n7,8,9\n",[&](auto & s)
                {
                    static_assert(std::is_same_v<decltype(s), const csv_field_string&>);
                    ++cells;
                });
        expect(cells == 9);

    };

    "Reader callbacks calculate cols and rows"_test = []
    {

        auto cells{0u}, rows{0u};
        reader r
                ("one,two,three\nfour,five,six\nseven,eight,nine\n"
                 ,[&](auto & s)
                 {
                     cells++;
                 }, [&]()
                 {
                     rows++;
                 });

        expect(cells % rows == 0);
        auto cols = cells/rows;
        expect(cols == 3 && rows == 3);

    };

    "Reader callback is filling data"_test = []
    {

        std::vector<csv_field_string> v;
        reader r
                ("one,two,three\n four, five, six\nseven,eight,nine\n"
                 ,[&](auto & s)
                 {
                    v.push_back(s);
                 });

        std::vector<csv_field_string> v2 {"one","two","three"," four"," five"," six","seven","eight","nine"};
        expect (v2 == v);

    };

    "Reader is trimming data"_test = []
    {

        std::vector<csv_field_string> v;
        static char const trimming_chars [] = " \t\r";
        reader<trim_policy::trimming<trimming_chars>> r
                ("one, \ttwo , three \n four, five, six\n seven , eight\t , nine\r\n"
                ,[&](auto & s)
                {
                    v.push_back(s);
                });

        std::vector<csv_field_string> v2 {"one","two","three","four","five","six","seven","eight","nine"};
        expect (v2 == v);

    };

    "Reader [knows] last row may lack line feed"_test = []
    {

        std::vector<csv_field_string> v;
        reader r
                ("one,two,three\nfour,five,six"
                ,[&](auto & s)
                {
                    v.push_back(s);
                });

        expect (v.size() == 6);
        expect (v.back() == "six");

    };

    "Reader with another delimiter character"_test = []
    {

        std::vector<csv_field_string> v;

        using new_delimiter = delimiter<';'>;

        reader<trim_policy::no_trimming, double_quote, new_delimiter> r
                ("one;two;three\nfour;five;six"
                ,[&](auto & s)
                {
                    v.push_back(s);
                });

        expect (v.size() == 6);
        expect (v == std::vector<csv_field_string>{{"one","two","three","four","five","six"}});

    };

    "Reader provides empty cell as expected"_test = []
    {
        std::vector<csv_field_string> v;
        reader r
                ("one,two,three\nfour,,six"
                ,[&](auto & s)
                {
                    v.push_back(s);
                });

        expect (v.size() == 6);
        expect (v == std::vector<csv_field_string>{{"one","two","three","four","","six"}});

    };


    // -- Now, Topic Change: Quoting --

    // NOTE:
    "Incorrect use of single quotes inside quoted cell"_test = []
    {

        std::vector<csv_field_string> v;
        std::size_t cells{0};
        reader r
                (R"(2022, Mouse, "It's incorrect to use "Hello, Christmas Tree!"" ,, "4900,00")"
                ,[&](auto & s)
                {
                    cells++;
                    v.push_back(s);
                });

        expect (cells == 6);
        expect (v == std::vector<csv_field_string>
                {{"2022"," Mouse",R"( It's incorrect to use "Hello)",R"( Christmas Tree!" )",""," 4900,00"}});

    };

    "Correct use of doubled quotes inside quoted cell"_test = []
    {

        std::vector<csv_field_string> v;
        std::size_t cells{0};
        reader r
                (R"(2022, Mouse, "It's a correct use case: ""Hello, Christmas Tree!""" ,, "4900,00")"
                ,[&](auto & s)
                {
                    cells++;
                    v.push_back(s);
                });

        expect (cells == 5);
        expect (v == std::vector<csv_field_string>
                {{"2022"," Mouse",R"( It's a correct use case: "Hello, Christmas Tree!" )",""," 4900,00"}});

    };

    "Correct use case of quoted parts of the cell"_test = []
    {

        std::vector<csv_field_string> v;
        std::size_t cells{0};
        reader r
                (R"(2022,Mouse,What is quoted is necessary part "Hello, Tree!" of the sell,,"4900,00")"
                , [&](auto & s)
                {
                    cells++;
                    v.push_back(s);
                });
        expect (cells == 5);
        expect (v == std::vector<csv_field_string>
                {{"2022", "Mouse", R"(What is quoted is necessary part "Hello, Tree!" of the sell)","","4900,00"}});

    };

    "Reader with another quoting character"_test = []
    {

        constexpr std::size_t CORRECT_RESULT = 1;
        auto cells{0u};
        reader r
                (R"("just one, and only one, quoted cell")"
                , [&](auto & s)
                {
                    cells++;
                });
        expect (cells == CORRECT_RESULT);

        constexpr std::size_t INCORRECT_RESULT = 3;
        cells = 0u;
        reader r2
                (R"(`just one, and only one, quoted cell`)"
                        , [&](auto & s)
                {
                    cells++;
                });
        expect (cells == INCORRECT_RESULT);

        constexpr std::size_t CORRECT_RESULT_AGAIN = 1;
        cells = 0u;

        using new_quote_char = quote_char<'`'>;

        reader<trim_policy::no_trimming, new_quote_char> r3
                (R"(`just one, and only one, quoted cell`)"
                        , [&](auto & s)
                {
                    expect (s == "just one, and only one, quoted cell");
                    cells++;
                });
        expect (cells == CORRECT_RESULT_AGAIN);

    };

    // -- Topic change: File processing --

    "Reader processes a well-known file"_test = []
    {
        auto cells{0u};
        std::string first_string {};
        auto rows {0u};

        expect(nothrow([&]
        {

        reader r
                (std::filesystem::path ("game.csv")
                , [&](auto & s)
                {
                    cells++;
                    if (rows < 1)
                    {
                        first_string += s;
                    }
                }
                , [&]
                {
                    rows++;
                });
        }) >> fatal) << "doesn't throw";

        expect (first_string == "hello, world1!\r");

        // for now let us do trimming manually, just for diversity
        // that is what the parser does when with a trimming policy
        string_functions::alltrim(first_string);
        expect (first_string == "hello, world1!");

        expect (rows == 100000);
        expect (cells/rows == 6);

    };

}

