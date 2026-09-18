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
pivot updates a product-form inverse with an eta matrix. The solver performs a
fresh sparse LU after 32 accepted updates, or before rejecting a weak update
pivot, so numerical drift cannot grow without a controlled refactorization.
Phase I removes zero artificial basics only after an
exact-zero dependency check. A small nonzero candidate below the pivot threshold
causes numerical failure, never row deletion.

An optimal standard-form basis can be exported as an `LpWarmStart`. A compatible
bound-modified child first checks dual feasibility, then uses dual-simplex pivots
to repair primal infeasibility. Structurally incompatible or unsuitable bases
fall back to the cold two-phase path. Every repair pivot consumes the ordinary
iteration and time budgets and produces telemetry.

`SparseBasis` stores LU factors as sparse ordered rows with partial row pivoting.
It never allocates a dense square matrix. `UpdatedBasis` retains that factorization
and applies product-form eta solves in forward order and transpose solves in
reverse order. Controlled refactorization occurs after 32 updates. Production
sparse-LU updates such as Forrest-Tomlin remain future performance work. Phase II
excludes artificial variables, optimizes the original objective,
and returns a primal point plus dual multipliers or an improving recession ray.

An optimal `LpWarmStart` can also be converted into a sparse-basis tableau view.
Each extracted row is computed as `e_i^T B^-1 A`, checked to contain the expected
identity basis, and paired with `B^-1 b`. Column metadata records its original
variable and restoration coefficient. A column is marked as an integer lattice
only when the transformation is a single unit-sign term with an integral shift;
scaled and split integer columns are conservatively rejected for cut generation.

## Interior-point LP/QP

The infeasible-start predictor-corrector method forms the augmented Newton KKT
system for the quadratic Hessian, equality rows and the inequality diagonal
scaling. KKT entries are accumulated in sparse ordered rows, canonicalized to
CSC and factorized by the core sparse partial-pivot LU. Factorization count and
maximum assembled KKT nonzeros are returned in `QpResult`; the sparse basis solve
also performs residual refinement. Constraint canonicalization and Hessian
symmetry/positive-semidefinite validation use sparse row maps and sparse LDL
updates. Elimination fill is not yet controlled by a fill-reducing ordering, so
the QP pipeline is not yet an industrial-scale sparse implementation.

Before Newton iterations, the QP path solves a zero-objective linear feasibility
problem. A verified Phase-I Farkas certificate proves QP infeasibility because
the quadratic objective does not change the feasible set. For unboundedness it
constructs the original model's recession cone, adds one equality row per Hessian
row to enforce `Qd = 0`, and searches for an improving direction with the native
LP solver. The independent checker then verifies the feasible base, every row and
bound recession sign, strict objective improvement, and the Hessian-null residual.

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

At the root node, the deterministic feasibility pump alternates rounding with a
real auxiliary LP that minimizes the sum of absolute deviations from the integer
target. Two linear inequalities and one nonnegative deviation variable represent
each absolute value. Repeated targets trigger a bounded deterministic perturbation.
All projection pivots consume the global LP iteration and time budgets, and a
candidate becomes an incumbent only after independent verification against the
original model. The pass limit is configurable and the heuristic can be disabled.

Branching supports most-fractional and learned up/down pseudo-cost scores. Child
bounds use floor and ceil of the relaxation point. Pure-integer rows with exactly
integral coefficients admit safe row-bound strengthening: finite upper bounds are
floored and lower bounds ceiled with an outward ULP guard. Root separation also
generates single-row Chvatal-Gomory inequalities for general integer coefficients.
Variables are shifted by finite integral lower bounds, both row sides are normalized
to upper inequalities, and a bounded set of nonnegative row multipliers is tested.
Only cuts violated by the solved root LP beyond a scaled efficacy tolerance are
installed. A checked tableau row whose basic variable lies on a proven integer
lattice can additionally produce a generalized mixed-integer cut. Nonbasic
integer and continuous columns use their respective GMI coefficient functions;
slack and transformed columns are substituted through their original-coordinate
expressions. Artificial columns are fixed at zero, and any unrepresentable split
column rejects the row. Production integration installs at most one efficacious
GMI cut and performs one root reoptimization to limit numerical and iteration cost.

Node, time, iteration and cancellation limits preserve the incumbent and best
known open-node bound. MIP gap is measured in normalized objective space. When
all variables and objective coefficients are integral, a conservative LP bound is
rounded onto the proven objective lattice, preventing endless branching caused by
sub-ULP gaps. LP unboundedness establishes MILP unboundedness only if an integer-
feasible base point and integral integer-coordinate recession ray independently
verify.
