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

## Isolated reference comparison — 2026-09-15

HiGHS 1.15.1 was installed under the ignored `.tools/reference` directory and
executed only by `benchmarks/baselines/run_highspy.py`. It is not imported,
linked, or invoked by any AstraNiti production target. Raw valid-JSON evidence
is stored in `benchmarks/results/reference-highs-2026-09-15.json`.

| Instance | AstraNiti iterations/nodes | HiGHS iterations/nodes | Both objectives agree |
|---|---:|---:|---|
| AFIRO | 47 / 0 | 6 / — | Yes |
| SC50A | 55 / 0 | 18 / — | Yes |
| SC50B | 52 / 0 | 18 / — | Yes |
| flugpl | 100000 / 51, no incumbent | 858 / 89, optimal | No AstraNiti terminal objective |

The new `flugpl` run used strong branching and safe root cuts. It stopped at the
same global LP-iteration ceiling with bound 1173926.1111111108 after 51 nodes.
The comparison indicates that repeated cold simplex starts and lack of basis
reoptimization dominate the search; stronger branching alone does not close the
reliability gap.

After compatible-basis inheritance and dual-simplex repair, a fresh pseudocost
`flugpl` run processed 96 nodes before the same 100,000-iteration limit, with
bound 1176026.6666666665 and no incumbent in 7.93 seconds. The failure remains:
basis reuse without sparse factor updates and stronger incumbent generation is
insufficient for this instance.

After adding product-form inverse basis updates with a 32-pivot controlled
refactorization interval, the same Release executable and pseudocost settings
processed 363 nodes in 2.90 solver seconds (3.03 seconds harness wall time). It
still reached 100,000 LP iterations without an incumbent; the reported bound was
1181497.4999999998. This is a measured throughput improvement, not a claim that
the MIPLIB reliability target is complete.

Enabling the deterministic eight-pass L1 feasibility pump did not change the
`flugpl` terminal result: no verified incumbent was found before 100,000 LP
iterations, with 363 nodes, bound 1181497.4999999998 and 2.83 solver seconds.
The heuristic is therefore demonstrated on hand-verifiable regressions, but it
does not resolve this difficult MIPLIB instance.

Efficacy-filtered single-row Chvatal-Gomory separation produced no violated cut
on the solved `flugpl` root relaxation. The measured result therefore remained
100,000 iterations, 363 nodes, no incumbent and bound 1181497.4999999998. An
initial experiment that installed every valid candidate before solving made the
root LP consume the entire budget; that approach was rejected and is not part of
the implementation.

The first production GMI round was deliberately capped at one cut after measured
four- and 32-cut rounds spent excessive iterations reoptimizing the root. With
one GMI cut, `flugpl` still reached 100,000 iterations without an incumbent after
5 nodes and 4.82 solver seconds, with bound 1169774.1730715898. The baseline with
no applicable tableau cut remains faster on this instance, so GMI is a verified
capability rather than a demonstrated `flugpl` performance improvement.

## Expanded Netlib and reference run — 2026-09-20

The checksum-pinned suite now includes SC105. Raw native and isolated HiGHS
records are in `benchmarks/results/netlib-2026-09-20.json` and
`benchmarks/results/reference-highs-2026-09-20.json`. Wall times include the
respective harness calls and are not matched-process timing comparisons.

| Instance | Native objective | Native pivots | Native wall | HiGHS objective | HiGHS pivots | HiGHS wall |
|---|---:|---:|---:|---:|---:|---:|
| AFIRO | -464.7531428571428 | 52 | 0.051 s | -464.75314285714285 | 6 | 0.004 s |
| SC50A | -64.5750770585645 | 55 | 0.086 s | -64.5750770585645 | 18 | 0.002 s |
| SC50B | -70 | 52 | 0.034 s | -70 | 18 | 0.002 s |
| SC105 | -52.202061211707246 | 136 | 0.273 s | -52.20206121170723 | 37 | 0.004 s |

All four native optimality certificates pass original-coordinate verification
and agree with the published Netlib objectives within the manifest tolerances.
On additional locally cached Netlib models, ADLITTLE stopped at 100,000 simplex
iterations without a terminal claim; BLEND parsed after accepting its descriptive
NAME card, then returned numerical failure after 82 iterations. These failures
are retained as reliability targets.

The root-diving heuristic found a verified incumbent on the small binary
regression. It did not change the MIPLIB flugpl result: with root cuts disabled,
100,000 iterations ended after 363 nodes with no incumbent and best bound
1181497.4999999998. Raising the budget to 1,000,000 iterations also stopped at
node 363, after one node consumed the extra budget. This is not evidence of
difficult-MIPLIB reliability.
