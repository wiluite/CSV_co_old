## C++20 CSV data reader

CSV_co is a C++20 coroutine-driven, callback-providing and safe CSV data processor, reader or parser. 
Hope, the tool is to a large extent in line with standard RFC 4180, because it was conceived and has 
been developed to handle complicated field selection carefully and clearly. The following requirements 
tend to be satisfied:

- Windows and Unix style line endings.
- Optional header line.
- Each row (record) must contain the same number of fields.
- A field can be enclosed in double quotes.
- If a field contains commas, line breaks, double quotes, then this field must be enclosed in double quotes.
- The double quote character in the field must be doubled.

In fact, coroutines and callbacks do not contradict each other, but complement each other if they perform 
different tasks in the whole, and when only few callbacks intended for the end-user are.

### Version
Pre 1.0.0

### Features
- Memory-mapping of CSV files.
- Both "energetic" and "lazy" modes of bypass.
- Callbacks for each field/cell (header's or value).
- Callbacks for new rows.
- String data types only, apply lexical cast transformations yourself.
- Strong typed (concept-based) reader template parameters.

### Minimum Supported Compilers
- Linux
  - GNU GCC 10.2 C++ compiler
  - LLVM Clang 12.0 C++ compiler 
- Windows 
  - Microsoft Visual Studio 2019 Update 9 (16.9.4) +
  - Cygwin with GCC 10.2 C++ compiler
  - MinGW with GCC 10.2 C++ compiler

### Acknowledgments
To Andreas Fertig for coroutine tutorials and code that was highly borrowed.

### FAQ
    Q. Why another parser?
    A. Because I don't think some authors work correctly with quoted fields.

    Q. Why are you unquoting quoted fields?
    A. Because quotes are for data processors, not the end-user. String itself is quoted.

    Q. How fast is it?
    A. Read benchmarks chapter, please.

### Example
"Energetic" mode, iterate over all fields, general scheme:
```cpp
    #include <csv_co/reader.hpp>

    using namespace csv_co;
    using reader_type = reader< trimming_policy >;

    reader_type r( CSV_source );
    r.run([](auto & s) {
        // do something with field string
    });
```

"Energetic" mode, save all fields to a container and view a data:
```cpp
  try {
        reader_type r(std::filesystem::path("smallpop.csv"));
        std::vector<cell_string> ram;
        ram.reserve(r.cols() * r.rows());
        r.valid().run( // check validity and run
        [](auto) {
            // ignore header fields
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
```

"Energetic" mode, use 'new row' callback to facilitate filling a matrix:
```cpp
    try {
        reader<trim_policy::alltrim> r (std::filesystem::path ("smallpop.csv"));
        some_matrix_class matrix (shape);

        auto c_row {-1}; // will be incremented automatically
        auto c_col {0u};

        // ignore header fields, obtain value fields, and trace rows:
        r.run([](auto) {}
              ,[&](auto & s){ matrix[c_row][c_col++] = s; } 
              ,[&]{ c_row++; c_col = 0; });

        // population of Southborough,MA
        std::cout << matrix[0][0] << ',' << matrix[0][1] << ':' << matrix[0][3] << '\n';
    } catch (reader_type::exception const & e) {/* handler */}
```
However, in this mode parser works hard to select, prepare and provide every field. This is
somewhat more time-consuming, especially if you are interested in some specific fields and 
in common would prefer to move forward more quickly. There is a lazier option for field 
traversal, where the parser keeps for you the memory span corresponding to the current field
and gives you the opportunity to get the value of this field in the container you provide.


### Problems

### Benchmarks

