#include <iostream>
#include <list>
#include "threadPool.h"
#include "fileManager.h"
#include "scheduler.h"

#define THREAD_NUM 4
using frame_id_t=cache::frame_id_t;
std::condition_variable_any cv;
bool condition1= false;
bool condition2= false;


struct commandM{
    frame_id_t fid;
    size_t manner:1;
    commandM(frame_id_t _fid,size_t _manner):fid(_fid),manner(_manner){};
};


struct myList{
    std::mutex m;
    std::queue<commandM> command;
};

struct res{
    size_t accesscount;
    size_t visits;
    size_t hit;
    res(size_t _acc=0,size_t _vis=0,size_t _hi=0):accesscount(_acc),visits(_vis),hit(_hi){};
};

static std::vector<myList> m(THREAD_NUM);
void readCommand();
res processCommand(size_t gid);

int main() {
    threadPool::thread_pool tp(THREAD_NUM+1);
    threadPool::multi_future<res> mf(4);
    tp.submit(readCommand);
    mf[0]=tp.submit(processCommand,0);
    mf[1]=tp.submit(processCommand,1);
    mf[2]=tp.submit(processCommand,2);
    mf[3]=tp.submit(processCommand,3);
    tp.wait_for_tasks();
    res r;
    for(size_t i=0;i<THREAD_NUM;i++){
        res tmp=mf[i].get();
        r.hit+=tmp.hit;
        r.visits+=tmp.visits;
        r.accesscount+=tmp.accesscount;
    }
    std::cout<<"The hit rating is :"<<(double)r.hit/r.visits<<'\n';
    return 0;
}

void readCommand(){
    cache::CacheReader<char> reader;
    size_t manner;
    frame_id_t fid;
    size_t queueId;
    char delimiter;
    while(reader.read(manner)&&reader.read(delimiter)&&reader.read(fid)){
        queueId=fid%THREAD_NUM;
        std::unique_lock<std::mutex> lock(m[queueId].m);
        m[queueId].command.emplace(fid,manner);
        condition1= true;
        cv.notify_all();
    }
    condition2= true;
    cv.notify_all();
}

res processCommand(size_t gid){
    cache::Mscheduler_ mscheduler(gid);
    std::queue<commandM> q;
    while(true){
        std::unique_lock<std::mutex> lock(m[gid].m);
        cv.wait(lock,[&](){return (condition1&&!m[gid].command.empty())||
                (condition2&&m[gid].command.empty());});
        if(condition1||!m[gid].command.empty()){
            while (!m[gid].command.empty()){
                q.push(m[gid].command.front());
                m[gid].command.pop();
            }
            lock.unlock();
            while(!q.empty()){
                if(q.front().manner==1){
                    mscheduler.write(q.front().fid);
                }else{
                    mscheduler.read(q.front().fid);
                }
                q.pop();
            }
        }
        if(condition2){
            break;
        }
    }
    std::cout<<mscheduler.getvisits()<<"---"<<mscheduler.gethit()<<"---"<<mscheduler.getaccessaccount()<<'\n';
    return {mscheduler.getaccessaccount(),mscheduler.getvisits(),mscheduler.gethit()};
}









