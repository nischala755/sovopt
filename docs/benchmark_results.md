# Benchmark evidence and acceptance status

Measured on 2026-09-14 using the MSVC 19.51 Release build on Windows 11. The
checked-in manifest verifies downloaded archives with SHA-256 before execution.
Times below are end-to-end harness wall times; solver time excludes process startup.

| Instance | Source | Published objective | AstraNiti objective | Pivots/nodes | Wall time | Verification |
|---|---|---:|---:|---:|---:|---|
| AFIRO | Netlib | -464.75314286 | -464.75314285714285 | 47 / 0 | 0.0802 s | Passed |
| SC50A | Netlib | -64.575077059 | -64.5750770585645 | 55 / 0 | 0.0670 s | Passed |
| SC50B | Netlib | -70 | -70 | 52 / 0 | 0.0207 s | Passed |
| flugpl | MIPLIB | 1201500 | no terminal claim | 100000 / 272 | 3.506 s | Iteration limit |

SC50B previously exhausted 100,000 simplex iterations. Harris two-pass ratio
selection now reaches the published primal objective in 52 pivots. Conservative
general-row implied-domain propagation allows both SC50 certificates to be
checked in original coordinates without accepting multipliers on infinite bounds.

The MIPLIB `flugpl` failure is retained: its tree exhausted the global LP-iteration
budget at 272 nodes with best bound 1178114.9999999998 and no verified incumbent.
MIPLIB-scale MILP robustness therefore remains incomplete.

No `highs`, `cbc`, `scip`, or `glpsol` executable was found on this host. The
adapter in `benchmarks/baselines/` reports unavailable rather than inventing a
comparison. External executables remain outside the AstraNiti production path.
