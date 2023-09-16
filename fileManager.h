//
// Created by student on 2023/8/31.
//

#ifndef CACHE_FILEMANAGER_H
#define CACHE_FILEMANAGER_H
#include <iostream>
#include <string>
#include <fstream>
#include "Log.h"


namespace cache {
    template <typename chart,typename traits=std::char_traits<chart>>
    class CacheReader{
        public:
            CacheReader(std::string&& path="E:/code/C++/Cache/data-5w-50w.txt"):fp(std::make_unique<std::ifstream>(path
            ,std::ios::in | std::ios::binary)){
                if(!fp->is_open()){
                    //
                }
                //
            }

        template<typename T>
            bool read(T& t){
                if(*fp>>t){
                    return true;
                }else if(fp->fail()) {

                }
            return false;
            }

            ~CacheReader(){
                if(fp->is_open()){
                    fp->close();
                }
            }

        private:
           std::unique_ptr<std::ifstream> fp;
        };

}
#endif //CACHE_FILEMANAGER_H
