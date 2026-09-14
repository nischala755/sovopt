# Solver completion roadmap

The remaining SIH scope is a multi-increment numerical-computing program. Work
is ordered so every commit leaves the existing solver usable and every claim is
backed by executable evidence.

1. Add a convex quadratic model and independent KKT verifier.
2. Add an infeasible-start primal-dual interior-point solver for LP/QP models.
3. Extend industrial model generators and test their mathematical structure.
4. Extend branch-and-cut interfaces with independently safe separators,
   branching policies, heuristics, and deterministic parallel coordination.
5. Add sparse factor update/refactorization policy and numerical telemetry.
6. Add synthetic sparse, degenerate, weak-relaxation, and ill-conditioned
   benchmark manifests at increasing sizes.
7. Extend Netlib, MIPLIB, and Mittelmann runners and retain failed cases.
8. Run the isolated reference adapter only when an executable is installed.
9. Run CUDA comparisons only on a detected NVIDIA CUDA device.
10. Refresh the README and requirements traceability from measured results.

Each algorithm is introduced by a failing hand-verifiable test, followed by
targeted and full-suite verification. External solvers remain outside the
production solver path.
