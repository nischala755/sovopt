#include <sovereign/generators.hpp>
#include <algorithm>
#include <random>
#include <stdexcept>

namespace sovereign {
namespace {
double draw(std::mt19937_64& rng, double low, double high) { return std::uniform_real_distribution<double>(low, high)(rng); }
void positive(Index value, const char* name) { if (!value) throw std::invalid_argument(std::string(name) + " must be positive"); }
}

Model generate_refinery_model(std::uint64_t seed, Index periods) {
    positive(periods, "periods"); std::mt19937_64 rng(seed);
    Model m; m.name = "seeded_refinery_" + std::to_string(seed); m.sense = ObjectiveSense::maximize;
    std::vector<Triplet> entries;
    for (Index t = 0; t < periods; ++t) {
        const Index crude = 3*t, gasoline = crude+1, diesel = crude+2;
        m.variables.push_back({"crude_"+std::to_string(t),0,draw(rng,80,130)});
        m.variables.push_back({"gasoline_"+std::to_string(t),0,infinity});
        m.variables.push_back({"diesel_"+std::to_string(t),0,infinity});
        m.objective.insert(m.objective.end(), {-draw(rng,42,55), draw(rng,68,84), draw(rng,61,76)});
        const Index gas_balance = 3*t, diesel_balance = gas_balance+1, capacity = gas_balance+2;
        const double gas_yield = draw(rng,.42,.55), diesel_yield = draw(rng,.30,.42), cap = draw(rng,85,125);
        m.constraints.push_back({"gas_balance_"+std::to_string(t),0,0});
        m.constraints.push_back({"diesel_balance_"+std::to_string(t),0,0});
        m.constraints.push_back({"capacity_"+std::to_string(t),-infinity,cap});
        entries.insert(entries.end(), {{gas_balance,crude,gas_yield},{gas_balance,gasoline,-1},
            {diesel_balance,crude,diesel_yield},{diesel_balance,diesel,-1},{capacity,crude,1}});
    }
    m.matrix = CscMatrix::from_triplets(m.constraints.size(), m.variables.size(), std::move(entries));
    return m;
}

Model generate_power_model(std::uint64_t seed, Index units) {
    positive(units, "units"); std::mt19937_64 rng(seed);
    Model m; m.name = "seeded_power_" + std::to_string(seed); m.sense = ObjectiveSense::minimize;
    std::vector<Triplet> entries; double capacity_sum = 0;
    m.constraints.push_back({"system_demand",0, infinity});
    m.constraints.push_back({"emissions_cap",-infinity,0});
    for (Index u = 0; u < units; ++u) {
        const double capacity = draw(rng,40,150), cost = draw(rng,18,62), emissions = draw(rng,.15,.9);
        capacity_sum += capacity;
        m.variables.push_back({"generation_"+std::to_string(u),0,capacity}); m.objective.push_back(cost);
        entries.push_back({0,u,1}); entries.push_back({1,u,emissions});
    }
    m.constraints[0].lower = capacity_sum * .55;
    m.constraints[1].upper = capacity_sum * .55 * .75;
    m.matrix = CscMatrix::from_triplets(2, units, std::move(entries));
    return m;
}

Model generate_logistics_model(std::uint64_t seed, Index locations) {
    positive(locations, "locations"); std::mt19937_64 rng(seed);
    Model m; m.name = "seeded_logistics_" + std::to_string(seed); m.sense = ObjectiveSense::minimize;
    // A balanced transportation network with one source and one sink row per location.
    for (Index i = 0; i < locations; ++i) {
        const double supply = draw(rng,30,90);
        m.constraints.push_back({"supply_"+std::to_string(i),-infinity,supply});
        m.constraints.push_back({"demand_"+std::to_string(i),supply*.65,infinity});
    }
    std::vector<Triplet> entries;
    for (Index from = 0; from < locations; ++from) for (Index to = 0; to < locations; ++to) if (from != to || locations == 1) {
        const Index column = m.variables.size();
        m.variables.push_back({"ship_"+std::to_string(from)+"_"+std::to_string(to),0,draw(rng,25,100)});
        m.objective.push_back(draw(rng,2,18));
        entries.push_back({2*from,column,1}); entries.push_back({2*to+1,column,1});
    }
    m.matrix = CscMatrix::from_triplets(m.constraints.size(), m.variables.size(), std::move(entries));
    return m;
}

Model generate_crude_blending_model(std::uint64_t seed, Index crudes) {
    positive(crudes,"crudes"); std::mt19937_64 rng(seed); Model m; m.name="seeded_crude_blending_"+std::to_string(seed);
    m.constraints={{"demand",100,100},{"sulfur_limit",-infinity,160}}; std::vector<Triplet> e;
    for(Index j=0;j<crudes;++j){m.variables.push_back({"crude_"+std::to_string(j),0,100});m.objective.push_back(draw(rng,35,65));e.push_back({0,j,1});e.push_back({1,j,draw(rng,.4,1.5)});}
    m.matrix=CscMatrix::from_triplets(2,crudes,std::move(e)); return m;
}

Model generate_production_planning_model(std::uint64_t seed,Index products,Index periods) {
    positive(products,"products");positive(periods,"periods");std::mt19937_64 rng(seed);Model m;m.name="seeded_production_planning_"+std::to_string(seed);std::vector<Triplet>e;
    for(Index p=0;p<products;++p)m.constraints.push_back({"demand_"+std::to_string(p),double(periods),infinity});
    for(Index t=0;t<periods;++t)m.constraints.push_back({"capacity_"+std::to_string(t),-infinity,double(products)*3});
    for(Index p=0;p<products;++p)for(Index t=0;t<periods;++t){Index j=m.variables.size();m.variables.push_back({"make_"+std::to_string(p)+"_"+std::to_string(t),0,5,VariableType::integer});m.objective.push_back(draw(rng,4,15));e.push_back({p,j,1});e.push_back({products+t,j,1});}
    m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(e));return m;
}

Model generate_supply_chain_model(std::uint64_t seed,Index plants,Index customers) {
    positive(plants,"plants");positive(customers,"customers");std::mt19937_64 rng(seed);Model m;m.name="seeded_supply_chain_"+std::to_string(seed);std::vector<Triplet>e;
    const double demand=10;for(Index p=0;p<plants;++p)m.constraints.push_back({"plant_"+std::to_string(p),-infinity,demand*customers/plants*1.25});for(Index c=0;c<customers;++c)m.constraints.push_back({"customer_"+std::to_string(c),demand,infinity});
    for(Index p=0;p<plants;++p)for(Index c=0;c<customers;++c){Index j=m.variables.size();m.variables.push_back({"ship_"+std::to_string(p)+"_"+std::to_string(c),0,infinity});m.objective.push_back(draw(rng,1,12));e.push_back({p,j,1});e.push_back({plants+c,j,1});}
    m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(e));return m;
}

Model generate_process_model(std::uint64_t seed,Index stages) {
    positive(stages,"stages");std::mt19937_64 rng(seed);Model m;m.name="seeded_process_"+std::to_string(seed);m.sense=ObjectiveSense::maximize;std::vector<Triplet>e;
    for(Index j=0;j<stages;++j){m.variables.push_back({"flow_"+std::to_string(j),0,100});m.objective.push_back(j+1==stages?draw(rng,20,30):-draw(rng,1,3));if(j){Index i=m.constraints.size();const double yield=draw(rng,.82,.97);m.constraints.push_back({"balance_"+std::to_string(j),0,0});e.push_back({i,j-1,yield});e.push_back({i,j,-1});}}
    m.constraints.push_back({"feed_capacity",-infinity,100});e.push_back({m.constraints.size()-1,0,1});m.matrix=CscMatrix::from_triplets(m.constraints.size(),m.variables.size(),std::move(e));return m;
}
}
