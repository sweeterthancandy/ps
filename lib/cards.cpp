#include "ps/cards.h"

namespace{
        // random access, means
        //      {id=0, key=4}, {id=2,key=5} needs
        //      {4,-1,5} etc
        template<class T, class Key = ps::id_type>
        struct decl_factory{
                template<class... Args>
                explicit decl_factory(Args&&... args)
                        : vec_{std::forward<Args>(args)...}
                {
                        boost::sort( vec_ );
                }
                T const& get(Key k)const{
                        assert( k < vec_.size() && "key doesn't exist");
                        return vec_[k];
                }
        private:
                std::vector<T> vec_;
        };
}

namespace ps{
        suit_decl const& suit_decl::get(suit_id id){
                using namespace decl;
                static decl_factory<suit_decl> fac{_h, _d, _c, _s};
                return fac.get(id);
        }
        suit_decl const& suit_decl::parse(std::string const& s){
                assert(s.size()==1 && "preconditon failed");
                using namespace decl;
                switch(s.front()){
                        case 'h': case 'H': return _h;
                        case 'd': case 'D': return _d;
                        case 'c': case 'C': return _c;
                        case 's': case 'S': return _s;
                        default:
                                BOOST_THROW_EXCEPTION(std::domain_error("not a suit (" + s + ")"));
                }
        }
        
        rank_decl const& rank_decl::get(rank_id id){
                using namespace decl;
                static decl_factory<rank_decl> fac{_2,_3,_4,_5,_6,
                                                   _7,_8,_9,_T,_J,
                                                   _Q,_K,_A};
                return fac.get(id);
        }
        rank_decl const& rank_decl::parse(std::string const& s){
                assert(s.size()==1 && "preconditon failed");
                using namespace decl;
                switch(s.front()){
                        case '2': return _2;
                        case '3': return _3;
                        case '4': return _4;
                        case '5': return _5;
                        case '6': return _6;
                        case '7': return _7;
                        case '8': return _8;
                        case '9': return _9;
                        case 'T': case 't': return _T;
                        case 'J': case 'j': return _J;
                        case 'Q': case 'q': return _Q;
                        case 'K': case 'k': return _K;
                        case 'A': case 'a': return _A;
                        default:
                                BOOST_THROW_EXCEPTION(std::domain_error("not a rank (" + s + ")"));
                }
        }
        
        card_decl const& card_decl::get(card_id id){
                using namespace decl;
                static decl_factory<card_decl> fac{
                        _Ah, _Kh, _Qh, _Jh, _Th, _9h, _8h, _7h, _6h, _5h, _4h, _3h, _2h,
                        _Ad, _Kd, _Qd, _Jd, _Td, _9d, _8d, _7d, _6d, _5d, _4d, _3d, _2d,
                        _Ac, _Kc, _Qc, _Jc, _Tc, _9c, _8c, _7c, _6c, _5c, _4c, _3c, _2c,
                        _As, _Ks, _Qs, _Js, _Ts, _9s, _8s, _7s, _6s, _5s, _4s, _3s, _2s
                };
                return fac.get(id);
        }
        holdem_hand_decl const& holdem_hand_decl::parse(std::string const& s){
                assert( s.size() == 4 && "precondition failed");
                auto x = card_decl::parse(s.substr(0,2)).id();
                auto y = card_decl::parse(s.substr(2,2)).id();
                return get(make_id(x,y));
        }
        holdem_id holdem_hand_decl::make_id( rank_id r0, suit_id s0,
                                rank_id r1, suit_id s1)
        {
                holdem_id x{ card_decl::make_id(s0, r0) };
                holdem_id y{ card_decl::make_id(s1, r1) };
                return make_id(x,y);
        }
        
        holdem_hand_decl const& holdem_hand_decl::get(holdem_id id){
                static decl_factory<holdem_hand_decl> fac{
                        [](){
                                std::vector< holdem_hand_decl> aux;
                                for( char a{52};a!=0;){
                                        --a;
                                        for( char b{52};b!=0;){
                                                --b;
                                                aux.emplace_back( card_decl::get(a), card_decl::get(b));
                                        }
                                }
                                return std::move(aux);
                        }()
                };
                return fac.get(id);
        }

        holdem_class_id holdem_class_decl::make_id(holdem_class_type cat, rank_id x, rank_id y){
                using std::get;
                static auto aux{[](){
                        std::vector<std::tuple<int,rank_id, rank_id, holdem_class_id> > vec;
                        holdem_class_id id{0};
                        for(rank_id a{13};a!=0;){
                                --a;
                                vec.emplace_back(0,a,a,id++);
                        }
                        for( unsigned a{13};a!=1;){
                                --a;
                                for( unsigned b{a};b!=0;){
                                        --b;
                                        vec.emplace_back(1,a,b,id++);
                                        vec.emplace_back(2,a,b,id++);
                                }
                        }
                        PRINT(vec.size());
                        boost::sort(vec);
                        for( auto const& t : vec ){
                                PRINT_SEQ((get<0>(t))(get<1>(t))(get<2>(t))(get<3>(t)));
                        }
                        return std::move(vec);
                }()};
                if( x < y )
                        std::swap(x,y);
                 
                PRINT_SEQ((static_cast<int>(cat))(x)(y));
                for( auto const& t : aux ){
                        if( get<0>(t) == static_cast<int>(cat) &&
                            get<1>(t) == x &&
                            get<2>(t) == y ){
                                return get<3>(t);
                        }
                }
                BOOST_THROW_EXCEPTION(std::domain_error("not a tuple"));
        }
        holdem_class_decl const& holdem_class_decl::get(holdem_id id){
                static decl_factory<holdem_class_decl> fac{
                        [](){
                                std::vector<holdem_class_decl> aux;
                                for( rank_id a{13};a!=0;){
                                        --a;
                                        aux.emplace_back( holdem_class_type::pocket_pair,
                                                          rank_decl::get(a),
                                                          rank_decl::get(a) );
                                }
                                for( rank_id a{13};a!=1;){
                                        --a;
                                        for( rank_id b{a};b!=0;){
                                                --b;
                                                aux.emplace_back( holdem_class_type::suited,
                                                                  rank_decl::get(a),
                                                                  rank_decl::get(b) );
                                                aux.emplace_back( holdem_class_type::offsuit,
                                                                  rank_decl::get(a),
                                                                  rank_decl::get(b) );
                                        }
                                }
                                return std::move(aux);
                        }()
                };
                return fac.get(id);
        }


                
                
        holdem_class_decl::holdem_class_decl(holdem_class_type cat,
                          rank_decl const& a,
                          rank_decl const& b):
                id_{ make_id( cat, a.id(), b.id() ) },
                cat_{cat},
                first_{a},
                second_{b}
        {
                //PRINT_SEQ((id_)(*this));

                switch(cat_){
                case holdem_class_type::suited:
                        for(suit_id x{0};x!=4;++x){
                                hand_set_.emplace_back(
                                        card_decl(
                                                suit_decl::get(x), first_),
                                        card_decl(
                                                suit_decl::get(x), second_));
                        }
                        break;
                case holdem_class_type::offsuit:
                        for(suit_id x{0};x!=4;++x){
                                for(suit_id y{0};y!=4;++y){
                                        if( x == y )
                                                continue;
                                        hand_set_.emplace_back(
                                                card_decl(
                                                        suit_decl::get(x), first_),
                                                card_decl(
                                                        suit_decl::get(y), second_));
                                }
                        }
                        break;
                case holdem_class_type::pocket_pair:
                        for(suit_id x{0};x!=4;++x){
                                for(suit_id y{static_cast<suit_id>(x+1)};y!=4;++y){
                                        hand_set_.emplace_back(
                                                card_decl(
                                                        suit_decl::get(x), first_),
                                                card_decl(
                                                        suit_decl::get(y), first_));
                                }
                        }
                        break;
                }
        }

        std::string holdem_class_decl::to_string()const{
                std::string tmp{
                        first_.to_string() + 
                        second_.to_string()};
                switch(cat_){
                case holdem_class_type::suited:
                        tmp += 's';
                        break;
                case holdem_class_type::offsuit:
                        tmp += 'o';
                        break;
                case holdem_class_type::pocket_pair:
                        break;
                }
                return std::move(tmp);
        }
        holdem_class_decl const& holdem_class_decl::parse(std::string const& s){
                auto x = rank_decl::parse(s.substr(0,1)).id();
                auto y = rank_decl::parse(s.substr(1,1)).id();
                PRINT_SEQ((s)(x)(y));
                switch(s.size()){
                case 2:
                        return get(make_id(holdem_class_type::pocket_pair, x, y) );
                case 3:
                        switch(s[2]){
                        case 's': 
                                return get(make_id(holdem_class_type::suited, x, y) );
                        case 'o': 
                                return get(make_id(holdem_class_type::offsuit, x, y) );
                        default:
                                BOOST_THROW_EXCEPTION(std::domain_error("not a holdem hand cat (" + s + ")"));
                        }
                default:
                        BOOST_THROW_EXCEPTION(std::domain_error("not a holdem hand cat (" + s + ")"));
                }
        }
} // 