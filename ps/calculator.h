#ifndef PS_CALCULATOR_H
#define PS_CALCULATOR_H

#include <string>
#include <memory>

#include "ps/cards_fwd.h"
#include "ps/calculator_view.h"

namespace ps{
        
        using view_t = detailed_view_type;

        struct  calculator_impl;
        
        struct aggregator{
                void append(view_t const& view){
                        if( n_ == 0 ){
                                n_ = view.n();
                                data_.resize( n_ * n_ );
                        }
                        sigma_ += view.sigma();
                        for(size_t i=0;i!=n_;++i){
                                for(size_t j=0;j!=n_;++j){
                                        data_access(i, j) += view.player(i).nwin(j);
                                }
                        }
                }
                view_t make_view(){
                        std::vector<int> perm;
                        for(size_t i=0;i!=n_;++i)
                                perm.emplace_back(i);
                        return view_t{
                                n_,
                                sigma_,
                                detail::array_view<size_t>{ data_},
                                std::move(perm)
                        };
                }
        private:
                size_t& data_access(size_t i,size_t j){
                        return data_[i * n_ + j];
                }
                size_t n_=0;
                size_t sigma_ =0;
                std::vector< size_t > data_;
        };

        struct calculater{
                // rule of 6
                calculater();
                ~calculater();
                calculater(const calculater& that)=delete;
                calculater(calculater&& that);
                calculater& operator=(const calculater& that)=delete;
                calculater& operator=(calculater&& that);



                // how about some sugar
                template<class Players_Type>
                view_t calculate_hand_equity(Players_Type const& players){
                        return calculate_hand_equity_( detail::make_array_view(players) );
                }
                template<class Players_Type>
                view_t calculate_class_equity(Players_Type const& players){
                        return calculate_class_equity_(detail::make_array_view(players));
                }


                bool load(std::string const& name);
                bool save(std::string const& name)const;
                void append(calculater const& that);

                // It's nice to have nice things
                void json_dump(std::ostream& ostr)const;

        private:
                view_t calculate_hand_equity_(detail::array_view<holdem_id> const& players);
                view_t calculate_class_equity_(detail::array_view<holdem_id> const& players);

                std::unique_ptr<calculator_impl> impl_;
        };

        
}

#endif // PS_CALCULATOR_H 
