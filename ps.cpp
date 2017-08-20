#include <iostream>
#include <thread>
#include <atomic>
#include <numeric>
#include <bitset>
#include <future>
#include <boost/format.hpp>
#include "ps/base/cards.h"
#include "ps/detail/print.h"
#include "ps/base/board_combination_iterator.h"
#include "ps/eval/class_equity_evaluator.h"
#include "ps/eval/equity_breakdown_matrix.h"
#include "ps/eval/evaluator.h"
#include "ps/sim/holdem_class_strategy.h"
#include "ps/support/index_sequence.h"
#include "ps/support/config.h"


/*
                Strategy in the form 
                        vpip/fold
 */
#include <boost/range/algorithm.hpp>

/*
        Need to,

                iterate over boards
                        abcde  where a < b < c < d < e ( think strict lower triangle )
                iterator over hands
                        ab  where a <= b               ( think lower triable )


 */



using namespace ps;



struct evaluator_7_card_map : evaluator
{
        evaluator_7_card_map(){
                impl_ = &evaluator_factory::get("5_card_map");
                card_map_7_.resize( make_hash_(12,12,12,12, 11,11,11));

                for(size_t i=0;i!=52;++i){
                        card_rank_device_[i] = card_decl::get(i).rank().id();
                }

                using iter_t = basic_index_iterator<
                        int, ordered_policy
                >;
                for(iter_t iter(3,3),end;iter!=end;++iter){
                        std::cout << detail::to_string(*iter) << "\n";
                }

                for(iter_t iter(7,13),end;iter!=end;++iter){
                        maybe_add_(*iter);

                        auto b{*iter};
                        if( impl_->rank( b[0], b[1], b[2], b[3], b[4], b[5], b[6] ) !=
                             this->rank( b[0], b[1], b[2], b[3], b[4], b[5], b[6] ) )
                        {
                                std::cout << "ERROR\n";
                        }

                        #if 0
                        if( assert_set.size() == 20 ){
                                return;
                        }
                        #endif
                }
        }
        ranking_t rank(long a, long b, long c, long d, long e)const override{
                return impl_->rank(a,b,c,d,e);
        }
        ranking_t rank(long a, long b, long c, long d, long e, long f)const override{
                return impl_->rank(a,b,c,d,e,f);
        }
        ranking_t rank(long a, long b, long c, long d, long e, long f, long g)const override{
                return impl_->rank(a,b,c,d,e,f,g);
        }
private:
        void maybe_add_(std::vector<int> const& b){
                // first check we don't have more than 4 of each card
                std::array<int, 13> aux = {0};
                for(size_t i=0;i!=7;++i){
                        ++aux[b[i]];
                }
                for(size_t i=0;i!=aux.size();++i){
                        if( aux[i] > 4 )
                                return;
                }
                auto hash = make_hash_( b[0], b[1], b[2], b[3], b[4], b[5], b[6] );

                if( assert_set.count(hash) !=0  ){
                        //std::cout << "not injective\n";
                        return;
                }
                assert_set.insert(hash);

                auto val  = impl_->rank( b[0], b[1], b[2], b[3], b[4], b[5], b[6] );

                //std::cout << detail::to_string(aux) << " - " << detail::to_string(b) << " => " << std::bitset<30>(static_cast<unsigned long long>(hash)).to_string() << "\n";
                //

                card_map_7_[hash] = val;
        }
        /*
                  +----+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |card|A |K |Q |J |T |9 |8 |7 |6 |5 |4 |3 |2 |
                  +----+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |yyyy|xx|xx|xx|xx|xx|xx|xx|xx|xx|xx|xx|xx|xx|
                  +----+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |  1A|18|16|14|12|10| E| C| A| 8| 6| 4| 2| 0|
                  +----+--+--+--+--+--+--+--+--+--+--+--+--+--+

                  yyyy ~ value of rank with 4 cards, zero 
                         when there warn't 4 cards

                  xx   ~ bit mask to non-injective mapping for
                         number of cards, 


                                   n | bits
                                   --+-----
                                   0 | 00
                                   1 | 01
                                   2 | 10
                                   3 | 11
                                   4 | 11
        */
        size_t make_hash_(long a, long b, long c, long d, long e, long f, long g)const{
                size_t hash = 0;

                hash_add_(hash, a);
                hash_add_(hash, b);
                hash_add_(hash, c);
                hash_add_(hash, d);
                hash_add_(hash, e);
                hash_add_(hash, f);
                hash_add_(hash, g);

                return hash;
        } 
        void hash_add_(size_t& hash, size_t rank)const{
                auto idx = rank * 2;
                auto mask = ( hash & ( 0x3 << idx ) ) >> idx;
                switch(mask){
                case 0x0:
                        // set the idx'th bit
                        hash |= 0x1 << idx;
                        break;
                case 0x1:
                        // unset the idx'th bit
                        hash &= ~(0x1 << idx);
                        // set the (idx+1)'th bit
                        hash |= 0x1 << (idx+1);
                        break;
                case 0x2:
                        // set the idx'th bit
                        hash |= 0x1 << idx;
                        break;
                case 0x3:
                        // set the special part of mask for the fourth card
                        hash |= (rank + 1) << 0x1A;
                        break;
                default:
                        PS_UNREACHABLE();
                }
        }
        std::set<size_t> assert_set;
        evaluator* impl_;
        std::array<size_t, 52> card_rank_device_;
        std::vector<ranking_t> card_map_7_;
};

int main(){
        evaluator_7_card_map m;
}
