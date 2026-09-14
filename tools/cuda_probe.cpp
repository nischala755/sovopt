#include <sovereign/backend.hpp>
#include <sovereign/benchmark.hpp>
#include <sovereign/model.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
using namespace sovereign;
namespace {
Model banded(Index n){Model m;m.name="cuda_banded_"+std::to_string(n);m.variables.reserve(n);m.objective.assign(n,0);m.constraints.reserve(n);std::vector<Triplet>e;e.reserve(3*n);for(Index i=0;i<n;++i){m.variables.push_back({"x"+std::to_string(i),-1,1});m.constraints.push_back({"r"+std::to_string(i),-3,3});for(Index j=i>0?i-1:i;j<std::min(n,i+2);++j)e.push_back({i,j,1.0/(1+std::abs(static_cast<double>(i)-static_cast<double>(j)))});}m.matrix=CscMatrix::from_triplets(n,n,std::move(e));return m;}
double error(const std::vector<double>&a,const std::vector<double>&b){if(a.size()!=b.size())return infinity;double r=0;for(Index i=0;i<a.size();++i)r=std::max(r,std::abs(a[i]-b[i])/(1+std::abs(a[i])));return r;}
}
int main(int argc,char**argv){try{Index n=argc>1?static_cast<Index>(std::stoull(argv[1])):100000;Index iterations=argc>2?static_cast<Index>(std::stoull(argv[2])):100;const auto cap=backend(BackendKind::cuda).capabilities();std::cout<<std::boolalpha<<std::setprecision(17)<<"{\"compiled\":"<<cap.compiled<<",\"available\":"<<cap.available<<",\"device\":\""<<cap.name<<"\",\"multiprocessors\":"<<cap.workers<<",\"device_memory_bytes\":"<<cap.device_memory_bytes<<",\"detail\":\""<<cap.detail<<"\"";if(!cap.available){std::cout<<"}\n";return 2;}auto model=banded(n);std::vector<double>x(n);for(Index i=0;i<n;++i)x[i]=std::sin(static_cast<double>(i));const auto cpu=backend(BackendKind::cpu).multiply(model.matrix,x);const auto gpu=backend(BackendKind::cuda).multiply(model.matrix,x);const double mismatch=error(cpu,gpu);std::cout<<",\"variables\":"<<n<<",\"nonzeros\":"<<model.matrix.nonzeros()<<",\"matvec_relative_error\":"<<mismatch;if(!(mismatch<=1e-11)){std::cout<<",\"verified\":false}\n";return 3;}FirstOrderOptions first;first.iteration_limit=iterations;BenchmarkOptions o;o.warmup_runs=1;o.repetitions=5;o.first_order=first;std::cout<<",\"cpu\":";o.mode=BenchmarkMode::static_cpu;std::cout<<benchmark_json(benchmark(model,o));std::cout<<",\"gpu\":";o.mode=BenchmarkMode::static_cuda;std::cout<<benchmark_json(benchmark(model,o));std::cout<<",\"adaptive\":";o.mode=BenchmarkMode::adaptive;std::cout<<benchmark_json(benchmark(model,o));std::cout<<",\"verified\":true}\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 4;}}
