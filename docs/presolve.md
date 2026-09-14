# Conservative presolve

`presolve(model, tolerances)` copies the input and returns a reduced model,
original column and row indices, original-sized fixed values (NaN marks surviving
variables), and a readable transformation journal. `restore(reduced_primal)`
reconstructs original primal coordinates and rejects wrong dimensions or nonfinite
values. It does not itself certify primal feasibility.

An optional third argument, `tighten_singletons=false`, disables singleton bound
changes for integrations that do not yet reconstruct their dual multipliers.

The bounded reduction loop substitutes exactly equal finite variable bounds,
shifts the objective offset and row bounds, removes feasible constant rows,
detects contradictory constant rows, and tightens bounds from singleton rows.
Integer and binary bounds use ceiling and floor. Singleton rows remain in the
model so their identities are available for later certificate reconstruction.
Each repeated pass requires a newly fixed column, so at most the number of
columns plus one full passes are needed.

No coefficient is removed using a tolerance, and close variable bounds are never
treated as fixed. Fused multiply-add computes row shifts with one rounding;
shifted and divided bounds expand outward by one ULP before integer rounding.
This can retain a redundant row or miss a reduction at floating-point limits.
Any finite arithmetic overflow throws `std::overflow_error`; malformed models
throw `InvalidModelError`. Empty integer domains and contradictory constant rows
return `infeasible`. Tolerances are validated but are not used to weaken these
structural decisions. Objective arithmetic is checked finite floating-point
arithmetic, not an exact rational calculation.

Reduced solutions must still be verified against the original model. The row
mapping alone does not reconstruct dual multipliers after singleton tightening;
the journal is descriptive, not a complete dual postsolve algorithm.
