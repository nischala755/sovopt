#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sovereign/lp.hpp>
#include <sovereign/backend.hpp>
#include <sovereign/benchmark.hpp>
#include <sovereign/fingerprint.hpp>
#include <sovereign/mip.hpp>
#include <sovereign/mps.hpp>
#include <sovereign/verification.hpp>
#include <sovereign/validation.hpp>
#include <map>
#include <cmath>
#include <type_traits>

namespace py = pybind11;
using namespace sovereign;

namespace {
py::dict report(const VerificationReport& r) {
    py::dict d; d["passed"]=r.passed; d["objective"]=r.objective;
    d["primal_residual"]=r.primal_residual; d["dual_residual"]=r.dual_residual;
    d["objective_error"]=r.objective_error; d["integrality_residual"]=r.integrality_residual;
    d["dual_bound"]=r.dual_bound; d["duality_gap"]=r.duality_gap; d["violations"]=r.violations; return d;
}
py::dict result(const SolveResult& r) {
    py::dict d; d["status"]=status_name(r.status); d["message"]=r.message; d["primal"]=r.primal;
    d["ray"]=r.ray; d["objective"]=r.objective; d["best_bound"]=r.best_bound; d["mip_gap"]=r.mip_gap;
    d["runtime_seconds"]=r.runtime_seconds; d["cpu_seconds"]=r.cpu_seconds; d["gpu_kernel_seconds"]=r.gpu_kernel_seconds;
    d["transfer_seconds"]=r.transfer_seconds; d["iterations"]=r.iterations; d["nodes"]=r.nodes;
    d["nodes_generated"]=r.nodes_generated; d["incumbent_updates"]=r.incumbent_updates; d["cuts_added"]=r.cuts_added;
    py::dict cert; cert["row_lower"]=r.certificate.row_lower; cert["row_upper"]=r.certificate.row_upper;
    cert["variable_lower"]=r.certificate.variable_lower; cert["variable_upper"]=r.certificate.variable_upper;
    d["certificate"]=cert; d["verification"]=report(r.verification); return d;
}
py::dict stats(const Model& m) {
    const auto s=statistics(m); py::dict d; d["name"]=m.name; d["variables"]=s.variables; d["constraints"]=s.constraints;
    d["nonzeros"]=s.nonzeros; d["continuous_variables"]=s.continuous_variables; d["integer_variables"]=s.integer_variables;
    d["binary_variables"]=s.binary_variables; d["equality_rows"]=s.equality_rows; d["objective_nonzeros"]=s.objective_nonzeros;
    d["density"]=s.density; d["minimum_absolute_coefficient"]=s.minimum_absolute_coefficient;
    d["maximum_absolute_coefficient"]=s.maximum_absolute_coefficient; const auto f=fingerprint(m);
    d["model_hash"]=f.hash_hex; d["estimated_bytes"]=f.estimated_bytes; d["coefficient_l2_norm"]=f.coefficient_l2_norm;
    d["mean_column_nonzeros"]=f.mean_column_nonzeros; d["mean_row_nonzeros"]=f.mean_row_nonzeros; return d;
}
SolverOptions options(const py::dict& in, py::object emit=py::none()) {
    SolverOptions o;
    auto set=[&](const char* k,auto& v){ if(in.contains(k)) v=in[k].cast<std::decay_t<decltype(v)>>(); };
    set("iteration_limit",o.iteration_limit); set("node_limit",o.node_limit); set("time_limit_seconds",o.time_limit_seconds);
    set("mip_gap",o.mip_gap); set("scaling",o.scaling); set("presolve",o.presolve); set("deterministic",o.deterministic);
    set("cuts",o.cuts); set("rounding",o.rounding); set("branching",o.branching);
    if(!emit.is_none()) o.telemetry=[emit](const TelemetryEvent& e){ py::gil_scoped_acquire gil; py::dict d; d["type"]=e.type;
        d["elapsed_seconds"]=e.elapsed_seconds; d["iterations"]=e.iterations; d["nodes"]=e.nodes; d["objective"]=e.objective;
        d["best_bound"]=e.best_bound; d["mip_gap"]=e.mip_gap; d["detail"]=e.detail; emit(d); };
    return o;
}
DualCertificate certificate(const py::dict& d) { DualCertificate c;
    if(d.contains("row_lower")) c.row_lower=d["row_lower"].cast<std::vector<double>>();
    if(d.contains("row_upper")) c.row_upper=d["row_upper"].cast<std::vector<double>>();
    if(d.contains("variable_lower")) c.variable_lower=d["variable_lower"].cast<std::vector<double>>();
    if(d.contains("variable_upper")) c.variable_upper=d["variable_upper"].cast<std::vector<double>>();
    return c; }
Model formulation(const py::dict& d) {
    Model m; m.name=d["name"].cast<std::string>(); m.sense=d["sense"].cast<std::string>()=="maximize"?ObjectiveSense::maximize:ObjectiveSense::minimize;
    m.objective_offset=d["objective_offset"].cast<double>(); std::vector<Triplet> entries; py::dict objective=d["objective"];
    std::map<std::string,Index> cols; for(const auto item:d["variables"].cast<py::list>()){ py::dict v=py::reinterpret_borrow<py::dict>(item); Variable x;
      x.name=v["name"].cast<std::string>(); x.lower=v["lower"].cast<double>(); x.upper=v["upper"].cast<double>(); auto t=v["type"].cast<std::string>();
      x.type=t=="binary"?VariableType::binary:(t=="integer"?VariableType::integer:VariableType::continuous); cols[x.name]=m.variables.size(); m.variables.push_back(x); m.objective.push_back(objective.contains(x.name.c_str())?objective[x.name.c_str()].cast<double>():0); }
    Index row=0; for(const auto item:d["constraints"].cast<py::list>()){ py::dict c=py::reinterpret_borrow<py::dict>(item); Constraint x{c["name"].cast<std::string>(),c["lower"].cast<double>(),c["upper"].cast<double>()}; m.constraints.push_back(x);
      for(auto kv:c["coefficients"].cast<py::dict>()){ auto name=kv.first.cast<std::string>(); if(!cols.contains(name)) throw std::invalid_argument("unknown variable: "+name); entries.push_back({row,cols[name],kv.second.cast<double>()}); } ++row; }
    m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(entries)); require_valid(m); return m;
}
}

class NativeEngine {
public:
 py::dict capabilities(){ const auto cpu=backend(BackendKind::cpu).capabilities(),gpu=backend(BackendKind::cuda).capabilities(); py::dict d;
   d["cpu_available"]=cpu.available; d["cpu_name"]=cpu.name; d["gpu_available"]=gpu.available; d["gpu_name"]=gpu.name; d["gpu_detail"]=gpu.detail; return d; }
 py::dict inspect(const std::string& p){ return stats(read_mps_file(p)); }
 py::dict solve(const std::string& p,const std::string& kind,const py::dict& o,py::object emit){ auto m=read_mps_file(p); auto configured=options(o,emit); SolveResult r; { py::gil_scoped_release release; r=kind=="mip"?solve_mip(m,configured):solve_lp(m,configured); } return result(r); }
 py::dict verify(const std::string& p,const py::dict& request){ auto m=read_mps_file(p); auto kind=request["kind"].cast<std::string>(); auto d=request["data"].cast<py::dict>();
   if(kind=="primal") return report(verify_primal(m,d["primal"].cast<std::vector<double>>(),d["objective"].cast<double>()));
   if(kind=="optimality") return report(verify_optimality(m,d["primal"].cast<std::vector<double>>(),d["objective"].cast<double>(),certificate(d["certificate"].cast<py::dict>())));
   if(kind=="infeasibility") return report(verify_infeasibility(m,certificate(d["certificate"].cast<py::dict>())));
   const bool integer=request.contains("integer")&&request["integer"].cast<bool>();
   return report(verify_unboundedness(m,d["primal"].cast<std::vector<double>>(),d["ray"].cast<std::vector<double>>(),{},integer)); }
 py::dict benchmark(const std::string& p,const py::dict& request,py::object emit){ py::list runs; auto n=request["repetitions"].cast<int>(); for(int i=0;i<n;++i) runs.append(solve(p,request["kind"].cast<std::string>(),request["options"].cast<py::dict>(),emit)); py::dict d; d["runs"]=runs; return d; }
 py::list execution_benchmark(const std::string& p,const py::list& modes,int repetitions){ const auto model=read_mps_file(p); py::list output;
   for(const auto item:modes){ const auto name=item.cast<std::string>(); BenchmarkOptions o; o.warmup_runs=0; o.repetitions=static_cast<Index>(repetitions); o.first_order.iteration_limit=100;
     o.mode=name=="gpu"?BenchmarkMode::static_cuda:(name=="adaptive"?BenchmarkMode::adaptive:BenchmarkMode::static_cpu); const auto r=sovereign::benchmark(model,o); py::dict group; group["backend"]=name; group["model_hash"]=r.model.hash_hex; py::list records;
     for(const auto& x:r.records){ py::dict record; record["backend"]=std::string(backend_name(x.backend)); record["status"]=x.executed?(x.converged?"converged":"executed"):"unavailable"; record["wall_seconds"]=x.wall_seconds; record["cpu_seconds"]=x.cpu_seconds; if(x.gpu_kernel_seconds) record["gpu_kernel_seconds"]=*x.gpu_kernel_seconds; else record["gpu_kernel_seconds"]=py::none(); if(x.transfer_seconds) record["transfer_seconds"]=*x.transfer_seconds; else record["transfer_seconds"]=py::none(); record["iterations"]=x.iterations; if(std::isfinite(x.primal_residual)) record["primal_residual"]=x.primal_residual; else record["primal_residual"]=py::none(); record["detail"]=x.message; records.append(record); }
     group["records"]=records; output.append(group); } return output; }
 py::dict create_model(const py::dict& d){ return stats(formulation(d)); }
};

PYBIND11_MODULE(sovereign_optimizer,m){
 py::class_<Model>(m,"Model").def_property_readonly("statistics",[](const Model& model){return stats(model);});
 m.def("read_mps_file",[](const std::string& path){return read_mps_file(path);});
 m.def("solve_lp",[](const Model& model,const py::dict& o){return result(solve_lp(model,options(o)));},py::arg("model"),py::arg("options")=py::dict{});
 m.def("solve_mip",[](const Model& model,const py::dict& o){return result(solve_mip(model,options(o)));},py::arg("model"),py::arg("options")=py::dict{});
 m.def("verify_primal",[](const Model& model,const std::vector<double>& x,double objective){return report(verify_primal(model,x,objective));});
 m.def("verify_optimality",[](const Model& model,const std::vector<double>& x,double objective,const py::dict& cert){return report(verify_optimality(model,x,objective,certificate(cert)));});
 m.def("verify_infeasibility",[](const Model& model,const py::dict& cert){return report(verify_infeasibility(model,certificate(cert)));});
 m.def("verify_unboundedness",[](const Model& model,const std::vector<double>& x,const std::vector<double>& ray){return report(verify_unboundedness(model,x,ray));});
 m.def("model_from_formulation",&formulation);
 py::class_<NativeEngine>(m,"NativeEngine").def(py::init<>()).def("capabilities",&NativeEngine::capabilities).def("inspect",&NativeEngine::inspect).def("solve",&NativeEngine::solve)
 .def("verify",&NativeEngine::verify).def("benchmark",&NativeEngine::benchmark).def("execution_benchmark",&NativeEngine::execution_benchmark).def("create_model",&NativeEngine::create_model); }
