//
// Created by student on 2023/9/4.
// https://github.com/purecpp-org/easylog/tree/main/include
//

#ifndef CACHE_LOG_H
#define CACHE_LOG_H
#include <cassert>
#include <cstdio>
#include <chrono>
 namespace cache{

    enum class level{M_INFO=0,M_WARNING=1,M_ERROR=2,M_FATAL=3};
    struct Loginfo{
        level l;
        std::string info;
        std::time_t t_c;
    };
     using LogSeverity=level;
        class Logger{
        public:
            static void Log(){

            }

            static void LogInfo(){

            }
            static void LogWarning(){

            }
            void log(LogSeverity serverity,const std::string& message){
                switch (serverity) {
                    case level::M_INFO:

                        break;
                    case level::M_WARNING:
                        break;
                    case level::M_ERROR:
                        break;
                    case level::M_FATAL:
                        break;
                }
            }

            void _write_time(){
                auto end=std::chrono::high_resolution_clock::now();
                auto duration=std::chrono::duration_cast<std::chrono::microseconds>(end-start);

            }
            void _write_log_info(const std::string& message){

            }

            void _write_log_warning(const std::string& message){

            }

            void _write_log_error(const std::string& message){

            }

            void _write_log_fatal(const std::string& message){

            }

            std::string& _convert_time2str(){

            }


            static Logger* getInstance(){
                static Logger log;
                return  &log;
            }
        private:
            std::string path;
            static Logger instance;
            std::ofstream logFile;
            Logger(const std::string& _path="log.txt"){
                path=_path;
                logFile.open(path,std::ios::app);
                if(!logFile.is_open()){
                    std::cerr << "Error: Could not open log file " << path << std::endl;
                }else{
                    std::cout<<"=========Log Loading:all information will be recorded in file "<<_path<<"===\n";
                }
                start=std::chrono::high_resolution_clock::now();
            }
            std::chrono::time_point<std::chrono::high_resolution_clock> start;

            std::mutex m1,m2;
        };


}

#endif //CACHE_LOG_H
