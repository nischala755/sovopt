# Benchmark evidence and acceptance status

All measurements below were produced locally on the documented MSVC Release
build. Reference objectives come from the Netlib LP collection. Downloaded
benchmark files live under ignored build output and are not repackaged here.

| Instance | Reference objective | AstraNiti result | Iterations | Verification |
|---|---:|---:|---:|---|
| AFIRO | -464.75314286 | -464.7531428571429 | 49 | Passed |
| SC50A | -64.575077059 | -64.5750770585645 | 55 | Certificate postsolve failed |
| SC50B | -70 | No terminal claim | 100,000 | Iteration limit |

AFIRO demonstrates successful execution on a recognised benchmark and agrees
with the published objective. SC50A and SC50B are retained as transparent
release blockers for any claim of broad Netlib coverage or industrial numerical
robustness. SC50A finds the reference primal objective but correctly refuses to
label it verified because its reconstructed dual certificate is invalid. SC50B
exposes unresolved degeneracy/cycling behavior.

No established external solver was installed or invoked in the production path.
A fair performance comparison still requires an isolated benchmark runner,
pinned solver/version, identical model semantics, common hardware and repeated
wall-clock measurements. Until that exists, this project makes no competitive
performance claim.

The official Netlib directory identifies these as compressed MPS LP problems:
<https://www.netlib.org/lp/data/>. Objective references were checked against the
Netlib-derived published table linked from the project research notes.
