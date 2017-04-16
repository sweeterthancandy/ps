#ifndef PS_EQUITY_CALC_H
#define PS_EQUITY_CALC_H

#include <ostream>
#include <string>

#include <boost/format.hpp>

#include "ps/core/eval.h"
#include "ps/detail/visit_combinations.h"
#include "ps/holdem/holdem_traits.h"
#include "ps/core/cards.h"


namespace ps{


        // AkAh vs 2c2c      atomic
        // AA vs T9s         
        //
        // AcAd       Tc9c
        // AcAh       Td9d
        // AcAs   VS  Th9h
        // AhAd       Ts9s
        // AhAs
        // AsAs
        //
        // Ako,KK+ vs 89s+,TT+,AQo+

        struct equity_player{
                equity_player(id_type hand)
                        :hand_(hand),
                        wins_{0},draw_{0},sigma_{0},equity_{0.0}
                {}

                friend std::ostream& operator<<(std::ostream& ostr, equity_player const& self){
                        auto pct{ 1.0 / self.sigma_ * 100 };
                        return ostr  << boost::format("{ sigma=%d, wins=%d(%.2f), draw=%d(%.2f), equity=%.2f(%.2f) }")
                                % self.sigma_
                                % self.wins_ % ( self.wins_ * pct)
                                % self.draw_ % ( self.draw_ * pct)
                                % self.equity_ % ( self.equity_ * pct);
                }
                auto wins()const{ return wins_; }
                auto draws()const{ return draw_; }
                auto sigma()const{ return sigma_; }
                auto equity()const{ return equity_ / sigma_; }

                auto get_hand()const{ return hand_; }
        private:
                // these 2 classes a deeply coupled
                friend class equity_calc;

                id_type hand_;

                size_t wins_;
                size_t draw_;
                size_t sigma_;
                double equity_;
        };
        
        struct equity_context{


                equity_context& add_player(std::string const& s){
                        players_.emplace_back( holdem_hand_decl::get(s).id() );
                        return *this;
                }
                equity_context& add_board(std::string const& s){
                        for( size_t i=0; i!= s.size();i+=2)
                                board_.emplace_back( card_decl::get(s.substr(i,2)).id() );
                        return *this;
                }
                std::vector<equity_player>& get_players(){ return players_; }
                decltype(auto) get_board(){ return board_; }
                decltype(auto) get_dead(){ return dead_; }
        private:
                holdem_hand_maker hm_;

                std::vector<id_type> board_;
                std::vector<id_type> dead_;

                std::vector<equity_player> players_;
        };


        struct equity_calc{
                void run( equity_context& ctx){

                        std::vector<long> known;
                        for( auto const& p : ctx.get_players() ){
                                known.emplace_back( holdem_hand_decl::get(p.get_hand()).first().id() );
                                known.emplace_back( holdem_hand_decl::get(p.get_hand()).second().id() );
                        }
                        boost::copy( ctx.get_board(), std::back_inserter(known));
                        boost::copy( ctx.get_dead(), std::back_inserter(known));
                        boost::sort(known);
                        auto filter = [&](long c){ return ! boost::binary_search(known, c); };
                
                        auto do_eval = [&](id_type a, id_type b, id_type c, id_type d, id_type e){
                                std::vector<std::pair<std::uint32_t, equity_player* > > ranked;
                                std::vector<equity_player*> winners;
                                for( auto& p : ctx.get_players() ){
                                        auto x{ holdem_hand_decl::get(p.get_hand()).first().id() };
                                        auto y{ holdem_hand_decl::get(p.get_hand()).second().id() };
                                        ++p.sigma_;
                                        ranked.emplace_back( std::make_pair(eval_(x,y,a,b,c,d,e), &p));
                                }
                                boost::sort( ranked, [](auto const& l, auto const& r){ return l.first < r.first; });

                                auto iter{ boost::find_if( ranked, [&](auto const& _){ return _.first != ranked.front().first; } ) }; 
                                ranked.resize( iter - ranked.begin());
                                if( ranked.size() == 1 ){
                                        ++ranked.front().second->wins_;
                                        ranked.front().second->equity_ += 1.0;
                                } else{
                                        for( auto& r : ranked ){
                                                ++r.second->draw_;
                                                r.second->equity_ += 1.0 / ranked.size();
                                        }
                                }
                        };

                        auto const& board{ ctx.get_board() };


                        switch(ctx.get_board().size()){
                        case 0:
                                detail::visit_combinations<5>( [&](long a, long b, long c, long d, long e){
                                        do_eval(a,b,c,d,e);
                                }, filter, 51);
                                break;
                        case 1:
                                detail::visit_combinations<4>( [&](long a, long b, long c, long d){
                                        do_eval(board[0], a,b,c,d);
                                }, filter, 51);
                                break;
                        case 2:
                                detail::visit_combinations<3>( [&](long a, long b, long c){
                                        do_eval(board[0], board[1], a,b,c);
                                }, filter, 51);
                                break;
                        case 3:
                                detail::visit_combinations<2>( [&](long a, long b){
                                        do_eval(board[0], board[1], board[2], a,b);
                                }, filter, 51);
                                break;
                        case 4:
                                detail::visit_combinations<1>( [&](long a){
                                        do_eval(board[0], board[1], board[2], board[3], a);
                                }, filter, 51);
                                break;
                        case 5:
                                do_eval( board[0], board[1], board[2], board[3], board[4]);
                                break;
                        default:
                                BOOST_THROW_EXCEPTION(std::domain_error("bad number of board cards"));
                        }
                }
        private:
                eval eval_;
        };

}

#endif // PS_EQUITY_CALC_H
