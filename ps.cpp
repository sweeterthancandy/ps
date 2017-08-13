#include <iostream>
#include <string>
#include <type_traits>
#include <chrono>
#include "ps/support/processor.h"
#include "ps/base/holdem_hand_vector.h"
#include "ps/base/holdem_class_vector.h"
#include "ps/base/holdem_class_range_vector.h"
#include "ps/eval/class_equity_evaluator_cache.h"
#include "ps/eval/class_range_equity_evaluator.h"
#include "ps/detail/cross_product.h"

#include <boost/timer/timer.hpp>

using namespace ps;

#if 0
struct equity_future{
        using result_t = std::shared_future<
                std::shared_ptr<equity_breakdown>
        >;
        equity_future()
                : impl_{ &equity_evaluator_factory::get("principal") }
        {}
        result_t schedual_group(support::processor::process_group& pg, holdem_hand_vector const& players){
                std::cout << "schedual_group(" << players << "\n";

                auto iter = m_.find(players);
                if( iter != m_.end() ){
                        return iter->second;
                }
                auto task = std::make_shared<std::packaged_task<std::shared_ptr<equity_breakdown>()> >(
                        [players,this](){
                        return impl_->evaluate(players);
                });
                m_.emplace(players, std::move(task->get_future()));

                // yuk
                auto w = [task]()mutable{
                        (*task)();
                };
                pg.push(std::move(w));
                return schedual_group(pg, players);
        }
private:
        equity_evaluator* impl_;
        // only cache those in standard form
        std::map< holdem_hand_vector, result_t > m_;
};
#endif

#if 0
using shared_fut_t = std::shared_future<
        std::shared_ptr<equity_breakdown>
>;
using eval_t = std::function<
        std::shared_ptr<equity_breakdown>()
>;

struct equity_future{
        equity_future()
                : impl_{ &equity_evaluator_factory::get("principal") }
        {}
        eval_t schedual_group(support::processor::process_group& pg, holdem_hand_vector const& players){
                std::cout << "schedual_group(" << players << "\n";

                if( players.is_standard_form() ){
                        auto sf = do_schedual_standard_form_(pg, players);
                        return eval_t{
                                [_sf=std::move(sf)]()
                                { return _sf.get(); }
                        };
                } else {
                        auto p =  permutate_for_the_better(players) ;
                        auto& perm = std::get<0>(p);
                        auto const& perm_players = std::get<1>(p);

                        auto sf = do_schedual_standard_form_(pg, perm_players);
                        return eval_t(
                                [_perm=std::move(perm),_sf=std::move(sf)]()
                                { 
                                        return std::make_shared<equity_breakdown_permutation_view>(
                                                _sf.get(),
                                                _perm
                                        );
                                }
                        );
                }
        }
private:
        shared_fut_t do_schedual_standard_form_(support::processor::process_group& pg, holdem_hand_vector const& players){
                std::cout << "do_schedual_standard_form_(" << players << "\n";

                auto iter = m_.find(players);
                if( iter != m_.end() ){
                        return iter->second;
                }
                auto task = std::make_shared<std::packaged_task<std::shared_ptr<equity_breakdown>()> >(
                        [players,this](){
                        return impl_->evaluate(players);
                });
                m_.emplace(players, std::move(task->get_future()));

                // yuk
                auto w = [task]()mutable{
                        (*task)();
                };
                pg.push(std::move(w));
                return do_schedual_standard_form_(pg, players);
        }

        equity_evaluator* impl_;
        // only cache those in standard form
        std::map< holdem_hand_vector, shared_fut_t > m_;
};

struct class_equity_future{
        using result_t = std::shared_future<
                std::shared_ptr<equity_breakdown>
        >;
        class_equity_future()
        {
        }
        result_t schedual_group(support::processor::process_group& pg, holdem_class_vector const& players){
                std::cout << "schedual_group(" << players << "\n";
                
                std::vector< eval_t > items;

                for( auto hv : players.get_hand_vectors()){
                        items.emplace_back( ef_.schedual_group(pg, hv ) );
                }
                auto task = std::make_shared<std::packaged_task<std::shared_ptr<equity_breakdown>()> >(
                        [n_=players.size(),items_=std::move(items)](){
                        auto result = std::make_shared<equity_breakdown_matrix_aggregator>(n_);
                        for( auto const& item : items_ ){
                                result->append( *item() );
                        }
                        return result;
                });
                result_t fut = task->get_future();
                pg.sequence_point();
                pg.push([task]()mutable{ (*task)(); });
                return fut;
        }
private:
        ::equity_future ef_;
};

struct class_range_equity_future{
        using result_t = std::shared_future<
                std::shared_ptr<equity_breakdown>
        >;
        #if 0
        result_t schedual_group(support::processor::process_group& pg, holdem_class_range_vector const& players){
                std::cout << "schedual_group(" << players << "\n";

                std::vector< std::tuple< std::vector<int>, result_t > > items;

                for( auto const& cv : players.get_cross_product()){
                        auto sub = std::make_unique<support::processor::process_group>();
                        items.push_back( cef_.schedual_group(*sub, cv_perm) );
:w
                        pg.push(std::move(sub));
                }
                auto task = std::make_shared<std::packaged_task<std::shared_ptr<equity_breakdown>()> >(
                        [n_=players.size(),items_=std::move(items)](){
                        auto result = std::make_shared<equity_breakdown_matrix_aggregator>(n_);
                        for( auto& t : items_ ){
                                result->append(
                                        *std::make_shared<equity_breakdown_permutation_view>(
                                                std::get<1>(t).get(),
                                                std::get<0>(t)));
                        }
                        return result;
                });
                result_t fut = task->get_future();
                pg.sequence_point();
                pg.push([task]()mutable{ (*task)(); });
                return fut;
        }
        #endif
        result_t schedual_group(support::processor::process_group& pg, holdem_class_range_vector const& players){
                std::cout << "schedual_group(" << players << "\n";
                std::vector<result_t> params;
                for( auto const& cv : players.get_cross_product()){
                        auto sub = std::make_unique<support::processor::process_group>();
                        params.push_back( cef_.schedual_group(*sub, cv) );
                        pg.push(std::move(sub));
                }
                auto task = std::make_shared<std::packaged_task<std::shared_ptr<equity_breakdown>()> >(
                        [_n=players.size(),_params=std::move(params)](){
                                std::cout << "in task\n";
                                PRINT(_params.size());
                                auto result = std::make_shared<equity_breakdown_matrix_aggregator>(_n);
                                for( auto& item : _params ){
                                        result->append(*item.get());
                                }
                                return std::move(result);
                        }
                );
                result_t fut = task->get_future();
                pg.sequence_point();
                pg.push([task]()mutable{ (*task)(); });
                return fut;
        }
private:
        ::class_equity_future cef_;
};
#endif

template<class From, class To>
struct basic_cache_object{
        
        using from_t = From;
        using to_t   = To;

        // I think this is appropriate
        to_t const* lookup(from_t const& from){
                auto iter = cache_.find(from);
                if( iter == cache_.end())
                        return nullptr;
                return &iter->second;
        }
        void commit( from_t const& from, to_t const& to){
                cache_.emplace(from, to);
        }
        #if 0
        bool load(std::string const& name){
                std::ifstream is(name);
                if( ! is.is_open() )
                        return false;
                boost::archive::text_iarchive ia(is);
                ia >> *this;
                return true;
        }
        bool save(std::string const& name)const{
                std::ofstream of(name);
                boost::archive::text_oarchive oa(of);
                oa << *this;
                return true;
        }
        template<class Archive>
        void serialize(Archive& ar, unsigned int){
                ar & cache_;
        }
        #endif
private:
        std::map< from_t, to_t > cache_;
};

using holdem_hand_eval_cache  = basic_cache_object<holdem_hand_vector, equity_breakdown_matrix>;
using holdem_class_eval_cache = basic_cache_object<holdem_class_vector, equity_breakdown_matrix>;


struct cache_controller{
};


int main(){
        boost::timer::auto_cpu_timer at;
        holdem_class_range_vector players;
        #if 0
        // Hand 0:  30.845%   28.23%  02.61%      3213892992  297127032.00   { TT+, AQs+, AQo+ }
        // Hand 1:  43.076%   40.74%  02.33%      4637673516  265584984.00   { QQ+, AKs, AKo }
        // Hand 2:  26.079%   25.68%  00.40%      2923177728   45324924.00   { TT }
        players.push_back(" TT+, AQs+, AQo+ ");
        players.push_back(" QQ+, AKs, AKo ");
        players.push_back("TT");
        #else
        // Hand 0:  81.547%   81.33%  00.22%        50134104     133902.00   { AA }
        // Hand 1:  18.453%   18.24%  00.22%        11241036     133902.00   { QQ }
        players.push_back("AA");
        players.push_back("QQ");
        #endif

        support::processor proc;
        equity_breakdown_matrix_aggregator result;
        auto g = std::make_unique<support::processor::process_group>();
        for( size_t i=0; i!= std::thread::hardware_concurrency();++i){
                proc.spawn();
        }
        holdem_class_eval_cache class_cache;
        for( auto const& stdclass : players.to_class_standard_form() ){
                auto ptr = class_cache.find(std::get<1>(stdclass));
                if( ptr != nullptr ){
                        result->append_perm( result, std::get<0>(stdclass));
                }
                for( auto const& stdhand : std::get<1>(stdclass).to_standard_form()){
                        auto ptr = 
                }
        }
        proc.join();


        #if 0
        support::processor proc;
        ::equity_future ef;
        for( size_t i=0; i!= std::thread::hardware_concurrency();++i){
                proc.spawn();
        }
        
        auto g = std::make_unique<support::processor::process_group>();
        auto stdform = players.to_standard_form();
        std::vector< std::tuple< ::equity_future::result_t, std::vector<int> > > working;
        for( auto const& t : stdform){
                PRINT(std::get<1>(t));
                working.emplace_back( ef.schedual_group(*g, std::get<1>(t)), std::get<0>(t));
        }
        proc.accept(std::move(g));
        proc.join();
        auto const n = players.size();
        for( auto const& t : working){
                auto const& eb  = std::get<0>(t).get();
                auto const& mat = std::get<1>(t);
                result.append_matrix( *eb, mat);
        }
        std::cout << result << "\n";
        #endif



        
        #if 0
        class_range_equity_future cef;
        
        support::processor proc;
        for( size_t i=0; i!= std::thread::hardware_concurrency();++i){
                proc.spawn();
        }
        auto g = std::make_unique<support::processor::process_group>();
        //auto ret = cef.schedual_group(*g, players.get_cross_product().front());
        auto ret = cef.schedual_group(*g, players);
        proc.accept(std::move(g));
        proc.join();
        std::cout << *ret.get() << "\n";
        #endif
}

#if 0
int main(){
        support::processor proc;
        for( size_t i=0; i!= std::thread::hardware_concurrency();++i){
                proc.spawn();
        }
        holdem_class_vector players;
        players.push_back("AKs");
        players.push_back("KQs");
        players.push_back("QJs");
        class_equity_future ef;
        auto pg = std::make_unique<support::processor::process_group>();
        auto ret = ef.schedual_group(*pg, players);
        proc.accept(std::move(pg));
        proc.join();
        std::cout << *(ret.get()) << "\n";
        #if 0
        holdem_class_vector players;
        players.push_back("99");
        players.push_back("55");
        class_equity_evaluator_cache ec;

        ec.load("cache.bin");
        std::cout << *ec.evaluate(players) << "\n";
        ec.save("cache.bin");
        #endif
        #if 0
        holdem_class_vector players;
        players.push_back("AA");
        players.push_back("QQ");
        class_equity_evaluator_cache ec;
        std::cout << *ec.evaluate(players) << "\n";
        #endif

}
#endif


#if 0

auto make_proto(std::string const& tok){
        auto a = std::make_unique<processor::process_group>();
        a->push( [tok](){
                std::cout << tok << "::1\n";
        });
        a->push( [tok](){
                std::cout << tok << "::2\n";
        });
        a->sequence_point();
        a->sequence_point();
        a->push( [tok](){
                std::cout << tok << "::3\n";
        });
        return std::move(a);
}


int processor_test0(){
        processor proc;
        #if 1
        for(int i=0;i!=100;++i)
                proc.spawn();
                #endif
        proc.spawn();
        #if 1
        auto g = std::make_unique<processor::process_group>();
        g->push( make_proto("a"));
        g->push( make_proto("b"));
        g->sequence_point();
        g->push( make_proto("c"));
        auto h = std::make_unique<processor::process_group>();
        h->push(std::move(g));
        //h->sequence_point();
        h->push([]{std::cout << "Finished\n"; });

        proc.accept(std::move(h));
        #endif
        proc.join();
}
#endif 












