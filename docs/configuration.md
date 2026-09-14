# Configuration and logging

Configuration is opt-in through `--config PATH`. Without it, defaults match
`examples/inspect.yaml`. Command-line `--mps-format` takes precedence over the file.

| Key | Default | Accepted values |
|---|---|---|
| `log_level` | `info` | `trace`, `debug`, `info`, `warn`, `error` |
| `mps_format` | `free` | `free`, `fixed` |
| `max_line_length` | `1048576` | Positive integer fitting `size_t` |
| `max_entries` | `10000000` | Positive integer fitting `size_t` |
| `branching` | `most_fractional` | `most_fractional`, `pseudocost` |
| `time_limit` | `300` | Finite nonnegative seconds |
| `node_limit` | `10000` | Positive integer |
| `iteration_limit` | `100000` | Positive integer |
| `mip_gap` | `0` | Finite nonnegative relative gap |
| `scaling`, `presolve`, `cuts`, `rounding`, `deterministic` | `true` | `true`, `false` |
| `primal_tolerance`, `dual_tolerance` | `1e-7` | Finite value strictly between zero and one |
| `integrality_tolerance` | `1e-7` | Finite value strictly between zero and one |
| `pivot_tolerance` | `1e-12` | Finite value strictly between zero and one |

The implemented parser accepts this flat YAML scalar schema with one unique key per
line. Values are unquoted. A `#` begins a comment. Indentation, nested mappings,
sequences, quoted scalars, anchors, environment interpolation, document markers
and unknown keys are not accepted. Unsupported syntax fails rather than being
partially interpreted. Full YAML can be added later behind `read_config` without
changing the core model or parser interfaces.

Each log record is one JSON object with `level` and `message`. Messages escape
quotes, backslashes and JSON control characters. Record writes through the same
logger are serialized. Filtering suppresses levels below the configured minimum.
There is no global logger and no external logging dependency.

The CLI logs to stderr. Successful `--json` output consists of one stdout document;
errors produce no success document. Error messages are also JSON log records,
regardless of whether `--json` was requested. The caller must keep streams valid.
