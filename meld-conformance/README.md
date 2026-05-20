# Meld Conformance Suite

Machine-verifiable contracts for Meld language behavior. Each fixture is a `.meld` file paired with a `.expected` file containing the exact expected output.

## Running

```bash
./meld-conformance/run.sh
```

## Structure

```
meld-conformance/
├── run.sh                          # Test runner
├── README.md
└── fixtures/
    ├── hello.meld                  # Test input
    ├── hello.expected              # Expected output
    ├── arithmetic.meld
    ├── arithmetic.expected
    └── ...
```

## Adding a Fixture

1. Create `fixtures/<name>.meld` with the program
2. Run it manually: `meld run fixtures/<name>.meld`
3. Capture the output to `fixtures/<name>.expected`
4. Run `./run.sh` to verify

## Categories

- **basics/** — Values, types, functions, output
- **control/** — when/then/else, forEach, map, filter, reduce, match
- **structs/** — Creation, fields, methods, operators
- **errors/** — Result, Option, unwrap, propagation
- **effects/** — Console effects, sandboxing
- **modules/** — imp, multi-file
