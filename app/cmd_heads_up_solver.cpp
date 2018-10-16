#include <thread>
#include <numeric>
#include <atomic>
#include <boost/format.hpp>
#include "ps/support/config.h"
#include "ps/base/frontend.h"
#include "ps/base/cards.h"
#include "ps/base/tree.h"
#include "ps/base/board_combination_iterator.h"
#include "ps/base/cards.h"
#include "ps/support/index_sequence.h"
#include "app/pretty_printer.h"
#include "ps/base/algorithm.h"
#include "ps/eval/instruction.h"
#include "ps/base/rank_hasher.h"
#include "ps/base/suit_hasher.h"
#include "ps/base/holdem_board_decl.h"
#include "ps/eval/computer_mask.h"
#include "ps/eval/computer_eval.h"
#include "ps/eval/class_cache.h"
#include "ps/detail/tree_printer.h"

#include <boost/timer/timer.hpp>

#include <boost/log/trivial.hpp>

#include <Eigen/Dense>
#include <fstream>

#include "ps/support/command.h"

/*
 * 2 player
        
        player ev
        =========
 
        ---+---------------
        f_ | 0
        pf | sb+bb
        pp | 2*S*Ev - (S-sb)

        ---+---------------
        pf | 0
        pp | 2*S*Ev - (S-bb)
        

        total value
        ===========


        ---+---------------
        f_ | -sb
        pf | bb
        pp | 2*S*Ev - S
        
        ---+---------------
        pf | -sb
        pp | 2*S*Ev - S





                2 players

                             P1         P2
                        -- (fold) -->                 f
                        -- (push) --> (fold) -->      pf
                        -- (push) --> (call) -->      pp
                             
                3 players
                             P1         P2         P3
                        -- (fold) --> (fold)              ff
                        -- (fold) --> (push) --> (fold)   fpf
                        -- (fold) --> (push) --> (call)   fpp
                        -- (push) --> (fold) --> (fold)   pff
                        -- (push) --> (fold) --> (call)   pfp
                        -- (push) --> (call) --> (fold)   ppf
                        -- (push) --> (call) --> (call)   ppp
 */

namespace ps{
namespace gt{


        struct eval_tree_node;

        struct gt_context{
                gt_context(size_t num_players, double eff, double sb, double bb)
                        :num_players_{num_players},
                        eff_(eff),
                        sb_(sb),
                        bb_(bb)
                {}
                size_t num_players()const{ return num_players_; }
                double eff()const{ return eff_; }
                double sb()const{ return sb_; }
                double bb()const{ return bb_; }

                gt_context& use_game_tree(std::shared_ptr<eval_tree_node> gt){
                        gt_ = gt;
                        return *this;
                }
                gt_context& use_cache(class_cache const& cc){
                        cc_ = &cc;
                        return *this;
                }
                
                std::shared_ptr<eval_tree_node> root()const{
                        return gt_;
                }
                class_cache const* cc()const{
                        return cc_;
                }
                
                friend std::ostream& operator<<(std::ostream& ostr, gt_context const& self){
                        ostr << "eff_ = " << self.eff_;
                        ostr << ", sb_ = " << self.sb_;
                        ostr << ", bb_ = " << self.bb_;
                        return ostr;
                }
        private:
                size_t num_players_;
                double eff_;
                double sb_;
                double bb_;

                std::shared_ptr<eval_tree_node> gt_;
                class_cache const* cc_;
        };


        struct eval_tree_node{
                enum{ MinusZero = 1024 };
                virtual void evaluate(Eigen::VectorXd& out,
                                      double p,
                                      gt_context const& ctx,
                                      holdem_class_vector const& vec,
                                      Eigen::VectorXd const& s)=0;
                eval_tree_node& times(size_t idx){
                        p_ticker_.push_back(static_cast<int>(idx));
                        return *this;
                }
                eval_tree_node& not_times(size_t idx){
                        if( idx == 0 ){
                                p_ticker_.push_back(MinusZero);
                        } else {
                                p_ticker_.push_back(-static_cast<int>(idx));
                        }
                        return *this;
                }
                double factor(Eigen::VectorXd const& s){
                        double result = 1.0;
                        for(auto op : p_ticker_){
                                if( op == MinusZero ){
                                        result *= ( 1. - s[0] );
                                } else if( op >= 0 ){
                                        result *= s[static_cast<size_t>(op)];
                                } else{
                                        result *= ( 1. - s[static_cast<size_t>(-op)] );
                                }
                        }
                        //std::cout << "factor(" << vector_to_string(s) << ", [" << detail::to_string(p_ticker_) << "]) => " << result << "\n";
                        return result;
                }
        private:
                // we encode -0 here
                std::vector<int> p_ticker_;
        };
        struct eval_tree_node_static : eval_tree_node{
                explicit eval_tree_node_static(Eigen::VectorXd vec):
                        vec_{vec}
                {}

                virtual void evaluate(Eigen::VectorXd& out,
                                      double p,
                                      gt_context const& ctx,
                                      holdem_class_vector const& vec,
                                      Eigen::VectorXd const& s)override
                {
                        auto q = factor(s);
                        p *= q;
                        for(size_t idx=0;idx!=vec_.size();++idx){
                                out[idx] += vec_[idx] * p;
                        }
                        //out += vec_ * p;
                        out[vec.size()] += p;
                }
        private:
                Eigen::VectorXd vec_;
        };

        struct eval_tree_node_eval : eval_tree_node{
                explicit
                eval_tree_node_eval(std::vector<size_t> mask)
                        :mask_{mask}
                {
                        v_mask_.resize(mask_.size());
                        v_mask_.fill(0);
                        for(size_t idx=0;idx!=mask.size();++idx){
                                v_mask_(mask[idx]) = 1.0;
                        }
                }
                virtual void evaluate(Eigen::VectorXd& out,
                                      double p,
                                      gt_context const& ctx,
                                      holdem_class_vector const& vec,
                                      Eigen::VectorXd const& s)override
                {
                        auto q = factor(s);
                        p *= q;
                        if( std::fabs(p) < 0.001 )
                                return;

                        auto ev = ctx.cc()->LookupVector(vec);

                        auto equity_vec = ( mask_.size() * ev - v_mask_ ) * ctx.eff() * p;

                        out += equity_vec;
                        out[vec.size()] += p;
                }
        private:
                std::vector<size_t> mask_;
                Eigen::VectorXd v_mask_;
        };

        struct eval_tree_non_terminal
                : public eval_tree_node
                , public std::vector<std::shared_ptr<eval_tree_node> >
        {
                virtual void evaluate(Eigen::VectorXd& out,
                                      double p,
                                      gt_context const& ctx,
                                      holdem_class_vector const& vec,
                                      Eigen::VectorXd const& s)override
                {
                        auto q = factor(s);
                        p *= q;
                        for(auto& ptr : *this){
                                ptr->evaluate(out, p, ctx, vec, s);
                        }
                }
        };
        
        struct hu_eval_tree_flat : eval_tree_non_terminal{
                explicit
                hu_eval_tree_flat( gt_context const& ctx){
                        Eigen::VectorXd v_f_{2};
                        v_f_(0) = -ctx.sb();
                        v_f_(1) =  ctx.sb();
                        auto n_f_ = std::make_shared<eval_tree_node_static>(v_f_);
                        n_f_->not_times(0);
                        push_back(n_f_);

                        Eigen::VectorXd v_pf{2};
                        v_pf(0) =  ctx.bb();
                        v_pf(1) = -ctx.bb();
                        auto n_pf = std::make_shared<eval_tree_node_static>(v_pf);
                        n_pf->times(0);
                        n_pf->not_times(1);
                        push_back(n_pf);

                        std::vector<size_t> m_pp;
                        m_pp.push_back(0);
                        m_pp.push_back(1);
                        auto n_pp = std::make_shared<eval_tree_node_eval>(m_pp);
                        n_pp->times(0);
                        n_pp->times(1);
                        push_back(n_pp);
                }
        };

        struct hu_eval_tree : eval_tree_non_terminal{
                explicit
                hu_eval_tree( gt_context const& ctx){


                        Eigen::VectorXd v_f_{2};
                        v_f_(0) = -ctx.sb();
                        v_f_(1) =  ctx.sb();
                        auto n_f_ = std::make_shared<eval_tree_node_static>(v_f_);
                        n_f_->not_times(0);
                        push_back(n_f_);

                        auto n_p_ = std::make_shared<eval_tree_non_terminal>();
                        n_p_->times(0);

                        Eigen::VectorXd v_pf{2};
                        v_pf(0) =  ctx.bb();
                        v_pf(1) = -ctx.bb();
                        auto n_pf = std::make_shared<eval_tree_node_static>(v_pf);
                        n_pf->not_times(1);
                        n_p_->push_back(n_pf);

                        std::vector<size_t> m_pp;
                        m_pp.push_back(0);
                        m_pp.push_back(1);
                        auto n_pp = std::make_shared<eval_tree_node_eval>(m_pp);
                        n_pp->times(1);
                        n_p_->push_back(n_pp);

                        push_back(n_p_);

                }
        };


        // returns a vector each players hand value
        Eigen::VectorXd combination_value(gt_context const& ctx,
                                          holdem_class_vector const& vec,
                                          Eigen::VectorXd const& s){
                Eigen::VectorXd result{vec.size()+1};
                result.fill(.0);
                ctx.root()->evaluate(result, 1., ctx, vec,s);

                return result;
        }

        Eigen::VectorXd unilateral_detail(gt_context const& ctx,
                                          size_t idx,
                                          std::vector<Eigen::VectorXd> const& S,
                                          double weighted = true)
        {
                Eigen::VectorXd result(169);
                result.fill(.0);
                        
                Eigen::VectorXd s(ctx.num_players());

                for(holdem_class_perm_iterator iter(ctx.num_players()),end;iter!=end;++iter){

                        auto const& cv = *iter;
                        auto p = cv.prob();
                        for(size_t idx=0;idx!=cv.size();++idx){
                                s[idx] = S[idx][cv[idx]];
                        }
                        auto meta_result = combination_value(ctx, cv, s);
                        result(cv[idx]) += p * meta_result[idx];
                }

                return result;
        }
        Eigen::VectorXd choose_push_fold(Eigen::VectorXd const& push, Eigen::VectorXd const& fold){
                Eigen::VectorXd result(169);
                result.fill(.0);
                for(holdem_class_id id=0;id!=169;++id){
                        if( push(id) >= fold(id) ){
                                result(id) = 1.0;
                        }
                }
                return result;
        }
        Eigen::VectorXd clamp(Eigen::VectorXd s){
                for(holdem_class_id idx=0;idx!=169;++idx){
                        s(idx) = ( s(idx) < .5 ? .0 : 1. );
                }
                return s;
        }
        
        static Eigen::VectorXd fold_s = Eigen::VectorXd::Zero(169);
        static Eigen::VectorXd push_s = Eigen::VectorXd::Ones(169);
        Eigen::VectorXd unilateral_maximal_exploitable(gt_context const& ctx, size_t idx, std::vector<Eigen::VectorXd> const& S)
        {

                auto copy = S;
                copy[idx] = fold_s;
                auto fold = unilateral_detail(ctx, idx, copy);
                copy[idx] = push_s;
                auto push = unilateral_detail(ctx, idx, copy);
                return choose_push_fold(push, fold);
        }





        

        struct solver{
                virtual std::vector<Eigen::VectorXd> step(gt_context const& ctx,
                                                          std::vector<Eigen::VectorXd> const& state)=0;
        };
        struct maximal_exploitable_solver : solver{
                explicit maximal_exploitable_solver(double factor = 0.05):factor_{factor}{}
                virtual std::vector<Eigen::VectorXd> step(gt_context const& ctx,
                                                          std::vector<Eigen::VectorXd> const& state)override
                {
                        auto bb_counter = unilateral_maximal_exploitable(ctx, 1, state);
                        auto tmp = state;
                        tmp[1] = bb_counter;
                        auto sb_counter = unilateral_maximal_exploitable(ctx, 0, tmp);
                        auto copy = state;
                        copy[0] *= ( 1 - factor_ );
                        copy[0] +=  factor_ * sb_counter;
                        copy[1]  = bb_counter;
                        return copy;
                }
        private:
                double factor_;
        };
        struct maximal_exploitable_solver_uniform : solver{
                explicit maximal_exploitable_solver_uniform(double factor = 0.05):factor_{factor}{}
                virtual std::vector<Eigen::VectorXd> step(gt_context const& ctx,
                                                          std::vector<Eigen::VectorXd> const& state)override
                {
                        std::vector<Eigen::VectorXd> result(state.size());
                        
                        for(size_t idx=0;idx!=state.size();++idx){

                                auto counter = unilateral_maximal_exploitable(ctx,idx, state);
                                result[idx] = state[idx] * ( 1.0 - factor_ ) + counter * factor_;
                        }
                        return result;
                }
        private:
                double factor_;
        };

        struct cond_single_strategy_lp1{
                using state_t = std::vector<Eigen::VectorXd>;
                cond_single_strategy_lp1(size_t idx, double epsilon)
                        :idx_(idx),
                        epsilon_(epsilon)
                {}
                bool operator()(state_t const& from, state_t const& to)const{
                        auto d = from[idx_] - to[idx_];
                        auto norm = d.lpNorm<1>();
                        auto cond = ( norm < epsilon_ );
                        std::cout << "norm => " << norm << "\n"; // __CandyPrint__(cxx-print-scalar,norm)
                        return cond;
                }
        private:
                size_t idx_;
                double epsilon_;
        };

        struct make_solver{
                
                enum{ DefaultMaxIter = 400 };

                using state_t         = std::vector<Eigen::VectorXd>;
                using step_observer_t = std::function<void(state_t const&)>;
                using stoppage_condition_t = std::function<bool(state_t const&, state_t const&)>;

                explicit make_solver(gt_context const& ctx){
                        ctx_ = &ctx;
                }
                make_solver& use_solver(std::shared_ptr<solver> s){
                        solver_ = s;
                        return *this;
                }
                make_solver& max_steps(size_t n){
                        max_steps_ = n;
                        return *this;
                }
                make_solver& init_state(std::vector<Eigen::VectorXd> const& s0){
                        state0_ = s0;
                        return *this;
                }
                make_solver& observer(step_observer_t obs){
                        obs_.push_back(obs);
                        return *this;
                }
                make_solver& stoppage_condition(stoppage_condition_t cond){
                        stop_cond_ = cond;
                        return *this;
                }
                std::vector<Eigen::VectorXd> run(){

                        BOOST_ASSERT(ctx_ );
                        BOOST_ASSERT(solver_ );
                        BOOST_ASSERT(state0_.size() );
                        BOOST_ASSERT( stop_cond_ );

                        std::vector<Eigen::VectorXd> state = state0_;
                        for(auto& _ : obs_){
                                _(state);
                        }

                        for(size_t idx=0;idx<max_steps_;++idx){

                                auto next = solver_->step(*ctx_, state);

                                if( stop_cond_(state, next) ){
                                        state[0] = clamp(state[0]);
                                        state[1] = clamp(state[1]);
                                        return state;
                                }
                                state = next;
                                for(auto& _ : obs_){
                                        _(state);
                                }

                        }

                        std::vector<Eigen::VectorXd> result;
                        result.push_back(Eigen::VectorXd::Zero(169));
                        result.push_back(Eigen::VectorXd::Zero(169));

                        BOOST_LOG_TRIVIAL(warning) << "Failed to converge solve ctx = " << *ctx_;
                        return result;
                }
        private:
                gt_context const* ctx_;
                std::shared_ptr<solver> solver_;
                std::vector<Eigen::VectorXd> state0_;
                std::vector<step_observer_t > obs_;
                stoppage_condition_t stop_cond_;
                size_t max_steps_{DefaultMaxIter};
        };




} // end namespace gt

struct HeadUpSolverCmd : Command{
        explicit
        HeadUpSolverCmd(std::vector<std::string> const& args):args_{args}{}
        virtual int Execute()override{
                class_cache cc;
	
                std::string cache_name{".cc.bin"};
                try{
                        cc.load(cache_name);
                }catch(std::exception const& e){
                        std::cerr << "Failed to load (" << e.what() << ")\n";
                        throw;
                }

                using namespace gt;

                size_t num_players = 2;

                // create a vector of num_players of zero vectors
                std::vector<Eigen::VectorXd> state0(num_players, Eigen::VectorXd::Zero(169));

                using result_t = std::future<std::tuple<double, std::vector<Eigen::VectorXd> > >;
                std::vector<result_t> tmp;


                auto enque = [&](double eff){
                        tmp.push_back(std::async([num_players, eff,&cc,&state0](){
                                gt_context gtctx(num_players, eff, .5, 1.);
                                auto root = std::make_shared<hu_eval_tree>(gtctx);
                                gtctx.use_game_tree(root);
                                gtctx.use_cache(cc);
                                auto result = make_solver(gtctx)
                                        .use_solver(std::make_shared<maximal_exploitable_solver_uniform>())
                                        .stoppage_condition(cond_single_strategy_lp1(0, 0.1))
                                        .init_state(state0)
                                        .run();
                                return std::make_tuple(eff, result);
                        }));
                };
                #if 0
                for(double eff = 10.0;eff <= 20.0;eff+=1){
                        enque(eff);
                }
                #else
                enque(20);
                #endif

                #if 0
                Eigen::VectorXd s0(169);
                s0.fill(.0);
                Eigen::VectorXd s1(169);
                s1.fill(.0);
                for(auto& _ : tmp){
                        auto aux = _.get();
                        auto eff = std::get<0>(aux);
                        auto const& vec = std::get<1>(aux);
                        for(size_t idx=0;idx!=169;++idx){
                                s0(idx) = std::max(s0(idx), eff*vec[0](idx));
                                s1(idx) = std::max(s1(idx), eff*vec[1](idx));
                        }
                }
                
                pretty_print_strat(s0, 1);
                pretty_print_strat(s1, 1);
                #endif

                for(auto& _ : tmp){
                        auto aux = _.get();
                        auto const& vec = std::get<1>(aux);
                        for(auto const& s : vec ){
                                pretty_print_strat(s, 2);
                        }
                }



                return EXIT_SUCCESS;
        }
private:
        std::vector<std::string> const& args_;
};
static TrivialCommandDecl<HeadUpSolverCmd> HeadsUpSolverCmdDecl{"heads-up-solver"};
        
} // end namespace ps