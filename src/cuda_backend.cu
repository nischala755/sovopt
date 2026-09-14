#include <sovereign/backend.hpp>
#include <sovereign/errors.hpp>
#include <sovereign/validation.hpp>
#include <cuda_runtime.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <stdexcept>

namespace sovereign::cuda_detail {
namespace {
void check(cudaError_t status, const char* operation) {
    if (status != cudaSuccess) throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
}

template<class T> class DeviceBuffer {
public:
    explicit DeviceBuffer(std::size_t count) { if (count) check(cudaMalloc(reinterpret_cast<void**>(&data_), count*sizeof(T)), "cudaMalloc"); }
    ~DeviceBuffer() { if (data_) cudaFree(data_); }
    DeviceBuffer(const DeviceBuffer&) = delete; DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    T* get() { return data_; } const T* get() const { return data_; }
private: T* data_ = nullptr;
};

struct HostCsc { std::vector<Index> offsets, rows; std::vector<double> values; };
HostCsc unpack(const CscMatrix& a) {
    HostCsc h; h.offsets.assign(a.column_offsets().begin(), a.column_offsets().end());
    h.rows.reserve(a.nonzeros()); h.values.reserve(a.nonzeros());
    for (Index j=0;j<a.columns();++j) { const auto c=a.column(j); h.rows.insert(h.rows.end(),c.rows.begin(),c.rows.end()); h.values.insert(h.values.end(),c.values.begin(),c.values.end()); }
    return h;
}

__global__ void csc_matvec(Index columns, const Index* offsets, const Index* rows,
                           const double* values, const double* x, double* y) {
    const Index j=blockIdx.x*blockDim.x+threadIdx.x;
    if(j<columns) for(Index k=offsets[j];k<offsets[j+1];++k) atomicAdd(y+rows[k],values[k]*x[j]);
}
__global__ void csc_transpose(Index columns, const Index* offsets, const Index* rows,
                              const double* values, const double* x, double* y) {
    const Index j=blockIdx.x*blockDim.x+threadIdx.x;
    if(j<columns) { double sum=0; for(Index k=offsets[j];k<offsets[j+1];++k) sum+=values[k]*x[rows[k]]; y[j]=sum; }
}

struct Product { std::vector<double> values; double kernel=0, transfer=0; };
Product product(const CscMatrix& a, std::span<const double> x, bool transpose) {
    if ((!transpose && x.size()!=a.columns()) || (transpose && x.size()!=a.rows())) throw InvalidModelError("matrix-vector dimension mismatch");
    const auto h=unpack(a); const Index output=transpose?a.columns():a.rows();
    DeviceBuffer<Index> offsets(h.offsets.size()), rows(h.rows.size()); DeviceBuffer<double> values(h.values.size()), dx(x.size()), dy(output);
    const auto transfer_start=std::chrono::steady_clock::now();
    check(cudaMemcpy(offsets.get(),h.offsets.data(),h.offsets.size()*sizeof(Index),cudaMemcpyHostToDevice),"copy offsets");
    if(!h.rows.empty()) { check(cudaMemcpy(rows.get(),h.rows.data(),h.rows.size()*sizeof(Index),cudaMemcpyHostToDevice),"copy rows"); check(cudaMemcpy(values.get(),h.values.data(),h.values.size()*sizeof(double),cudaMemcpyHostToDevice),"copy values"); }
    if(!x.empty()) check(cudaMemcpy(dx.get(),x.data(),x.size()*sizeof(double),cudaMemcpyHostToDevice),"copy vector");
    if(output) check(cudaMemset(dy.get(),0,output*sizeof(double)),"clear output");
    Product result; result.transfer=std::chrono::duration<double>(std::chrono::steady_clock::now()-transfer_start).count(); result.values.resize(output);
    cudaEvent_t start{},stop{}; check(cudaEventCreate(&start),"create event"); check(cudaEventCreate(&stop),"create event");
    check(cudaEventRecord(start),"record event"); const unsigned threads=256; const unsigned blocks=static_cast<unsigned>((a.columns()+threads-1)/threads);
    if(blocks) { if(transpose) csc_transpose<<<blocks,threads>>>(a.columns(),offsets.get(),rows.get(),values.get(),dx.get(),dy.get()); else csc_matvec<<<blocks,threads>>>(a.columns(),offsets.get(),rows.get(),values.get(),dx.get(),dy.get()); }
    check(cudaGetLastError(),"launch sparse kernel"); check(cudaEventRecord(stop),"record event"); check(cudaEventSynchronize(stop),"wait sparse kernel"); float milliseconds=0; check(cudaEventElapsedTime(&milliseconds,start,stop),"time sparse kernel"); result.kernel=milliseconds/1000.0; cudaEventDestroy(start);cudaEventDestroy(stop);
    const auto download=std::chrono::steady_clock::now(); if(output) check(cudaMemcpy(result.values.data(),dy.get(),output*sizeof(double),cudaMemcpyDeviceToHost),"copy result"); result.transfer+=std::chrono::duration<double>(std::chrono::steady_clock::now()-download).count(); return result;
}
double primal_residual(const Model& m,std::span<const double> x,const std::vector<double>& activity){ double r=0;for(Index i=0;i<activity.size();++i){if(std::isfinite(m.constraints[i].lower))r=std::max(r,m.constraints[i].lower-activity[i]);if(std::isfinite(m.constraints[i].upper))r=std::max(r,activity[i]-m.constraints[i].upper);}for(Index j=0;j<x.size();++j){if(std::isfinite(m.variables[j].lower))r=std::max(r,m.variables[j].lower-x[j]);if(std::isfinite(m.variables[j].upper))r=std::max(r,x[j]-m.variables[j].upper);}return std::max(0.0,r);}
}

BackendCapabilities capabilities() {
    int count=0; const auto status=cudaGetDeviceCount(&count); if(status!=cudaSuccess||count==0) return {BackendKind::cuda,"CUDA",true,false,0,0,status==cudaSuccess?"no CUDA device detected":cudaGetErrorString(status)};
    cudaDeviceProp property{}; if(cudaGetDeviceProperties(&property,0)!=cudaSuccess) return {BackendKind::cuda,"CUDA",true,false,0,0,"CUDA device query failed"};
    return {BackendKind::cuda,property.name,true,true,static_cast<Index>(property.multiProcessorCount),property.totalGlobalMem,"CUDA sparse kernels available"};
}
std::vector<double> multiply(const CscMatrix& a,std::span<const double> x,bool transpose){if(!capabilities().available)throw InvalidModelError("CUDA runtime is unavailable");return product(a,x,transpose).values;}

NumericalResult solve_relaxation(const Model& m,const FirstOrderOptions& o) {
    require_valid(m); if(!o.iteration_limit||o.primal_step<=0||o.dual_step<=0||o.tolerance<0)throw InvalidModelError("invalid first-order options");
    const auto wall=std::chrono::steady_clock::now();const auto cpu=std::clock();const Index n=m.variables.size(),rows=m.constraints.size();std::vector<double>x(n,0),avg(n,0),lo(rows,0),up(rows,0);for(Index j=0;j<n;++j){if(std::isfinite(m.variables[j].lower))x[j]=std::max(x[j],m.variables[j].lower);if(std::isfinite(m.variables[j].upper))x[j]=std::min(x[j],m.variables[j].upper);}NumericalResult result;result.backend=BackendKind::cuda;result.executed=true;double kernel=0,transfer=0,stationarity=infinity;auto initial=product(m.matrix,x,false);kernel+=initial.kernel;transfer+=initial.transfer;result.telemetry.initial_primal_residual=primal_residual(m,x,initial.values);const double sense=m.sense==ObjectiveSense::minimize?1:-1;
    for(Index it=0;it<o.iteration_limit;++it){auto ax=product(m.matrix,x,false);kernel+=ax.kernel;transfer+=ax.transfer;for(Index i=0;i<rows;++i){if(std::isfinite(m.constraints[i].lower))lo[i]=std::max(0.0,lo[i]+o.dual_step*(m.constraints[i].lower-ax.values[i]));if(std::isfinite(m.constraints[i].upper))up[i]=std::max(0.0,up[i]+o.dual_step*(ax.values[i]-m.constraints[i].upper));}std::vector<double>d(rows);for(Index i=0;i<rows;++i)d[i]=up[i]-lo[i];auto at=product(m.matrix,d,true);kernel+=at.kernel;transfer+=at.transfer;stationarity=0;const double step=o.primal_step/std::sqrt(1.0+double(it));for(Index j=0;j<n;++j){const double g=at.values[j]+sense*m.objective[j];stationarity=std::max(stationarity,std::abs(g));x[j]-=step*g;if(std::isfinite(m.variables[j].lower))x[j]=std::max(x[j],m.variables[j].lower);if(std::isfinite(m.variables[j].upper))x[j]=std::min(x[j],m.variables[j].upper);avg[j]+=x[j];}result.telemetry.iterations=it+1;auto updated=product(m.matrix,x,false);kernel+=updated.kernel;transfer+=updated.transfer;if(primal_residual(m,x,updated.values)<=o.tolerance&&stationarity<=o.tolerance){result.converged=true;break;}}
    if(o.average_iterates&&result.telemetry.iterations)for(Index j=0;j<n;++j)avg[j]/=double(result.telemetry.iterations);else avg=x;auto final=product(m.matrix,avg,false);kernel+=final.kernel;transfer+=final.transfer;result.primal=std::move(avg);result.row_activity=std::move(final.values);result.telemetry.final_primal_residual=primal_residual(m,result.primal,result.row_activity);result.telemetry.stationarity_residual=stationarity;result.telemetry.objective=m.objective_offset;for(Index j=0;j<n;++j)result.telemetry.objective+=m.objective[j]*result.primal[j];result.telemetry.wall_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-wall).count();result.telemetry.cpu_seconds=double(std::clock()-cpu)/CLOCKS_PER_SEC;result.telemetry.gpu_kernel_seconds=kernel;result.telemetry.transfer_seconds=transfer;result.message=result.converged?"experimental CUDA iteration tolerance reached":"experimental CUDA iteration limit reached";return result;
}
}
