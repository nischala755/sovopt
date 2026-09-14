# Supported MPS dialect

The reader imports linear model data only. It never calls an optimization engine.
The format is selected explicitly; it does not guess based on spacing.

## Layout and sections

Section headers begin in column 1; data begins with whitespace. Supported order:
`NAME`, optional `OBJSENSE`/`OBJNAME`, `ROWS`, `COLUMNS`, optional `RHS`, optional
`RANGES`, optional `BOUNDS`, `ENDATA`. Headers cannot repeat. `NAME` may omit a model
name. Exactly one `N` row is required as the objective. Other row types are `L`,
`G`, `E`. `OBJNAME` checks the identity of the sole `N` row; selecting among
multiple `N` rows is unsupported. Sense defaults to minimization;
`OBJSENSE` takes `MIN` or `MAX`. Objective metadata can be inline or on the next
indented line. An omitted RHS is zero. Blank lines and column-1 `*` comments are
accepted, including after `ENDATA`; other trailing content is rejected.

Free mode uses whitespace-delimited names and numeric values. COLUMNS, RHS and
RANGES records have an explicit column/vector name and one or two row/value pairs.
`$` comments in the row-name positions of COLUMNS/RHS/RANGES are accepted in free
mode. Dollar-prefixed column/vector identifiers are preserved. A row name starting
with `$` cannot be referenced in these free-mode fields because it denotes a comment.
Blank name continuations
require fixed mode. Fixed mode uses fields at columns 2–3, 5–12, 15–22, 25–36,
40–47 and 50–61. Blank column names continue the previous column. Blank vector
names continue the selected vector or identify the default unnamed vector. Tabs,
nonblank separators and trailing fixed-card sequence text are rejected.

Input is ASCII; names are not truncated. Numbers accept ordinary decimal/scientific
notation, a leading sign, and `D`/`d` exponents. Nonfinite values, out-of-range
conversions, malformed numbers and numeric aggregation overflow are errors.
Files are opened in binary mode so Windows Ctrl-Z cannot hide trailing data;
CRLF and LF line endings are handled explicitly.
Duplicates in COLUMNS are summed in input order within each coordinate, including
objective entries; exact zero sums disappear from the sparse matrix. Explicit
zero entries still declare their variable. Row/column encounter order is preserved.

## Bounds and ranges

Each constraint is represented as `lower <= A[row] x <= upper`. Before ranges:
`L` gives `[-inf,rhs]`, `G` gives `[rhs,+inf]`, `E` gives `[rhs,rhs]`.
For range value `r`: `L` sets lower to `rhs-|r|`, `G` sets upper to `rhs+|r|`,
positive `E` sets upper to `rhs+r`, negative `E` sets lower to `rhs+r`.
Zero ranges are valid equalities. The RHS of the objective row is negated to
produce the additive objective offset. These rules follow the published
[MPS record reference](https://www.ibm.com/docs/en/cofz/12.9.0?topic=standard-records-in-mps-format).

Continuous variables default to `[0,+inf]`. Supported BOUNDS records are:

| Type | Meaning |
|---|---|
| `LO` / `UP` | Finite lower / upper bound |
| `FX` | Both bounds equal the value |
| `FR` | Both bounds infinite with correct orientation |
| `MI` / `PL` | Lower `-inf` / upper `+inf` |
| `BV` | Binary domain and bounds `[0,1]` |
| `LI` / `UI` | Integer domain and integral lower / upper bound |

`INTORG`/`INTEND` marker regions declare integer variables with default `[0,1]`
bounds. Use explicit `UP`, `UI` or `PL` for a general integer upper bound.
This convention varies across readers; this implementation deliberately follows
the [documented zero-one marker convention](https://docs.mosek.com/latest/juliaapi/mps-format.html).
An integer declared only through `LI`/`UI` starts from continuous default bounds
before the record is applied. Integer representation does not imply MILP solving.

Each lower and upper bound may be assigned only once. `FX`, `FR` and `BV` assign
both. A negative upper bound with no explicit lower bound implies lower `-inf`;
an upper bound of zero preserves lower zero. Binary intervals must lie in `[0,1]`.
Contradictory bounds and discrete intervals containing no integer are rejected.

## Explicit limitations

- Multiple RHS, RANGES or BOUNDS vectors are rejected, not silently selected.
- Multiple free/objective rows, multiple objectives, quadratic or nonlinear data,
  SOS, indicators, semi-continuous and semi-integer extensions are rejected.
- No implicit exponents, compressed files, LP-format files, fixed-field inline
  comments or card sequence-number fields.
- Validation checks original data structure and impossible constant rows; it does
  not determine general feasibility.
- Parser errors carry the input line. Post-import validation/aggregate errors use
  the final input line; file-open errors use line 0.
- Line/entry limits bound individual lines and COLUMNS data, not total row-name
  storage or process memory. This is a local importer, not a hostile-input sandbox.

The linked documents are format references only. No vendor implementation,
library or executable is used by Sovereign.
