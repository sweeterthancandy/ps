#ifndef PS_EVAL_HOLDEM_CLASS_VECTOR_CACHE_H
#define PS_EVAL_HOLDEM_CLASS_VECTOR_CACHE_H

#include "ps/base/cards.h"
#include "ps/support/persistent.h"

namespace boost{
namespace serialization{
        struct access;
} // end namespace serialization
} // end namespace boost

namespace ps{

struct holdem_class_vector_cache_item{
        friend std::ostream& operator<<(std::ostream& ostr, holdem_class_vector_cache_item const& self){
                ostr << "cv = " << self.cv;
                ostr << ", count = " << self.count;
                ostr << ", prob = " << self.prob;
                ostr << ", ev = " << detail::to_string(self.ev);
                return ostr;
        }
        holdem_class_vector cv;
        size_t count{0};
        double prob{0};
        std::vector<double> ev;
private:
        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version){
                ar & cv;
                ar & count;
                ar & prob;
                ar & ev;
        }
};
struct holdem_class_vector_cache_item_pair{
        holdem_class_id cid;
        std::vector<holdem_class_vector_cache_item> vec;
private:
        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version){
                ar & cid;
                ar & vec;
        }
};
using holdem_class_vector_cache = std::vector<holdem_class_vector_cache_item>;

using holdem_class_vector_pair_cache = std::vector<holdem_class_vector_cache_item_pair>;

extern support::persistent_memory_decl<holdem_class_vector_pair_cache> Memory_ThreePlayerClassVector;
extern support::persistent_memory_decl<holdem_class_vector_pair_cache> Memory_TwoPlayerClassVector;

} // end namespace ps



#endif // PS_EVAL_HOLDEM_CLASS_VECTOR_CACHE_H