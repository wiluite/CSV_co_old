<p align="center">
  <img height="110" src="img/csv_co.png" alt="csv_co"/>
</p>

## C++20 CSV data reader
*    About
*    [FAQ](#faq)
*    [Features](#features)
*    [Minimum Supported Compilers](#minimum-supported-compilers)
*    [Acknowledgments](#acknowledgments)
*    [Example](#example)
*    [API](#api)
*    [Problems](#problems)
*    [Benchmarks](#benchmarks)

### About
CSV_co is a C++20 coroutine-driven, callback-providing and safe CSV data reader, or parser. 
Hope, it is to a large extent in line with standard RFC 4180, because it was conceived and 
has been developed to handle with field selection carefully. The following requirements tend
to be satisfied:

- Windows and Unix style line endings.
- Optional header line.
- Each row (record) must contain the same number of fields.
- A field **can** be enclosed in double quotes.
- If a field contains commas, line breaks, double quotes, then this field **must** be enclosed in
double quotes.
- The double double_quotes character in the field must be doubled.

### FAQ
> Why another parser?

Because I am very unsure about some authors' workings on complicated CSV fields.

> Why are you unquoting quoted fields?

Because quotes are for *preprocessing*, not the end-users.

> How fast is it?

Well, it is not the fastest. But look at [Benchmarks](#benchmarks). Remember, iteration speed
dissolves in data processing times.

### Features
- Memory-mapping CSV files.
- Several CSV data sources.
- Two modes of iteration.
- Callbacks for each field/cell (header's or value).
- Callbacks for new rows.
- String data type only, apply type casts on your own.
- Strong typed (concept-based) reader template parameters.
- Tested

### Minimum Supported Compilers
- Linux
  - GNU GCC 10.2 C++ compiler
  - LLVM Clang 12.0 C++ compiler 
- Windows
  - Cygwin with GCC 10.2 C++ compiler
  - Microsoft Visual Studio 2019 Update 9 (16.9.4) + 
  - MinGW with GCC 10.2 C++ compiler

### Acknowledgments
To A. Fertig for coroutine tutorials and code that was highly borrowed.

### Example
General scheme:
```cpp
    #include <csv_co/reader.hpp>

    using namespace csv_co;
    using reader_type = reader< trimming_policy >;

    reader_type r( CSV_source );
    r.run([](auto & s) {
        // do something with field s
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

"Energetic" mode, use `new row` callback to facilitate filling a matrix:
```cpp
    try {
        reader<trim_policy::alltrim> r (std::filesystem::path("smallpop.csv"));
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

String CSV source:
```cpp
    reader (R"("William said: ""I am going home, but someone always bothers me""","Movie ""Falling down""")")
    .run([](auto & s) {
        assert(
            s == R"(William said: "I am going home, but someone always bothers me")"
            || s == R"(Movie "Falling down")"
        );
    });
```

However, in above examples parser works hard to select, prepare and provide every field.
This is somewhat more time-consuming, especially if you are interested in specific
fields and in common would prefer to move forward faster. There is an option for
lazier field iteration: the parser is keeping the memory span corresponding to the
current field and gives you the opportunity to get the value of this field in the
container you provide.

### API
Public API available:
```cpp
    using cell_string = std::basic_string<char, std::char_traits<char>, alloc<char>>;
    
    template <TrimPolicyConcept TrimPolicy = trim_policy::no_trimming
            , QuoteConcept Quote = double_quotes, DelimiterConcept Delimiter = comma_delimiter>
    class reader
    {
    public:
        // Constructors
        explicit reader(std::filesystem::path const & csv_src);
        template <template<class> class Alloc=std::allocator>
        explicit reader (std::basic_string<char, std::char_traits<char>, Alloc<char>> const & csv_src);
        explicit reader (const char * csv_src);
        // CSV_co is movable type
        reader (reader && other) noexcept = default;
        reader & operator=(reader && other) noexcept = default;
        
        // Shape
        [[nodiscard]] std::size_t cols() const noexcept;
        [[nodiscard]] std::size_t rows() const noexcept;
        
        // Validation
        [[nodiscard]] reader& valid();
        
        // Parsing
        void run(value_field_cb_t fcb, new_row_cb_t nrc=[]{}) const;
        void run(header_field_cb_t hfcb, value_field_cb_t fcb, new_row_cb_t nrc=[]{}) const;
        void run_lazy(value_field_span_cb_t fcb, new_row_cb_t nrc=[]{}) const;
        void run_lazy(header_field_span_cb_t hfcb, value_field_span_cb_t fcb, new_row_cb_t nrc=[]{}) const;
        
        // Reading the field value in within callbacks
        class cell_span
        {   ///...
        public:
            void read_value(cell_string & s) const;
        }; 
    private:
        // Callback types:
        using header_field_cb_t = std::function <void (cell_string const & value)>;
        using value_field_cb_t = std::function <void (cell_string const & value)>;
        using header_field_span_cb_t = std::function <void (cell_span const & span)>;
        using value_field_span_cb_t = std::function <void (cell_span const & span)>;
        using new_row_cb_t = std::function <void ()>;
    };
```
### Problems
Frequent coroutine switching due to current byte-parsing protocol which lead to
time-consuming overheads. Well, another approach would bring parsing clarity at
the expense of speed. That could be easily implemented sometime.

### Benchmarks
Benchmarking source is in `benchmark` folder. It measures, in lazy iteration mode, the
average execution time (after a warmup run) for `CSV_co` to memory-map the input CSV
file and iterate over every field in it.
```bash
cd benchmark
clang++ -I../include -O3 -std=c++20 -stdlib=libc++ ./lazybench.cpp
cd ..
```
#### System Details

| Type          | Value                                           |
|---------------|-------------------------------------------------|
| Processor     | Intel(R) Core(TM) i7-6700 CPU @ 3.40 GHz        |
| Installed RAM | 16 GB                                           |
| HDD/SSD       | USB hard drive                                  |
| OS            | Linux slax 5.10.92 (Debian Linux 11 (bullseye)) |
| C++ Compiler  | Clang 15.0.6                                    |


#### Results

| Dataset                                                                            | File Size | Rows    | Cols | Cells       | Time   |
|------------------------------------------------------------------------------------|-----------|---------|------|-------------|--------|
| Benchmark folder's game.csv                                                        | 2.6M      | 100000  | 6    | 600'000     | 0.012s |
| [Denver Crime Data](https://www.kaggle.com/paultimothymooney/denver-crime-data)    | 102M      | 399573  | 20   | 7'991'460   | 0.530s |
| [2015 Flight Delays and Cancellations](https://www.kaggle.com/usdot/flight-delays) | 565M      | 5819080 | 31   | 180'391'480 | 3.7s   |
