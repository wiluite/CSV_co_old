## C++20 CSV data reader

CSV_co is a C++20 coroutine-driven, one-pass and callback-providing CSV data processor, reader or parser. 
Hope, the tool is to a large extent in line with standard RFC 4180, because it was conceived and has been 
designed to handle field selection transparently. The following requirements seem satisfied:

- MS-DOS (and Unix) style line endings.
- Optional header line.
- Each row (record) must contain the same number of fields.
- A field can be enclosed in double quotes.
- If a field contains commas, line breaks, double quotes, then this field must be enclosed in double quotes.
- The double quote character in the field must be doubled.

In fact, coroutines and callbacks do not contradict each other, but complement each other if they perform 
different tasks in one, and when only few callbacks intended for the e-user are.

### Version
Pre 1.0.0

### Features
- Currently only energetic (not lazy) mode of iteration.
- Callbacks for each field/cell (header's or value)
- Callbacks for new rows.
- String data types only, apply lexical cast transformations yourself.
- Strong typed (concept-based) reader template parameters

### Minimum Supported Compilers
- Linux
  - GNU GCC 10.2 C++ compiler
  - LLVM Clang 12.0 C++ compiler 
- Windows 
  - Microsoft Visual Studio 2019 Update 9 (16.9.4) +
  - Cygwin with GCC 10.2 C++ compiler
  - MinGW with GCC 10.2 C++ compiler

### Acknowledgments
To Andreas Fertig for his coroutine tutorials and code that was highly borrowed.

### FAQ
    Q. Why another parser?
    A. Because I'm not sure that some authors work correctly with quoted fields.

    Q. Why are you unboxing/unquoting fields in quotes?
    A. Because they are for the data processor, not the e-user. Nature of string itself is quoting.

### Example

### Problems

### Benchmarks

