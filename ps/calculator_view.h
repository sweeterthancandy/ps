#ifndef PS_CALCULATOR_VIEW_H
#define PS_CALCULATOR_VIEW_H

#include <vector>
#include <ostream>
#include <numeric>

#include "ps/support/array_view.h"

namespace ps{
struct detailed_view_type{

        using perm_type = std::vector<int>;

        struct player_view_t{
                explicit player_view_t(size_t n, size_t sigma, support::array_view<size_t> const& data)
                        :n_{n}, sigma_{sigma}, data_{data}
                {}
                double equity()const{
                        double result{0.0};
                        for(size_t i=0;i!=n_;++i){
                                result += nwin(i) / (i+1);
                        }
                        return result / sigma_;
                }
                // nwin(0) -> wins
                // nwin(1) -> draws to split pot 2 ways
                // nwin(2) -> draws to split pot 3 ways
                // ...
                size_t nwin(size_t idx)const{
                        return data_[idx];
                }
                size_t win()const{  return nwin(0); }
                size_t draw()const{ return nwin(1); }
                size_t lose()const{ return sigma_ - std::accumulate( data_.begin(), data_.end(), 0); }
                size_t sigma()const{ return sigma_; }

        private:
                size_t n_;
                size_t sigma_;
                support::array_view<size_t> data_;
        };
        

        explicit detailed_view_type(size_t n, size_t sigma, support::array_view<size_t> const& data, std::vector<int> const& perm)
                : n_{n}
                , sigma_{sigma}
                , data_{ data }
        {
                perm_.resize(n_);
                for(size_t i=0;i!=n_;++i){
                        perm_[perm[i]] = i;
                }
        }
        explicit detailed_view_type(size_t n, size_t sigma, support::array_view<size_t> const& data)
                : n_{n}
                , sigma_{sigma}
                , data_{ data }
        {
        }
                
        auto player(size_t idx)const{
                return player_view_t{n_,
                                     sigma_,
                                     support::array_view<size_t>{data_.begin() + n_ * map_idx_(idx), n_ }};
        }
        auto sigma()const{ return sigma_; }
        auto n()const{ return n_; }

        friend std::ostream& operator<<(std::ostream& ostr, detailed_view_type const& self);
private:
        size_t map_idx_(size_t idx)const{
                if( perm_.empty())
                        return idx;
                return perm_[idx];
        }
        size_t n_;
        size_t sigma_;
        support::array_view<size_t> data_;
        perm_type perm_;
};

} // ps 
#endif // PS_CALCULATOR_VIEW_H