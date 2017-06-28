#ifndef PS_HEADS_UP_H
#define PS_HEADS_UP_H

#include <future>
#include <thread>
#include <mutex>
#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

#include "ps/tree.h"
#include "ps/equity_calc_detail.h"
#include "ps/algorithm.h"

namespace ps{

struct hu_visitor{

        template<class Int, class Vec>
        void operator()(Int a, Int b, Int c, Int d, Int e, Vec const& ranked){
                //PRINT( detail::to_string(ranked) );
                if( ranked[0] < ranked[1] ){
                        ++win;
                } else if( ranked[0] > ranked[1] ){
                        ++lose;
                } else{
                        ++draw;
                }
        }  
        hu_visitor permutate(std::vector<int> const& perm)const{
                hu_visitor ret(*this);
                if( perm[0] == 1 )
                        std::swap(ret.win, ret.lose);
                return ret;
        }
        hu_visitor const& append(hu_visitor const& that){
                win  += that.win;
                lose += that.lose;
                draw += that.draw;
                return *this;
        }
        template<class Archive>
        void serialize(Archive& ar, unsigned int){
                ar & win;
                ar & lose;
                ar & draw;
        }
        friend std::ostream& operator<<(std::ostream& ostr, hu_visitor const& self){
                auto sigma{ self.win + self.lose + self.draw };
                auto equity{ (self.win + self.draw / 2.0 ) / sigma * 100 };
                return ostr << boost::format("%2.2f%% (%d,%d,%d)") % equity % self.win % self.draw % self.lose;
        }
        size_t win{0};
        size_t lose{0};
        size_t draw{0};
};
        

/*
        This call is take in a sequence of hands, and then return
        the preflop equity
 */
struct equity_cacher{
        hu_visitor visit_boards( std::vector<ps::holdem_id> const& players){

                auto p{ permutate_for_the_better(players) };
                
                return do_visit_boards( std::get<0>(p), std::get<1>(p) );
        }
        hu_visitor do_visit_boards( std::vector<int> const& perm, std::vector<ps::holdem_id> const& players){
                auto iter = cache_.find(players);
                if( iter != cache_.end() ){
                        #if 0
                        hu_visitor v;
                        ec_.visit_boards(v, players);
                        if( v.win != iter->second.win  ||
                            v.draw != iter->second.draw ||
                            v.lose != iter->second.lose ){
                                PRINT_SEQ((v)(iter->second));
                                asseet(false);
                        }
                        #endif 
                        hu_visitor aux{ iter->second.permutate(perm) };
                        return aux;
                }

                hu_visitor v;
                ec_.visit_boards(v, players);
                //std::cout << detail::to_string(players) << " -> " << v << "\n";

                cache_.insert(std::make_pair(players, v));
                return do_visit_boards(perm, players);
        }

        template<class Archive>
        void serialize(Archive& ar, unsigned int){
                ar & cache_;          
        }

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
        void append( equity_cacher const& that){
                for( auto const& m : that.cache_ ){
                        cache_.insert(m);
                }
        }
        auto cache_size()const{ return cache_.size(); }
private:
        std::map<std::vector<ps::holdem_id>, hu_visitor> cache_;
        ps::equity_calc_detail ec_;
};

struct class_equity_cacher{
};


inline
void generate_cache(){

        frontend::range p0;
        frontend::range p1;
        p0 = frontend::percent(100);
        p1 = frontend::percent(100);

        tree_range root{ std::vector<frontend::range>{p0, p1} };
        std::set< std::vector<ps::holdem_id> > world;
        size_t sigma{0};
        
        for( auto const& c : root.children ){
                for( auto d : c.children ){
                        ++sigma;
                        auto p{ permutate_for_the_better(d.players) };
                        world.insert( std::get<1>(p) );
                }
        }
        auto iter{world.begin()};
        auto end{world.end()};

        
        std::mutex mtx;
        std::mutex result_mtx;
        
        std::atomic_int done{0};

        std::vector<std::thread> tg;
        std::vector<equity_cacher> result;
        for(size_t i=0; i!=16 ; ++i){
                tg.emplace_back( [&]()mutable{
                        equity_cacher ec;
                        for(;;){
                                mtx.lock();
                                if( iter == end ){
                                        mtx.unlock();
                                        break;
                                }
                                auto first{iter};
                                const size_t batch_size{50};
                                for(size_t c=0; c != batch_size && iter!=end;++c,++iter);
                                auto last{iter};
                                mtx.unlock();
                                
                                for(;first!=last;++first){
                                        auto v = ec.visit_boards( *first );
                                        ++done;
                                }
                                PRINT_SEQ((sigma)(world.size())(done));
                                #if 0
                                if( done > 1000 )
                                        break;
                                #endif
                        }
                        std::lock_guard<std::mutex> lock(result_mtx);
                        result.push_back(std::move(ec));
                } );
        }
        for( auto& t : tg )
                t.join();
        equity_cacher aggregate;
        for( auto const& r : result )
                aggregate.append(r);
        PRINT(aggregate.cache_size());
        aggregate.save("cache.bin");
                        
        PRINT_SEQ((sigma)(world.size())(done));

}

} // ps

#endif // PS_HEADS_UP_H
