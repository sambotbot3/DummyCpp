# Multifile Known-Fail Example

This example is intentionally beyond the current Dpp bootstrap subset. It should
compile and run as ordinary C++11, then fail before full generated-C parity.

Run it with:

```sh
scripts/run_multifile_known_fail_example.sh
```

Expected Dpp gaps exercised here:

- Function templates that require unsupported parameter/container lowering.
- `std::map<std::string, T>` with iteration.
- Nested standard-library containers.
- `auto`, range-for loops, references from iterators, and lambda comparators.
- Cross-file use of templated helpers and richer standard-library APIs.
