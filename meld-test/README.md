# Meld Testing Library

A fluent testing framework written in pure Meld. Inspired by AssertJ, Mockito, and Cucumber.

## Library Files

Located at `meld-core/std/test/`:

| File | What |
|------|------|
| `assert.meld` | AssertJ-style fluent assertions: `assert-that(42).is-equal-to(42)` |
| `mock.meld` | Mock creation and verification: `verify(db, "query").was-called()` |
| `bdd.meld` | BDD scenarios: `scenario("login").given("user").when("login").then("success")` |
| `property.meld` | Property-based testing: generators and shrinkers |
| `core.meld` | Test suite management and reporting |
| `runner.meld` | Test discovery and execution |

## Usage

```meld
imp std.test.assert

// Fluent assertions — multiple dispatch picks the right type
assert-that(42).is-equal-to(42)
assert-that(age).is-greater-than(18).is-less-than(100)
assert-that(true).is-true()
assert-that("hello").is-not-empty()
assert-that(3.14).is-positive()
```

## Design

- **Pure Meld** — no C++ dependencies
- **Multiple dispatch** — one `assert-that()` function, type determines the assertion chain
- **Fluent chaining** — every assertion returns `self` for composition
- **Methods in type bodies** — assertions defined via type extension blocks
