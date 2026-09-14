# Mathematical algorithms

## Presolve

Presolve records original row and column indices plus exact fixed values. Fixed
variables are substituted into every affected row and the objective using checked
floating-point arithmetic. Constant rows are removed only when zero is inside the
shifted interval; contradictions return an infeasible presolve result. Singleton
rows can tighten variable bounds, with directed outward rounding and integral
ceil/floor propagation. The LP solver disables singleton tightening while producing
dual certificates because the current postsolve map does not attribute derived
bounds to their source rows. Original primal coordinates are always reconstructed
before verification.

## Revised simplex

The LP relaxation is transformed to equality standard form without changing the
original model. Lower-bounded variables are shifted, upper-only variables are
reflected, and free variables split into a positive and negative part. Finite
upper bounds become explicit rows. Each inequality receives a slack or surplus.
Rows and columns are scaled by measured coefficient magnitudes when enabled.

Phase I adds one artificial variable per row and minimizes their sum. Revised
simplex computes the basic solution by solving `B x_B = b`, dual prices from
`B^T y = c_B`, and reduced costs `c_j - a_j^T y`. Bland's first eligible entering
column and a deterministic minimum-ratio test prevent classical cycling. Every
pivot refactorizes the sparse basis; this is intentionally conservative and is
not a performance claim. Phase I removes zero artificial basics only after an
exact-zero dependency check. A small nonzero candidate below the pivot threshold
causes numerical failure, never row deletion.

`SparseBasis` stores LU factors as sparse ordered rows with partial row pivoting.
It never allocates a dense square matrix. The current implementation refactorizes
after each pivot; eta updates and Forrest–Tomlin updates are future performance
work. Phase II excludes artificial variables, optimizes the original objective,
and returns a primal point plus dual multipliers or an improving recession ray.

## Certificates and numerical policy

All terminal mathematical claims are rechecked against the original model by an
independent module that does not use basis calculations. Primal checks cover row
and variable bounds, objective consistency, finite arithmetic and optionally
integrality. Optimality checks nonnegative bound multipliers, complementary
slackness, stationarity and the primal-dual gap. Infeasibility requires a positive
Farkas bound. Unboundedness requires a feasible point, strict recession signs for
every finite bound, and a strictly improving objective direction.

Certificate dot products use floating expansions and fused multiply-add product
errors. Any nonzero stationarity residual contributes its infimum over an
independently bounded variable domain to the conservative dual bound. A residual
that points toward an infinite endpoint invalidates the certificate even when it
is below a usual numerical tolerance. Singleton rows independently derive safe
outward-rounded domains for this correction. Arithmetic uncertainty, non-nearest
rounding, exponent underflow risk, excessive pivot residuals or failed verification
produce `numerical_failure`.

This is numerical verification over IEEE binary64 inputs, not an exact rational
proof. The distinction matters: a verified result satisfies the documented scaled
tolerances and conservative-bound rules for the stored model.

## Branch-and-bound

MILP uses the independently implemented LP solver at every node. A deterministic
best-bound queue orders normalized minimization bounds and then node IDs. Only a
verified LP dual bound can prune a node. Incumbents are rounded or repaired by an
LP with fixed integer coordinates and then checked for original-model feasibility,
objective consistency and integrality.

Branching supports most-fractional and learned up/down pseudo-cost scores. Child
bounds use floor and ceil of the relaxation point. Pure-integer rows with exactly
integral coefficients admit safe row-bound strengthening: finite upper bounds are
floored and lower bounds ceiled with an outward ULP guard. This modular initial
cut pass is deliberately limited; it is not a Gomory or MIR implementation.

Node, time, iteration and cancellation limits preserve the incumbent and best
known open-node bound. MIP gap is measured in normalized objective space. When
all variables and objective coefficients are integral, a conservative LP bound is
rounded onto the proven objective lattice, preventing endless branching caused by
sub-ULP gaps. LP unboundedness establishes MILP unboundedness only if an integer-
feasible base point and integral integer-coordinate recession ray independently
verify.
