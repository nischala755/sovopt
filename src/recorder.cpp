#include <sovereign/recorder.hpp>
#include <sovereign/errors.hpp>
#include <sovereign/lp.hpp>
#include <sovereign/mip.hpp>
#include <sovereign/mps.hpp>
#include <sovereign/verification.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace sovereign {
namespace {
constexpr std::array<const char*,10> artifacts{"manifest.json","model.mps","model_fingerprint.json",
    "solver_config.json","presolve.json","telemetry.json","timeline.json","solution.json",
    "certificate.json","verification.json"};
constexpr const char* quadratic_artifact="quadratic.json";

std::string quote(std::string_view value) {
    std::ostringstream out; out<<'"';
    for (const unsigned char c:value) {
        if(c=='"'||c=='\\') out<<'\\'<<static_cast<char>(c);
        else if(c=='\n') out<<"\\n"; else if(c=='\r') out<<"\\r"; else if(c=='\t') out<<"\\t";
        else if(c<0x20) out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;
        else out<<static_cast<char>(c);
    }
    return out<<'"',out.str();
}
void number(std::ostream& out,double value) { if(std::isfinite(value)) out<<value; else out<<"null"; }
void array(std::ostream& out,std::span<const double> values) {
    out<<'['; for(Index i=0;i<values.size();++i){if(i)out<<',';number(out,values[i]);} out<<']';
}
void write_file(const std::filesystem::path& path,std::string_view contents) {
    std::ofstream out(path,std::ios::binary); if(!out) throw std::runtime_error("cannot create recorder artifact: "+path.string());
    out.write(contents.data(),static_cast<std::streamsize>(contents.size()));
    if(!out) throw std::runtime_error("cannot write recorder artifact: "+path.string());
}
std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary); if(!in) throw std::runtime_error("cannot read recorder artifact: "+path.string());
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}

constexpr std::array<std::uint32_t,64> sha_k{
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
std::uint32_t rotate(std::uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
std::string sha256(std::string_view input) {
    std::vector<std::uint8_t> data(input.begin(),input.end()); const auto bits=static_cast<std::uint64_t>(data.size())*8;
    data.push_back(0x80); while(data.size()%64!=56)data.push_back(0);
    for(int i=7;i>=0;--i)data.push_back(static_cast<std::uint8_t>(bits>>(i*8)));
    std::array<std::uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(Index offset=0;offset<data.size();offset+=64){std::array<std::uint32_t,64>w{};
        for(Index i=0;i<16;++i)w[i]=(std::uint32_t(data[offset+4*i])<<24)|(std::uint32_t(data[offset+4*i+1])<<16)|(std::uint32_t(data[offset+4*i+2])<<8)|data[offset+4*i+3];
        for(Index i=16;i<64;++i){const auto s0=rotate(w[i-15],7)^rotate(w[i-15],18)^(w[i-15]>>3);const auto s1=rotate(w[i-2],17)^rotate(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(Index i=0;i<64;++i){const auto s1=rotate(e,6)^rotate(e,11)^rotate(e,25);const auto ch=(e&f)^((~e)&g);const auto t1=hh+s1+ch+sha_k[i]+w[i];const auto s0=rotate(a,2)^rotate(a,13)^rotate(a,22);const auto maj=(a&b)^(a&c)^(b&c);const auto t2=s0+maj;hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto x:h)out<<std::setw(8)<<x;return out.str();
}
std::string field_string(const std::string& json,const std::string& key) {
    const std::regex pattern("\\\""+key+"\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");std::smatch match;
    if(!std::regex_search(json,match,pattern)) throw std::runtime_error("missing JSON string: "+key);
    return match[1].str();
}
double field_number(const std::string& json,const std::string& key) {
    const std::regex pattern("\\\""+key+"\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]*)?(?:[eE][+-]?[0-9]+)?)");std::smatch match;
    if(!std::regex_search(json,match,pattern)) throw std::runtime_error("missing JSON number: "+key);
    return std::stod(match[1].str());
}
std::vector<double> field_array(const std::string& json,const std::string& key) {
    const auto marker='"'+key+'"';auto start=json.find(marker);if(start==std::string::npos)throw std::runtime_error("missing JSON array: "+key);
    start=json.find('[',start);const auto end=json.find(']',start);if(start==std::string::npos||end==std::string::npos)throw std::runtime_error("invalid JSON array: "+key);
    std::vector<double> result;std::string body=json.substr(start+1,end-start-1);std::stringstream in(body);std::string token;
    while(std::getline(in,token,',')){if(token.empty())continue;result.push_back(std::stod(token));}return result;
}
std::string quadratic_json(const CscMatrix& matrix) {
    std::vector<double> rows,columns,values;rows.reserve(matrix.nonzeros());columns.reserve(matrix.nonzeros());values.reserve(matrix.nonzeros());
    for(Index j=0;j<matrix.columns();++j){const auto column=matrix.column(j);for(Index k=0;k<column.rows.size();++k){rows.push_back(static_cast<double>(column.rows[k]));columns.push_back(static_cast<double>(j));values.push_back(column.values[k]);}}
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<"{\"rows\":"<<matrix.rows()<<",\"columns\":"<<matrix.columns()<<",\"row_indices\":";array(out,rows);out<<",\"column_indices\":";array(out,columns);out<<",\"values\":";array(out,values);return out<<"}\n",out.str();
}
CscMatrix read_quadratic(const std::string& json,Index expected_dimension) {
    const auto row_values=field_array(json,"row_indices"),column_values=field_array(json,"column_indices"),values=field_array(json,"values");
    if(row_values.size()!=column_values.size()||row_values.size()!=values.size())throw std::runtime_error("invalid quadratic artifact dimensions");
    const auto rows=field_number(json,"rows"),columns=field_number(json,"columns");
    if(rows!=static_cast<double>(expected_dimension)||columns!=static_cast<double>(expected_dimension))throw std::runtime_error("quadratic artifact dimensions do not match model");
    std::vector<Triplet> entries;entries.reserve(values.size());
    for(Index k=0;k<values.size();++k){if(row_values[k]<0||column_values[k]<0||row_values[k]>=rows||column_values[k]>=columns||std::floor(row_values[k])!=row_values[k]||std::floor(column_values[k])!=column_values[k])throw std::runtime_error("invalid quadratic coordinate");entries.push_back({static_cast<Index>(row_values[k]),static_cast<Index>(column_values[k]),values[k]});}
    return CscMatrix::from_triplets(expected_dimension,expected_dimension,std::move(entries));
}
std::string events_json(std::span<const TelemetryEvent> events) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17)<<'[';
    for(Index i=0;i<events.size();++i){if(i)out<<',';const auto&e=events[i];out<<"{\"type\":"<<quote(e.type)<<",\"elapsed_seconds\":";number(out,e.elapsed_seconds);out<<",\"iterations\":"<<e.iterations<<",\"nodes\":"<<e.nodes<<",\"objective\":";number(out,e.objective);out<<",\"best_bound\":";number(out,e.best_bound);out<<",\"mip_gap\":";number(out,e.mip_gap);out<<",\"detail\":"<<quote(e.detail)<<'}';}return out<<']',out.str();
}
}

void record_solve(const std::filesystem::path& bundle,const std::filesystem::path& model_source,
                  const Model& model,const SolverOptions& options,const SolveResult& result,
                  std::span<const TelemetryEvent> events) {
    const auto temporary=std::filesystem::path(bundle.string()+".tmp");std::filesystem::remove_all(temporary);
    if(std::filesystem::exists(bundle))throw std::runtime_error("recorder bundle already exists: "+bundle.string());
    std::filesystem::create_directories(temporary);
    try {
        write_file(temporary/"manifest.json","{\"format\":\"astraniti-flight-recorder\",\"format_version\":2,\"problem_kind\":\"linear\",\"solver_version\":\"0.1.0\"}\n");
        std::filesystem::copy_file(model_source,temporary/"model.mps");
        const auto fp=fingerprint(model);std::ostringstream fingerprint_json;fingerprint_json<<"{\"hash\":"<<quote(fp.hash_hex)<<",\"variables\":"<<fp.variables<<",\"constraints\":"<<fp.constraints<<",\"nonzeros\":"<<fp.nonzeros<<"}\n";write_file(temporary/"model_fingerprint.json",fingerprint_json.str());
        std::ostringstream config;config<<std::boolalpha<<std::setprecision(17)<<"{\"method\":"<<quote(options.method)<<",\"iteration_limit\":"<<options.iteration_limit<<",\"node_limit\":"<<options.node_limit<<",\"time_limit_seconds\":"<<options.time_limit_seconds<<",\"mip_gap\":"<<options.mip_gap<<",\"scaling\":"<<options.scaling<<",\"presolve\":"<<options.presolve<<",\"deterministic\":"<<options.deterministic<<",\"cuts\":"<<options.cuts<<",\"rounding\":"<<options.rounding<<",\"feasibility_pump\":"<<options.feasibility_pump<<",\"feasibility_pump_passes\":"<<options.feasibility_pump_passes<<",\"branching\":"<<quote(options.branching)<<",\"strong_branching_candidates\":"<<options.strong_branching_candidates<<",\"primal_tolerance\":"<<options.tolerances.primal<<",\"dual_tolerance\":"<<options.tolerances.dual<<",\"integrality_tolerance\":"<<options.tolerances.integrality<<",\"pivot_tolerance\":"<<options.tolerances.pivot<<"}\n";write_file(temporary/"solver_config.json",config.str());
        write_file(temporary/"presolve.json",std::string("{\"enabled\":")+(options.presolve?"true":"false")+",\"original_space_verification\":true}\n");
        std::ostringstream summary;summary<<"{\"iterations\":"<<result.iterations<<",\"nodes\":"<<result.nodes<<",\"runtime_seconds\":"<<std::setprecision(17)<<result.runtime_seconds<<",\"events\":"<<events.size()<<"}\n";write_file(temporary/"telemetry.json",summary.str());
        write_file(temporary/"timeline.json",events_json(events)+"\n");
        std::ostringstream solution;solution<<std::setprecision(17)<<"{\"status\":"<<quote(status_name(result.status))<<",\"objective\":";number(solution,result.objective);solution<<",\"best_bound\":";number(solution,result.best_bound);solution<<",\"mip_gap\":";number(solution,result.mip_gap);solution<<",\"primal\":";array(solution,result.primal);solution<<",\"ray\":";array(solution,result.ray);solution<<"}\n";write_file(temporary/"solution.json",solution.str());
        std::ostringstream certificate;certificate<<std::setprecision(17)<<"{\"row_lower\":";array(certificate,result.certificate.row_lower);certificate<<",\"row_upper\":";array(certificate,result.certificate.row_upper);certificate<<",\"variable_lower\":";array(certificate,result.certificate.variable_lower);certificate<<",\"variable_upper\":";array(certificate,result.certificate.variable_upper);certificate<<"}\n";write_file(temporary/"certificate.json",certificate.str());
        const auto& v=result.verification;std::ostringstream verification;verification<<std::boolalpha<<std::setprecision(17)<<"{\"passed\":"<<v.passed<<",\"objective\":";number(verification,v.objective);verification<<",\"primal_residual\":";number(verification,v.primal_residual);verification<<",\"dual_residual\":";number(verification,v.dual_residual);verification<<",\"duality_gap\":";number(verification,v.duality_gap);verification<<"}\n";write_file(temporary/"verification.json",verification.str());
        std::ostringstream checksums;checksums<<"{\"algorithm\":\"SHA-256\",\"artifacts\":{";for(Index i=0;i<artifacts.size();++i){if(i)checksums<<',';checksums<<quote(artifacts[i])<<':'<<quote(sha256(read_file(temporary/artifacts[i])));}checksums<<"}}\n";write_file(temporary/"checksums.json",checksums.str());
        std::filesystem::rename(temporary,bundle);
    } catch(...) {std::filesystem::remove_all(temporary);throw;}
}

void record_qp_solve(const std::filesystem::path& bundle,const std::filesystem::path& model_source,
                     const QuadraticModel& model,const SolverOptions& options,const QpResult& result,
                     std::span<const TelemetryEvent> events) {
    const auto temporary=std::filesystem::path(bundle.string()+".tmp");std::filesystem::remove_all(temporary);
    if(std::filesystem::exists(bundle))throw std::runtime_error("recorder bundle already exists: "+bundle.string());
    std::filesystem::create_directories(temporary);
    try {
        write_file(temporary/"manifest.json","{\"format\":\"astraniti-flight-recorder\",\"format_version\":2,\"problem_kind\":\"quadratic\",\"solver_version\":\"0.1.0\"}\n");
        std::filesystem::copy_file(model_source,temporary/"model.mps");
        const auto fp=fingerprint(model.linear);std::ostringstream fingerprint_json;fingerprint_json<<"{\"hash\":"<<quote(fp.hash_hex)<<",\"variables\":"<<fp.variables<<",\"constraints\":"<<fp.constraints<<",\"nonzeros\":"<<fp.nonzeros<<"}\n";write_file(temporary/"model_fingerprint.json",fingerprint_json.str());
        write_file(temporary/quadratic_artifact,quadratic_json(model.quadratic));
        std::ostringstream config;config<<std::boolalpha<<std::setprecision(17)<<"{\"method\":"<<quote(options.method)<<",\"iteration_limit\":"<<options.iteration_limit<<",\"time_limit_seconds\":"<<options.time_limit_seconds<<",\"scaling\":"<<options.scaling<<",\"presolve\":"<<options.presolve<<",\"deterministic\":"<<options.deterministic<<",\"primal_tolerance\":"<<options.tolerances.primal<<",\"dual_tolerance\":"<<options.tolerances.dual<<",\"pivot_tolerance\":"<<options.tolerances.pivot<<"}\n";write_file(temporary/"solver_config.json",config.str());
        write_file(temporary/"presolve.json",std::string("{\"enabled\":")+(options.presolve?"true":"false")+",\"original_space_verification\":true}\n");
        std::ostringstream summary;summary<<"{\"iterations\":"<<result.iterations<<",\"runtime_seconds\":"<<std::setprecision(17)<<result.runtime_seconds<<",\"kkt_factorizations\":"<<result.kkt_factorizations<<",\"max_kkt_nonzeros\":"<<result.max_kkt_nonzeros<<",\"events\":"<<events.size()<<"}\n";write_file(temporary/"telemetry.json",summary.str());
        write_file(temporary/"timeline.json",events_json(events)+"\n");
        std::ostringstream solution;solution<<std::setprecision(17)<<"{\"status\":"<<quote(status_name(result.status))<<",\"objective\":";number(solution,result.objective);solution<<",\"primal\":";array(solution,result.primal);solution<<",\"ray\":";array(solution,result.ray);solution<<"}\n";write_file(temporary/"solution.json",solution.str());
        std::ostringstream certificate;certificate<<std::setprecision(17)<<"{\"row_lower\":";array(certificate,result.certificate.row_lower);certificate<<",\"row_upper\":";array(certificate,result.certificate.row_upper);certificate<<",\"variable_lower\":";array(certificate,result.certificate.variable_lower);certificate<<",\"variable_upper\":";array(certificate,result.certificate.variable_upper);certificate<<"}\n";write_file(temporary/"certificate.json",certificate.str());
        const auto& v=result.verification;std::ostringstream verification;verification<<std::boolalpha<<std::setprecision(17)<<"{\"passed\":"<<v.passed<<",\"objective\":";number(verification,v.objective);verification<<",\"primal_residual\":";number(verification,v.primal_residual);verification<<",\"stationarity_residual\":";number(verification,v.stationarity_residual);verification<<",\"complementarity_residual\":";number(verification,v.complementarity_residual);verification<<",\"certificate_passed\":"<<result.certificate_verification.passed<<"}\n";write_file(temporary/"verification.json",verification.str());
        std::ostringstream checksums;checksums<<"{\"algorithm\":\"SHA-256\",\"artifacts\":{";for(Index i=0;i<artifacts.size();++i){if(i)checksums<<',';checksums<<quote(artifacts[i])<<':'<<quote(sha256(read_file(temporary/artifacts[i])));}checksums<<','<<quote(quadratic_artifact)<<':'<<quote(sha256(read_file(temporary/quadratic_artifact)))<<"}}\n";write_file(temporary/"checksums.json",checksums.str());
        std::filesystem::rename(temporary,bundle);
    } catch(...) {std::filesystem::remove_all(temporary);throw;}
}

ReplayReport replay_solve(const std::filesystem::path& bundle,bool reverify) {
    ReplayReport report;
    try {
        if(!std::filesystem::is_directory(bundle)){report.message="bundle directory does not exist";return report;}
        for(const auto* name:artifacts)if(!std::filesystem::is_regular_file(bundle/name)){report.message="missing artifact: "+std::string(name);return report;}
        if(!std::filesystem::is_regular_file(bundle/"checksums.json")){report.message="missing artifact: checksums.json";return report;}
        const auto manifest=read_file(bundle/"manifest.json");const auto version=static_cast<Index>(field_number(manifest,"format_version"));if(version!=1&&version!=flight_recorder_format_version){report.message="unsupported recorder format version";return report;}report.problem_kind=version>=2?field_string(manifest,"problem_kind"):"linear";
        if(report.problem_kind!="linear"&&report.problem_kind!="quadratic"){report.message="unsupported recorder problem kind";return report;}
        if(report.problem_kind=="quadratic"&&!std::filesystem::is_regular_file(bundle/quadratic_artifact)){report.message="missing artifact: "+std::string(quadratic_artifact);return report;}
        const auto checksums=read_file(bundle/"checksums.json");
        for(const auto* name:artifacts){const std::regex p("\\\""+std::string(name)+"\\\"\\s*:\\s*\\\"([0-9a-f]{64})\\\"");std::smatch m;if(!std::regex_search(checksums,m,p)){report.message="missing checksum: "+std::string(name);return report;}const auto actual=sha256(read_file(bundle/name));if(actual!=m[1].str()){report.tampered_artifact=name;report.expected_sha256=m[1].str();report.actual_sha256=actual;report.message="TAMPER DETECTED: "+std::string(name);return report;}}
        if(report.problem_kind=="quadratic"){const std::regex p("\\\"quadratic.json\\\"\\s*:\\s*\\\"([0-9a-f]{64})\\\"");std::smatch m;if(!std::regex_search(checksums,m,p)){report.message="missing checksum: quadratic.json";return report;}const auto actual=sha256(read_file(bundle/quadratic_artifact));if(actual!=m[1].str()){report.tampered_artifact=quadratic_artifact;report.expected_sha256=m[1].str();report.actual_sha256=actual;report.message="TAMPER DETECTED: quadratic.json";return report;}}
        report.integrity_passed=true;const auto solution=read_file(bundle/"solution.json");report.recorded_status=field_string(solution,"status");
        const auto timeline=read_file(bundle/"timeline.json");
        for(auto begin=timeline.find('{');begin!=std::string::npos;begin=timeline.find('{',begin+1)) {
            const auto end=timeline.find('}',begin);if(end==std::string::npos)break;const auto object=timeline.substr(begin,end-begin+1);
            TelemetryEvent event;event.type=field_string(object,"type");event.detail=field_string(object,"detail");
            event.elapsed_seconds=field_number(object,"elapsed_seconds");event.iterations=static_cast<Index>(field_number(object,"iterations"));event.nodes=static_cast<Index>(field_number(object,"nodes"));
            report.timeline.push_back(std::move(event));begin=end;
        }
        if(!reverify){report.message="integrity PASS";return report;}
        const auto model=report.problem_kind=="quadratic"?read_qps_file(bundle/"model.mps").linear:read_mps_file(bundle/"model.mps");
        if(fingerprint(model).hash_hex!=field_string(read_file(bundle/"model_fingerprint.json"),"hash")){report.integrity_passed=false;report.message="model fingerprint does not match stored model";return report;}
        const auto primal=field_array(solution,"primal");
        const auto certificate_json=read_file(bundle/"certificate.json");DualCertificate certificate{field_array(certificate_json,"row_lower"),field_array(certificate_json,"row_upper"),field_array(certificate_json,"variable_lower"),field_array(certificate_json,"variable_upper")};
        bool passed=false;
        if(report.problem_kind=="quadratic"){
            QuadraticModel qp{model,read_quadratic(read_file(bundle/quadratic_artifact),model.variables.size())};
            if(report.recorded_status=="optimal")passed=verify_qp_optimality(qp,primal,field_number(solution,"objective"),certificate).passed;
            else if(report.recorded_status=="infeasible")passed=verify_infeasibility(model,certificate).passed;
            else if(report.recorded_status=="unbounded")passed=verify_qp_unboundedness(qp,primal,field_array(solution,"ray")).passed;
            else {report.message="recorded terminal status is not mathematically reverifiable";return report;}
        }else{
            const bool integer=std::any_of(model.variables.begin(),model.variables.end(),[](const auto& v){return v.type!=VariableType::continuous;});VerificationReport verification;
            if(report.recorded_status=="optimal") verification=integer?verify_primal(model,primal,field_number(solution,"objective"),{},true):verify_optimality(model,primal,field_number(solution,"objective"),certificate);
            else if(report.recorded_status=="infeasible") verification=verify_infeasibility(model,certificate);
            else if(report.recorded_status=="unbounded") verification=verify_unboundedness(model,primal,field_array(solution,"ray"),{},integer);
            else {report.message="recorded terminal status is not mathematically reverifiable";return report;}passed=verification.passed;
        }
        report.reverification_passed=passed;report.message=passed?"integrity PASS; independent reverification PASS":"integrity PASS; independent reverification FAILED";
    } catch(const std::exception& error){report.message=error.what();}
    return report;
}
}
