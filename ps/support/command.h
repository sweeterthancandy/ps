/*

CandyPoker
https://github.com/sweeterthancandy/CandyPoker

MIT License

Copyright (c) 2019 Gerry Candy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#ifndef PS_SUPPORT_COMMAND_H
#define PS_SUPPORT_COMMAND_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include <boost/exception/all.hpp>

namespace ps{


struct Command{
        virtual ~Command()=default;
        virtual int Execute()=0;
};

struct CommandDecl{
        CommandDecl(){
                World().push_back(this);
        }
        virtual std::shared_ptr<Command> TryMakeSingle(std::string const& name, std::vector<std::string> const& args)=0;
        virtual void PrintHelpSingle()const{
        }

        static std::vector<CommandDecl*>& World(){
                static std::vector<CommandDecl*>* ptr = new std::vector<CommandDecl*>{};
                return *ptr;
        }
        static std::shared_ptr<Command> TryMake(std::string const& name, std::vector<std::string> const& args){
                std::shared_ptr<Command> cmd;
                for( auto ptr : World()){
                        auto ret = ptr->TryMakeSingle(name, args);
                        if( ret ){
                                if( cmd ){
                                        BOOST_THROW_EXCEPTION(std::domain_error("not injective"));
                                }
                                cmd = ret;
                        }
                }
                return cmd;
        }
        static void PrintHelp(std::string const& exe_name){
                auto offset = exe_name.size();
                std::cout << exe_name << " <cmd> <option>...\n";
                for(auto ptr : World()){
                        ptr->PrintHelpSingle();
                }
        }
        static int Driver(int argc, char** argv){
                if( argc < 2 ){
                        PrintHelp(argv[0]);
                        return EXIT_FAILURE;
                }
                std::string cmd_s = argv[1];
                std::vector<std::string> args(argv+2, argv+argc);
                auto cmd = TryMake(cmd_s, args);
                if( ! cmd ){
                        std::cerr << "can't find cmd " << cmd_s << "\n";
                        PrintHelp(argv[0]);
                        return EXIT_FAILURE;
                }
                try{
                        return cmd->Execute();
                } catch(std::exception const& e){
                        std::cerr << "Exception: " << e.what() << "\n";
                        return EXIT_FAILURE;
                }
        }
};
template<class T>
struct TrivialCommandDecl : CommandDecl{
        explicit TrivialCommandDecl(std::string const& name):name_{name}{}
        virtual void PrintHelpSingle()const override{
                std::cout << "    " << name_ << "\n";
        }
        virtual std::shared_ptr<Command> TryMakeSingle(std::string const& name, std::vector<std::string> const& args)override{
                if( name != name_)
                        return std::shared_ptr<Command>{};
                return std::make_shared<T>(args);
        }
private:
        std::string name_;
};

} // end namespace ps

#endif // PS_SUPPORT_COMMAND_H
