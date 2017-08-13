#ifndef PS_EVAL_HOLDEM_CLASS_EVAL_CACHE_H
#define PS_EVAL_HOLDEM_CLASS_EVAL_CACHE_H

#include <fstream>

#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/export.hpp>

namespace ps{
        
struct holdem_class_eval_cache{

        equity_breakdown* lookup(holdem_class_vector const& vec){
                auto iter = cache_.find(vec);
                if( iter == cache_.end())
                        return nullptr;
                return &iter->second;
        }
        void commit(holdem_class_vector vec, equity_breakdown const& breakdown){
                std::cout << "breakdown = " <<  breakdown << "\n";
                cache_.emplace(std::move(vec), breakdown);
                display();
        }

        void display()const{
                std::cout << "DISPLAY BEGIN\n";
                for( auto const& item : cache_ ){
                        PRINT(item.first);
                        std::cout << item.second << "\n";
                }
                std::cout << "DISPLAY END\n";
        }

        // work with std::lock_guard etc
        void lock(){
                mtx_.lock();
        }
        void unlock(){
                mtx_.unlock();
        }
        bool load(std::string const& name){
                this->lock();
                std::ifstream is(name);
                if( ! is.is_open() )
                        return false;
                boost::archive::text_iarchive ia(is);
                ia >> *this;
                this->unlock();
                return true;
        }
        bool save(std::string const& name){
                this->lock();
                std::ofstream of(name);
                if( ! of.is_open() )
                        return false;
                boost::archive::text_oarchive oa(of);
                oa << *this;
                this->unlock();
                return true;
        }
        template<class Archive>
        void serialize(Archive& ar, unsigned int){
                ar & cache_;
        }
private:
        mutable std::mutex mtx_;
        std::map< holdem_class_vector, equity_breakdown_matrix> cache_;
};

} // ps
#endif // PS_EVAL_HOLDEM_CLASS_EVAL_CACHE_H