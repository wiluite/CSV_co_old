#define BOOST_UT_DISABLE_MODULE
#include "ut.hpp"
#include <csv_co/reader.hpp>
#include <fstream>

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

        cell_string s = "\n\t \r123 456 \t\r\n ";
        alltrim(s);
        expect(s == R"(123 456)");

        s = "\n\t \r \t\r\n ";
        expect (is_devastated(s));

        s = R"(""Christmas Tree"" is bad food)";
        unique_quote(s, double_quote());
        expect (s== R"("Christmas Tree" is bad food)");

    };

    "Special [del_last_quote] function"_test = []
    {

        using namespace string_functions;

        cell_string s = R"(qwerty")";
        del_last_quote(s, R"(")");
        expect (s == "qwerty");

        s = "qwerty\"\t\n \r";
        del_last_quote(s, R"(")");
        expect (s == "qwerty\t\n \r");

        s = "qwerty\"\t\n~\r";
        del_last_quote(s, R"(")");
        expect(s == "qwerty\"\t\n~\r");

    };

    "Reader callback calculates cells from char const *"_test = []
    {

        auto cells{0u};
        reader r ("1,2,3\n4,5,6\n7,8,9\n");
        r.run([&](auto & s)
              {
                  static_assert(std::is_same_v<decltype(s), const cell_string&>);
                  ++cells;
              });

        expect(cells == 9);

    };

    "Reader callback calculates cells from rvalue string"_test = []
    {

        auto cells{0u};
        reader r (cell_string("1,2,3\n4,5,6\n"));
        r.run([&](auto & s)
              {
                  static_assert(std::is_same_v<decltype(s), const cell_string&>);
                  ++cells;
              });

        expect(cells == 6);

    };


    "Reader callbacks calculate cols and rows"_test = []
    {

        auto cells{0u}, rows{0u};
        reader r ("one,two,three\nfour,five,six\nseven,eight,nine\n");
        r.run([&](auto & s)
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

    "Reader calculates cols and rows via special methods"_test = []
    {

        reader r ("one,two,three\nfour,five,six\nseven,eight,nine\n,ten,eleven,twelve\n");
        expect (r.rows() == 4);
        expect (r.cols() == 3);

        // exception, csv-string cannot be empty
        // this is to correspond to behaviour on memory-mapping zero-sized csv-files
        expect(throws([]{reader r2 ("");}));

        // NOW YES, there is ONE empty field, so there is a row and a column
        reader r3 ("\n");
        expect (r3.rows() == 1);
        expect (r3.cols() == 1);

        // as well...
        reader r4 (" ");
        expect (r4.rows() == 1);
        expect (r4.cols() == 1);

    };

    "Reader callback is filling data"_test = []
    {

        std::vector<cell_string> v;
        reader r ("one,two,three\n four, five, six\nseven,eight,nine\n");
        r.run([&](auto & s)
              {
                  v.push_back(s);
              });

        std::vector<cell_string> v2 {"one", "two", "three", " four", " five", " six", "seven", "eight", "nine"};
        expect (v2 == v);

    };

    "Reader is trimming data"_test = []
    {

        std::vector<cell_string> v;

        reader<trim_policy::alltrim>
            r ("one, \ttwo , three \n four, five, six\n seven , eight\t , nine\r\n");

        r.run([&](auto & s)
              {
                  v.push_back(s);
              });

        std::vector<cell_string> v2 {"one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
        expect (v2 == v);

    };

    "Reader [knows] last row may lack line feed"_test = []
    {

        std::vector<cell_string> v;
        reader r ("one,two,three\nfour,five,six");
        r.run([&](auto & s)
              {
                  v.push_back(s);
              });

        expect (v.size() == 6);
        expect (v.back() == "six");

    };

    "Reader with another delimiter character"_test = []
    {

        std::vector<cell_string> v;

        using new_delimiter = delimiter<';'>;

        reader<trim_policy::no_trimming, double_quote, new_delimiter> r ("one;two;three\nfour;five;six");
        r.run([&](auto & s)
              {
                  v.push_back(s);
              });

        expect (v.size() == 6);
        expect (v == std::vector<cell_string>{{"one", "two", "three", "four", "five", "six"}});

    };

    "Reader provides empty cell as expected"_test = []
    {

        std::vector<cell_string> v;
        reader r ("one,two,three\nfour,,six");
        r.run([&](auto & s)
              {
                  v.push_back(s);
              });

        expect (v.size() == 6);
        expect (v == std::vector<cell_string>{{"one", "two", "three", "four", "", "six"}});

    };
    
    // -- so now... topic change: Quoting --

    // NOTE:
    "Incorrect use of single quotes inside quoted cell"_test = []
    {

        std::vector<cell_string> v;
        std::size_t cells{0};
        reader r (R"(2022, Mouse, "It's incorrect to use "Hello, Christmas Tree!"" ,, "4900,00")");
        r.run([&](auto & s)
              {
                  cells++;
                  v.push_back(s);
              });

        expect (cells == 6);
        expect (v == std::vector<cell_string>
                {{"2022"," Mouse",R"( It's incorrect to use "Hello)",R"( Christmas Tree!" )",""," 4900,00"}});

    };

    "Correct use of doubled quotes inside quoted cell"_test = []
    {

        std::vector<cell_string> v;
        std::size_t cells{0};
        reader r (R"(2022, Mouse, "It's a correct use case: ""Hello, Christmas Tree!""" ,, "4900,00")");

        r.run([&](auto & s)
              {
                  cells++;
                  v.push_back(s);
              });

        expect (cells == 5);
        expect (v == std::vector<cell_string>
                {{"2022"," Mouse",R"( It's a correct use case: "Hello, Christmas Tree!" )",""," 4900,00"}});

    };

    "Correct use case of quoted parts of the cell"_test = []
    {

        std::vector<cell_string> v;
        std::size_t cells{0};
        reader r (R"(2022,Mouse,What is quoted is necessary part "Hello, Tree!" of the cell,,"4900,00")");
        r.run([&](auto & s)
              {
                  cells++;
                  v.push_back(s);
              });

        expect (cells == 5);
        expect (v == std::vector<cell_string>
                {{"2022", "Mouse", R"(What is quoted is necessary part "Hello, Tree!" of the cell)","","4900,00"}});

    };

    "Reader with another quoting character"_test = []
    {

        constexpr std::size_t CORRECT_RESULT = 1;
        auto cells{0u};
        reader r (R"("just one, and only one, quoted cell")");
        r.run([&](auto & s)
              {
                  cells++;
              });
        expect (cells == CORRECT_RESULT);

        constexpr std::size_t INCORRECT_RESULT = 3;
        cells = 0u;
        reader r2 (R"(`just one, and only one, quoted cell`)");
        r2.run([&](auto & s)
               {
                   cells++;
               });
        expect (cells == INCORRECT_RESULT);

        constexpr std::size_t CORRECT_RESULT_AGAIN = 1;
        cells = 0u;

        using new_quote_char = quote_char<'`'>;

        reader<trim_policy::no_trimming, new_quote_char> r3 (R"(`just one, and only one, quoted cell`)");
        r3.run([&](auto & s)
               {
                   expect (s == "just one, and only one, quoted cell");
                   cells++;
               });
        expect (cells == CORRECT_RESULT_AGAIN);

    };

    // -- Topic change: File processing --

    "Read a well-known file"_test = []
    {

        auto cells{0u};
        std::string first_string {};
        auto rows {0u};

        expect(nothrow([&]
        {
            reader r (std::filesystem::path ("game.csv"));
            r.run([&](auto & s)
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
            expect (r.rows() == 14);
            expect (r.cols() == 6);
        }) >> fatal
        ) << "it shouldn't throw";

        // depending on line-breaking style, note: reader's trimming  policy is absent
        expect (first_string == "hello, world1!\r" || first_string == "hello, world1!");

        // for now let us do trimming manually, just for diversity
        // that is what the parser does when with a trimming policy
        string_functions::alltrim(first_string);
        expect (first_string == "hello, world1!");

        expect (rows == 14);
        expect (cells/rows == 6);

    };

    "Read an empty file"_test = []
    {

        auto cells{0u};
        auto rows {0u};

        std::fstream fs;
        fs.open("empty.csv", std::ios::out);
        fs.close();

        expect(throws([&]
                       {
                           reader r (std::filesystem::path ("empty.csv"));
                           r.run([&](auto & s)
                                 {
                                     cells++;
                                 }
                                 ,[&]
                                 {
                                     rows++;
                                 });

                       }) >> fatal
        ) << "it should throw!";

    };

    "Read a well-known file with header"_test = []
    {

        auto cells{0u};
        auto rows {0u};
        std::vector<cell_string> v;
        std::vector<cell_string> v2;
        expect(nothrow([&]
                      {
                          static char const chars [] = "\r";
                          reader<trim_policy::trimming<chars>> r (std::filesystem::path ("smallpop.csv"));
                          r.run([&](auto & s)
                                {
                                    v.push_back(s);
                                }
                                , [&](auto & s)
                                {
                                    v2.push_back(s);
                                    cells++;
                                }
                                , [&]
                                {
                                    rows++;
                                }
                          );

                          expect (rows == r.rows());
 
                      }) >> fatal
        ) << "it shouldn't throw";

        expect (v == std::vector<cell_string>{"city","region","country","population"});
        expect (cells == (rows-1) * v.size());
        expect (v2.size() == 10*4);
        expect (v2.front() == "Southborough");
        expect (v2.back() == "42605");
        expect ( nothrow ([&v2] { expect (std::stoi(v2.back()) == 42605);}));

    };

    // -- Topic change: Check Validity --
    "Check validity of the csv format"_test = []
    {

        expect (reader ("1,2,3\n").valid());
        expect (!reader ("1,2,3\n4\n").valid());
        expect (!reader ("1,2,3\n4,5\n").valid());
        expect (reader ("1,2,3\n4,5,6\n").valid());
        expect (!reader ("1,2,3\n4,5,6,7\n").valid());
        expect (!reader ("1,2,3\n4,5,6\n7").valid());
        expect (reader ("1,2,3\n4,5,6\n7,8,9").valid());

        expect (reader (std::filesystem::path ("smallpop.csv")).valid());
        expect (!reader (std::filesystem::path ("game-invalid-format.csv")).valid());

    };

    // -- Topic change: Move Operations --
    "Move construction and assignment"_test = []
    {

        reader r ("One,Two,Three\n1,2,3\n");
        reader r2 = std::move(r);
        //---- r is in Move-From State. We must not touch it, wait for destruction, but let us risk:
        expect (!r.valid());
        //-------------------------------------
        expect (r2.valid());
        expect (r2.cols() == 3);
        expect (r2.rows() == 2);
        auto head_cells{0u};
        auto cells{0u};
        auto rows {0u};
        r2.run([&head_cells] (auto const & s)
            {
                expect (s=="One" || s=="Two" || s=="Three");
                head_cells++;
            }
            ,[&cells](auto const& s)
            {
                expect (s=="1" || s=="2" || s=="3");
                ++cells;
            }
            ,[&rows] {
                rows++;
            }
        );
        expect (head_cells == 3);
        expect (cells == 3);
        expect (rows == 2);

        // MOVE BACK (check for move assignment)
        r = std::move(r2);
        expect (r.valid());
        expect (r.cols() == 3);
        expect (r.rows() == 2);
        head_cells = 0;
        cells = 0;
        rows = 0;
        r.run([&head_cells] (auto const & s)
            {
                expect (s=="One" || s=="Two" || s=="Three");
                head_cells++;
            }
            ,[&cells](auto const& s)
            {
                expect (s=="1" || s=="2" || s=="3");
                ++cells;
            }
            ,[&rows]
            {
                rows++;
            }
        );
        expect (head_cells == 3);
        expect (cells == 3);
        expect (rows == 2);

    };

}

