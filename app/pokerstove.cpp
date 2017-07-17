#include "ps/calculator.h"
#include "ps/frontend.h"
#include "ps/tree.h"

#include <type_traits>
#include <functional>
#include <iostream>

#include <boost/timer/timer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

using namespace ps;

namespace ps{


        /*
                poker_stove AKs TT QQ
                poker_stove --cache new_cache.bin AKs TT
         */

        struct pretty_printer{
                void operator()(std::ostream& ostr, view_t result, std::vector<std::string> const& players){
                        std::vector<
                                std::vector<std::string>
                        > lines;

                        lines.emplace_back();
                        lines.back().emplace_back("range");
                        lines.back().emplace_back("equity");
                        lines.back().emplace_back("wins");
                        //lines.back().emplace_back("draws");
                        #if 1
                        for(size_t i=0; i != players.size() -1;++i){
                                lines.back().emplace_back("draw_"+ boost::lexical_cast<std::string>(i+1));
                        }
                        
                        #endif
                        lines.back().emplace_back("draw equity");
                        lines.back().emplace_back("lose");
                        lines.back().emplace_back("sigma");
                        lines.emplace_back();
                        lines.back().push_back("__break__");
                        for( size_t i=0;i!=players.size();++i){
                                auto pv{ result.player(i) };
                                lines.emplace_back();

                                lines.back().emplace_back( boost::lexical_cast<std::string>(players[i]) );
                                lines.back().emplace_back( str(boost::format("%.4f%%") % (pv.equity() * 100)));
                                /*
                                        draw_equity = \sum_i=1..n win_{i}/i
                                */
                                for(size_t i=0; i != players.size(); ++i ){
                                        lines.back().emplace_back( boost::lexical_cast<std::string>(pv.nwin(i)));
                                }

                                auto draw_sigma{
                                        (pv.equity() - static_cast<double>(pv.win())/pv.sigma())*pv.sigma()
                                };
                                lines.back().emplace_back( str(boost::format("%.2f%%") % ( draw_sigma )));
                                lines.back().emplace_back( boost::lexical_cast<std::string>(pv.lose()) );
                                lines.back().emplace_back( boost::lexical_cast<std::string>(pv.sigma()) );
                        }
                        
                        std::vector<size_t> widths(lines.back().size(),0);
                        for( auto const& line : lines){
                                for(size_t i=0;i!=line.size();++i){
                                        widths[i] = std::max(line[i].size(), widths[i]);
                                }
                        }
                        auto pad{ [](auto const& s, size_t w){
                                size_t padding{ w - s.size()};
                                size_t left_pad{padding/2};
                                size_t right_pad{padding - left_pad};
                                std::string ret;
                                if(left_pad)
                                       ret += std::string(left_pad,' ');
                                ret += s;
                                if(right_pad)
                                       ret += std::string(right_pad,' ');
                                return std::move(ret);
                        }};
                        for( auto const& line : lines){
                                if( line.size() >= 1 && line[0] == "__break__" ){
                                        for(size_t i=0;i!=widths.size();++i){
                                                if( i != 0 )
                                                        ostr << "-+-";
                                                ostr << std::string(widths[i],'-');
                                        }
                                } else{
                                        for(size_t i=0;i!=line.size();++i){
                                                if( i != 0 )
                                                        ostr << " | ";
                                                ostr << pad(line[i], widths[i]);
                                        }
                                }
                                ostr << "\n";
                        }
                }
        };

        int pokerstove_driver(int argc, char** argv){
                using namespace ps::frontend;

                calculater calc;

                std::vector<std::string> players_s;
                std::vector<frontend::range> players;

                boost::timer::auto_cpu_timer at;

                int arg_iter = 1;

                for(; arg_iter < argc;){
                        int args_left{ argc - arg_iter };
                        switch(args_left){
                        default:
                        case 2:
                                if( argv[arg_iter] == std::string{"--cache"} ){
                                        std::cerr << "Loading...\n";

                                        if( ! calc.load(argv[arg_iter+1] ) ){
                                                std::cerr << "Failed to load " << argv[arg_iter+1] << "\n";
                                        } else {
                                                std::cerr << "Done...\n";
                                        }
                                        arg_iter += 2;
                                        continue;
                                }
                        case 1:
                                // we must have players now
                                for(; arg_iter < argc;++arg_iter){
                                        players_s.push_back( argv[arg_iter] );
                                        players.push_back( frontend::parse(argv[arg_iter]));
                                }
                                break;
                        }
                }

                double sigma{0};
                size_t factor{0};
                tree_range root{ players };
                aggregator agg;
                for( auto const& c : root.children ){

                        // this means it's a class vs class evaulation
                        if( c.opt_cplayers.size() != 0 ){
                                auto ret{ calc.calculate_class_equity( c.opt_cplayers ) };
                                agg.append(ret);
                        } else{
                                for( auto const& d : c.children ){
                                        auto ret{ calc.calculate_hand_equity( d.players ) };
                                        agg.append(ret);
                                }
                        }

                }

                pretty_printer{}(std::cout, agg.make_view(), players_s);

                return 0;
        }
} // anon

int main(int argc, char** argv){
        return pokerstove_driver(argc, argv);
}
