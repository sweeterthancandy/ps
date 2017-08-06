#ifndef PS_EVAL_CLASS_EQUITY_EVALUATOR_PRINCIPAL_H
#define PS_EVAL_CLASS_EQUITY_EVALUATOR_PRINCIPAL_H

#include "ps/eval/equity_evaluator.h"
#include "ps/eval/equity_breakdown_matrix.h"
#include "ps/eval/class_equity_evaluator.h"

namespace ps{

struct class_equity_evaluator_principal : class_equity_evaluator{
        class_equity_evaluator_principal(){
                impl_ = &equity_evaluator_factory::get("cached");
        }
        virtual std::shared_ptr<equity_breakdown> evaluate(holdem_class_vector const& players)const override{
                auto result = std::make_shared<equity_breakdown_matrix_aggregator>(players.size());
                for( auto hv : players.get_hand_vectors()){
                        result->append( *impl_->evaluate(hv) );
                }
                return result;
        }
private:
        equity_evaluator const* impl_;
};

} // ps

#endif // PS_EVAL_CLASS_EQUITY_EVALUATOR_PRINCIPAL_H
