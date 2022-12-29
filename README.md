[UNDER CONSTRUCTION]

## C++20 CSV data reader

CSV_co is a C++20 coroutine-based and driven, callback-providing CSV data processor, reader or parser. Hope, the tool is to a large extent in line with standard RFC 4180, as the following requirements seem satisfied:

- MS-DOS (and Unix) style line endings.
- Optional header line.
- Each row (record) must contain the same number of fields.
- A field can be enclosed in double quotes.
- If a field contains commas, line breaks, double quotes, then this field must be enclosed in double quotes.
- The double quote character in the field must be doubled.

### Version
...

### Features
- Currently only energetic (not lazy) mode of "iteration".
- Callbacks for each field (or cell in CSV_co's terminology).
- Callbacks for new rows.
- String data types only, apply lexical cast transformations yourself.

### Acknowledgments
To Andreas Fertig for his coroutine tutorials and code that was highly borrowed.

### FAQ
    Q. Why are you unboxing/unquoting fields in quotes?
    A. Because they are for the data processor, not the end-user. The nature of the string type itself is quoting.

### Problems
Not that fast parser for now, but give a try.  Benchmarks will be available a bit later.

### Example

