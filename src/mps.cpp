#include <sovereign/mps.hpp>
#include <sovereign/errors.hpp>
#include <sovereign/validation.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace sovereign {
namespace {
std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : s.substr(first,s.find_last_not_of(" \t\r\n") - first + 1);
}
std::vector<std::string> words(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> result;
    for (std::string word; in >> word;) {
        result.push_back(std::move(word));
    }
    return result;
}
enum class Section { none, name, sense, objective, rows, columns, rhs, ranges, bounds, quadratic, end };
class Reader {
public:
    explicit Reader(const MpsOptions& options,bool allow_quadratic) : options_(options),allow_quadratic_(allow_quadratic) {}
    QuadraticModel read(std::istream& input);
private:
    MpsOptions options_;
    bool allow_quadratic_ = false;
    Index line_ = 0, entry_count_ = 0;
    Section section_ = Section::none;
    int rank_ = 0;
    bool integer_region_ = false, sense_read_ = false, objective_read_ = false;
    bool rows_seen_ = false, columns_seen_ = false;
    Model model_;
    std::string objective_name_, requested_objective_, previous_column_;
    std::unordered_map<std::string,Index> row_indices_, column_indices_;
    std::vector<char> row_types_;
    std::vector<double> rhs_;
    std::vector<std::optional<double>> ranges_;
    std::vector<Triplet> entries_;
    std::vector<Triplet> quadratic_entries_;
    std::unordered_set<std::string> quadratic_pairs_;
    struct BoundsSeen { bool lower = false; bool upper = false; };
    std::vector<BoundsSeen> bounds_seen_;
    std::optional<std::string> rhs_name_, range_name_, bound_name_;
    std::unordered_set<std::string> rhs_rows_, range_rows_;
    [[noreturn]] void fail(const std::string& message) const { throw ModelParseError(line_,message); }
    double number(std::string text) const;
    double finite_sum(double a, double b) const;
    void header(const std::vector<std::string>& tokens);
    std::vector<std::string> fields(const std::string& line) const;
    void data(std::vector<std::string> tokens);
    void select_vector(std::optional<std::string>& current, const std::string& name);
    void bounds(const std::vector<std::string>& tokens);
};
double Reader::number(std::string text) const {
    for (char& c : text) if (c == 'D' || c == 'd') c = 'e';
    if (!text.empty() && text.front() == '+') {
        text.erase(0,1);
        if (!text.empty() && (text.front() == '+' || text.front() == '-')) fail("multiple numeric signs");
    }
    double value = 0;
    const auto result = std::from_chars(text.data(),text.data()+text.size(),value);
    if (result.ec != std::errc{} || result.ptr != text.data()+text.size() || !std::isfinite(value)) fail("invalid finite number: " + text);
    return value;
}
double Reader::finite_sum(double a, double b) const {
    const double result = a + b;
    if (!std::isfinite(result)) fail("numeric overflow");
    return result;
}
void Reader::header(const std::vector<std::string>& t) {
    if (section_ == Section::end) fail("content after ENDATA");
    if ((section_ == Section::sense && !sense_read_) || (section_ == Section::objective && !objective_read_)) fail("missing objective metadata value");
    const auto& key = t[0];
    if (key == "NAME") {
        if (section_ != Section::none) fail("invalid or duplicate NAME");
        // Common Netlib cards append a human-readable title after the model
        // identifier. The identifier is the first token; the suffix is metadata.
        if (t.size() >= 2) model_.name = t[1];
        section_ = Section::name; return;
    }
    if (section_ == Section::none) fail("expected NAME first");
    if (key == "OBJSENSE" || key == "OBJNAME") {
        if (rank_ != 0 || t.size() > 2) fail("objective metadata must precede ROWS");
        if ((key == "OBJSENSE" && sense_read_) || (key == "OBJNAME" && objective_read_)) fail("duplicate objective metadata");
        section_ = key == "OBJSENSE" ? Section::sense : Section::objective;
        if (t.size() == 2) data({t[1]});
        return;
    }
    if (t.size() != 1) fail("unexpected values on section header");
    int next_rank = 0;
    if (key == "ROWS") { next_rank = 1; section_ = Section::rows; rows_seen_ = true; }
    else if (key == "COLUMNS") { next_rank = 2; section_ = Section::columns; columns_seen_ = true; }
    else if (key == "RHS") { next_rank = 3; section_ = Section::rhs; }
    else if (key == "RANGES") { next_rank = 4; section_ = Section::ranges; }
    else if (key == "BOUNDS") { next_rank = 5; section_ = Section::bounds; }
    else if (key == "QUADOBJ") { if(!allow_quadratic_)fail("unsupported section: QUADOBJ");next_rank=6;section_=Section::quadratic; }
    else if (key == "ENDATA") { next_rank = 7; section_ = Section::end; }
    else fail("unsupported section: " + key);
    if (next_rank <= rank_ || (next_rank >= 2 && !rows_seen_) || (next_rank >= 3 && !columns_seen_)) fail("invalid section order");
    if (integer_region_ && next_rank > 2) fail("unclosed INTORG marker");
    rank_ = next_rank;
}
std::vector<std::string> Reader::fields(const std::string& line) const {
    if (options_.format == MpsFormat::free || section_ == Section::sense || section_ == Section::objective) {
        auto tokens = words(line);
        // Dollar-prefixed column/vector identifiers are data, not comments.
        if (section_ == Section::columns || section_ == Section::rhs || section_ == Section::ranges) {
            if (tokens.size() > 1 && tokens[1].starts_with('$')) return {};
            if (tokens.size() > 3 && tokens[3].starts_with('$')) tokens.resize(3);
        }
        return tokens;
    }
    if (line.find('\t') != std::string::npos) fail("tabs are not allowed in fixed-field mode");
    const auto field = [&](Index start, Index count) { return start < line.size() ? trim(line.substr(start,count)) : ""; };
    std::array<std::string,6> f{field(1,2),field(4,8),field(14,8),field(24,12),field(39,8),field(49,12)};
    // Reject nonblank text in separators and beyond the last numeric field.
    for (auto [start,count] : {std::pair<Index,Index>{3,1},{12,2},{22,2},{36,3},{47,2},{61,line.size()}})
        if (!field(start,count).empty()) fail("nonblank text outside fixed fields");
    if (section_ == Section::rows) {
        for (Index i = 2; i < f.size(); ++i) if (!f[i].empty()) fail("extra ROWS fields");
        return {f[0],f[1]};
    }
    if (section_ == Section::bounds) {
        if (!f[4].empty() || !f[5].empty()) fail("extra BOUNDS fields");
        std::vector<std::string> t{f[0],f[1],f[2]};
        if (!f[3].empty()) t.push_back(f[3]);
        return t;
    }
    if (!f[0].empty()) fail("unexpected fixed indicator field");
    if (f[2] == "'MARKER'") {
        if (!f[3].empty() || !f[5].empty()) fail("invalid marker fields");
        return {f[1],f[2],f[4]};
    }
    std::vector<std::string> t{f[1],f[2],f[3]};
    if (!f[4].empty() || !f[5].empty()) { t.push_back(f[4]); t.push_back(f[5]); }
    return t;
}
void Reader::select_vector(std::optional<std::string>& current, const std::string& name) {
    // Empty fixed-field names continue the selected vector, or name the default vector.
    if (!current) current = name;
    else if (!name.empty() && name != *current) fail("multiple data vectors are unsupported");
}
void Reader::bounds(const std::vector<std::string>& t) {
    if (t.size() < 3 || t.size() > 4) fail("invalid BOUNDS record");
    select_vector(bound_name_,t[1]);
    const auto found = column_indices_.find(t[2]);
    if (found == column_indices_.end()) fail("unknown bound variable: " + t[2]);
    auto& v = model_.variables[found->second]; auto& seen = bounds_seen_[found->second];
    const auto& kind = t[0];
    const bool valued = kind == "LO" || kind == "UP" || kind == "FX" || kind == "LI" || kind == "UI";
    if (!valued && kind != "FR" && kind != "MI" && kind != "PL" && kind != "BV") fail("unsupported bound type: " + kind);
    if (t.size() != (valued ? 4U : 3U)) fail("incorrect bound value count");
    const double value = valued ? number(t[3]) : 0;
    if ((kind == "LI" || kind == "UI") && std::trunc(value) != value) fail("integer bound must be integral");
    const bool lower = kind == "LO" || kind == "LI" || kind == "FX" || kind == "FR" || kind == "MI" || kind == "BV";
    const bool upper = kind == "UP" || kind == "UI" || kind == "FX" || kind == "FR" || kind == "PL" || kind == "BV";
    if ((lower && seen.lower) || (upper && seen.upper)) fail("duplicate lower or upper bound");
    if (kind == "LO" || kind == "LI") v.lower = value;
    else if (kind == "UP" || kind == "UI") v.upper = value;
    else if (kind == "FX") v.lower = v.upper = value;
    else if (kind == "FR") { v.lower = -infinity; v.upper = infinity; }
    else if (kind == "MI") v.lower = -infinity;
    else if (kind == "PL") v.upper = infinity;
    else if (kind == "BV") { v.lower = 0; v.upper = 1; v.type = VariableType::binary; }
    if (kind == "LI" || kind == "UI") v.type = VariableType::integer;
    seen.lower = seen.lower || lower; seen.upper = seen.upper || upper;
}
void Reader::data(std::vector<std::string> t) {
    if (section_ == Section::sense) {
        if (sense_read_ || t.size() != 1 || (t[0] != "MIN" && t[0] != "MAX")) fail("expected MIN or MAX");
        model_.sense = t[0] == "MIN" ? ObjectiveSense::minimize : ObjectiveSense::maximize;
        sense_read_ = true; return;
    }
    if (section_ == Section::objective) {
        if (objective_read_ || t.size() != 1 || t[0].empty()) fail("expected objective row name");
        requested_objective_ = t[0]; objective_read_ = true; return;
    }
    if (section_ == Section::rows) {
        if (t.size() != 2 || t[0].size() != 1 || t[1].empty()) fail("invalid ROWS record");
        const char kind = t[0][0];
        if (row_indices_.contains(t[1]) || objective_name_ == t[1]) fail("duplicate row: " + t[1]);
        if (kind == 'N') {
            if (!objective_name_.empty()) fail("multiple N rows are unsupported");
            objective_name_ = t[1]; return;
        }
        if (kind != 'L' && kind != 'G' && kind != 'E') fail("unsupported row type");
        row_indices_[t[1]] = model_.constraints.size();
        model_.constraints.push_back({t[1]}); row_types_.push_back(kind); rhs_.push_back(0); ranges_.push_back(std::nullopt);
        return;
    }
    if (section_ == Section::bounds) { bounds(t); return; }
    if (section_ == Section::quadratic) {
        if(t.size()!=3)fail("expected two variables and one QUADOBJ value");
        const auto first=column_indices_.find(t[0]),second=column_indices_.find(t[1]);
        if(first==column_indices_.end())fail("unknown quadratic variable: "+t[0]);
        if(second==column_indices_.end())fail("unknown quadratic variable: "+t[1]);
        if(entry_count_>=options_.max_entries)fail("coefficient entry limit exceeded");++entry_count_;
        const auto lo=std::min(first->second,second->second),hi=std::max(first->second,second->second);
        const auto key=std::to_string(lo)+":"+std::to_string(hi);if(!quadratic_pairs_.insert(key).second)fail("duplicate quadratic variable pair");
        const auto value=number(t[2]);quadratic_entries_.push_back({first->second,second->second,value});
        if(first->second!=second->second)quadratic_entries_.push_back({second->second,first->second,value});
        return;
    }
    if (section_ != Section::columns && section_ != Section::rhs && section_ != Section::ranges) fail("data outside supported section");
    if (t.size() != 3 && t.size() != 5) fail("expected one or two row/value pairs");
    if (section_ == Section::columns && t[1] == "'MARKER'") {
        if (t.size() != 3 || t[0].empty()) fail("invalid integer marker");
        if (t[2] == "'INTORG'" && !integer_region_) integer_region_ = true;
        else if (t[2] == "'INTEND'" && integer_region_) integer_region_ = false;
        else fail("invalid or nested integer marker");
        previous_column_.clear(); return;
    }
    Index col = 0;
    if (section_ == Section::columns) {
        if (t[0].empty()) t[0] = previous_column_;
        if (t[0].empty()) fail("column continuation without previous column");
        auto [it,inserted] = column_indices_.try_emplace(t[0],model_.variables.size()); col = it->second;
        if (inserted) {
            model_.variables.push_back({t[0],0,integer_region_ ? 1 : infinity,integer_region_ ? VariableType::integer : VariableType::continuous});
            model_.objective.push_back(0); bounds_seen_.push_back({});
        } else if ((model_.variables[col].type == VariableType::integer) != integer_region_) fail("column has inconsistent integer markers");
        previous_column_ = t[0];
    } else select_vector(section_ == Section::rhs ? rhs_name_ : range_name_,t[0]);
    for (Index k = 1; k < t.size(); k += 2) {
        const auto& row = t[k]; const auto value = number(t[k+1]);
        const bool objective = !objective_name_.empty() && row == objective_name_;
        const auto found = row_indices_.find(row);
        if (!objective && found == row_indices_.end()) fail("unknown row: " + row);
        if (section_ == Section::columns) {
            if (entry_count_ >= options_.max_entries) fail("coefficient entry limit exceeded");
            ++entry_count_;
            if (objective) model_.objective[col] = finite_sum(model_.objective[col],value);
            else entries_.push_back({found->second,col,value});
        } else if (section_ == Section::rhs) {
            if (!rhs_rows_.insert(row).second) fail("duplicate RHS row");
            if (objective) model_.objective_offset = -value;
            else rhs_[found->second] = value;
        } else {
            if (objective) fail("objective row cannot have a range");
            if (!range_rows_.insert(row).second) fail("duplicate range row");
            ranges_[found->second] = value;
        }
    }
}
QuadraticModel Reader::read(std::istream& input) {
    if (options_.max_line_length == 0 || options_.max_entries == 0) fail("parser limits must be positive");
    if (options_.format != MpsFormat::free && options_.format != MpsFormat::fixed) fail("invalid MPS format");
    std::string line;
    while (true) {
        line.clear(); char c = 0;
        while (input.get(c)) {
            if (c == '\n') break;
            if (line.size() >= options_.max_line_length) { ++line_; fail("line length limit exceeded"); }
            line.push_back(c);
        }
        if (input.bad() || (input.fail() && !input.eof())) fail("input read failure");
        if (line.empty() && input.eof()) break;
        ++line_;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        for (const unsigned char byte : line)
            if ((byte < 32 && byte != '\t') || byte > 126) fail("MPS requires printable ASCII data");
        if (trim(line).empty() || line[0] == '*') continue;
        if (section_ == Section::end) fail("content after ENDATA");
        if (line[0] != ' ' && line[0] != '\t') {
            const auto tokens = words(line);
            if (!tokens.empty()) header(tokens);
        } else {
            auto tokens = fields(line);
            if (!tokens.empty()) data(std::move(tokens));
        }
    }
    if (line_ == 0) line_ = 1;
    if (section_ != Section::end || !rows_seen_ || !columns_seen_) fail("missing required section or ENDATA");
    if (objective_name_.empty()) fail("missing objective N row");
    if (objective_read_ && requested_objective_ != objective_name_) fail("OBJNAME does not name the objective N row");
    for (Index i = 0; i < model_.constraints.size(); ++i) {
        auto& row = model_.constraints[i]; const auto rhs = rhs_[i]; const auto kind = row_types_[i];
        row.lower = kind == 'L' ? -infinity : rhs; row.upper = kind == 'G' ? infinity : rhs;
        if (ranges_[i]) {
            const auto r = *ranges_[i];
            if (kind == 'L' || (kind == 'E' && r < 0)) row.lower = finite_sum(rhs,-std::abs(r));
            else row.upper = finite_sum(rhs,std::abs(r));
        }
    }
    for (Index j = 0; j < model_.variables.size(); ++j)
        if (!bounds_seen_[j].lower && model_.variables[j].upper < 0) model_.variables[j].lower = -infinity;
    try {
        model_.matrix = CscMatrix::from_triplets(model_.constraints.size(),model_.variables.size(),std::move(entries_));
        require_valid(model_);
    } catch (const InvalidModelError& error) { fail(error.what()); }
    QuadraticModel result;result.linear=std::move(model_);
    try {result.quadratic=CscMatrix::from_triplets(result.linear.variables.size(),result.linear.variables.size(),std::move(quadratic_entries_));}
    catch(const std::exception& error){fail(error.what());}
    return result;
}
}
Model read_mps(std::istream& input, const MpsOptions& options) { return Reader(options,false).read(input).linear; }
Model read_mps_file(const std::filesystem::path& path, const MpsOptions& options) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw ModelParseError(0,"cannot open file: " + path.string());
    return read_mps(input,options);
}
QuadraticModel read_qps(std::istream& input,const MpsOptions& options){return Reader(options,true).read(input);}
QuadraticModel read_qps_file(const std::filesystem::path& path,const MpsOptions& options){std::ifstream input(path,std::ios::binary);if(!input)throw ModelParseError(0,"cannot open file: "+path.string());return read_qps(input,options);}
}
