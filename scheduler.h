#ifndef CACHE_SCHEDULER_H
#define CACHE_SCHEDULER_H

#include <cstdio>
#include <bitset>
#include <unordered_map>
namespace cache{
    const int _cacheSize=1024;
    enum class Algorithms{LRU,LRF,CLOCK};
    using frame_id_t=size_t;
    using cache_id_t=size_t;



    class Mscheduler_{
    public:

        Mscheduler_(size_t gid,size_t n=256,Algorithms a=Algorithms::CLOCK):alg(a),groupsize_(n),hit(0){
            pos=0;
            visits=0;
            hit=0;
            used=0;
            begin= nullptr;
            offset=groupsize_*(gid);
            accessCount=0;
        }


        struct Node{
            Node* pre;
            Node* next;
            Node():pre(nullptr),next(nullptr){};
        };

        struct block{
            frame_id_t fid;
            bool dirty:1;
            bool used:1;
            bool visits:1;
            Node n;
            block():fid(0),dirty(false),used(false),visits(true){};
        };

        typedef  Node* iterator;
        typedef  block* block_itertor;

        void read(frame_id_t id){
            ++visits;
            switch (alg) {
                case Algorithms::LRU:
                    LruVisit(id, false);
                    break;
                case Algorithms::LRF:
                    LrfVisit(id, false);
                    break;
                case Algorithms::CLOCK:
                    ClockVisit(id, false);
                    break;
            }
        }

        void write(frame_id_t id){
            ++visits;
            switch (alg) {
                case Algorithms::LRU: {
                    LruVisit(id, true);
                    break;
                }
                case Algorithms::LRF:
                    LrfVisit(id, true);
                    break;
                case Algorithms::CLOCK:
                    ClockVisit(id, true);
                    break;
            }
        }

        size_t getaccessaccount()const{
            return accessCount;
        }

        size_t getvisits() const{
            return visits;
        }

        size_t gethit() const{
            return hit;
        }

    private:
        bool isempty() const{
            return used==0;
        }

        bool isfull() const{
            return used==groupsize_;
        }

        void LruVisit(frame_id_t fid,bool tag){
            if(!isfull()&&memery2cache.find(fid)==memery2cache.end()){
                addframeLru(fid);
            }else if(memery2cache.find(fid)!=memery2cache.end()){
                updateLru(fid);
            }else{
                swapLru(fid);
            }
            if(tag){
                setDirty(fid);
            }
        }

        void setDirty(frame_id_t fid){
            cache_id_t cid=memery2cache[fid];
            _cache[cid].dirty= true;
        }

        block& getBlock(iterator& b){
            block* blo= _parent_class_of(b,&block::n);
            return *blo;
        }

        void swapLru(frame_id_t fid){
            cache_id_t cid=memery2cache[fid];
            if(_cache[cid].dirty){
                ++accessCount;
            }
            unorderedMaperase(fid,memery2cache);
            iterator n=&(_cache[cid].n);
            store(cid,fid);
            popnode(begin,n);
            insertTail(begin,n);
        }

        iterator popHead(iterator& head){
            auto temp=head;
            if(head->next==head){
                head = nullptr;
            }else{
                head->next->pre=head->pre;
                head->pre->next=head->next;
                head=head->next;
            }
            return temp;
        }

        template<typename K,typename V>
        void unorderedMaperase(K& k,std::unordered_map<K,V>& map){
            map.erase(map.find(k));
        }


        ptrdiff_t  getPos(block_itertor&& b){
            return static_cast<block_itertor>(b)-static_cast<block_itertor>(&_cache[0]);
        }

        void store(cache_id_t cid,frame_id_t fid){
            _cache[cid].used= true;
            _cache[cid].fid=fid;
            _cache[cid].dirty= false;
            _cache[cid].visits= true;
            ++accessCount;
            memery2cache[fid]=cid;
        }

        template<class P,class Q>
        constexpr P* _parent_class_of( const Q* ptr, const Q P::*member) const {
            return reinterpret_cast<P*>(reinterpret_cast<char*>(const_cast<Q*>(ptr))- _offset_in_class(member));
        }

        template<class P,class Q>
        constexpr size_t _offset_in_class(const Q P::*memeber) const{
            return (size_t)&(reinterpret_cast<P*>(0)->*memeber);
        }


        void updateLru(frame_id_t fid){
            ++hit;
            cache_id_t cid=memery2cache[fid];
            iterator n=&(_cache[cid].n);
            popnode(begin,n);
            insertTail(begin,n);
        }

        void popnode(iterator &head,iterator& n){
            if(n->next==n){
                head=nullptr;
            }else{
                n->next->pre=n->pre;
                n->pre->next=n->next;
                if(head==n){
                    head=n->next;
                }
            }
        }

        void addframeLru(frame_id_t fid){
            ++used;
            store(offset+pos,fid);
            iterator n=&(_cache[offset+pos].n);
            insertTail(begin,n);
            pos=(pos+1)%groupsize_;
        }

        void insertHead(iterator& head,iterator n){
            if(head== nullptr){
                head=n;
                n->next=n->pre=n;
            }else{
                n->pre=head->pre;
                n->next=head;
                n->pre->next=n;
                n->next->pre=n;
                head=n;
            }
        }

        void insertTail(iterator& head,iterator n){
            if(head== nullptr){
                head=n;
                n->next=n->pre=n;
            }else{
                n->pre=head->pre;
                n->next=head;
                n->pre->next=n;
                n->next->pre=n;
            }
        }


        void LrfVisit(frame_id_t id,bool tag){
            if(memery2cache.find(id)!=memery2cache.end()){
                updateLrf(id);
            }else if(!isfull()&&memery2cache.find(id)==memery2cache.end()){
                addframeLrf(id);
            }else{
                swapLrf(id);
            }
            if(tag){
                setDirty(id);
            }
        }

        void updateLrf(frame_id_t fid){
            ++hit;
            cache_id_t cid=memery2cache[fid];
            size_t fre=frequently[fid];
            ++frequently[fid];
            iterator n= &(_cache[cid].n);
            popnode(frequency2List[fre],n);
            if(frequency2List.find(fre+1)==frequency2List.end()){
                frequency2List[fre+1]= nullptr;
            }
            insertTail(frequency2List[fre+1],n);
        }



        void addframeLrf(frame_id_t fid){
            ++used;
            cache_id_t cid=pos+offset;
            store(cid,fid);
            frequently[fid]=1;
            iterator n=&(_cache[cid].n);
            if(frequency2List.find(1)==frequency2List.end()){
                frequency2List[1]=nullptr;
            }
            insertTail(frequency2List[1],n);
            pos=(pos+1)%_cacheSize;
        }

        void swapLrf(frame_id_t fid){
            size_t fre;
            for(fre=1;;++fre){
                if(frequency2List[fre]){
                    break;
                }
            }
            iterator n=popHead(frequency2List[fre]);
            auto& block=getBlock(n);
            frame_id_t prefid=block.fid;
            unorderedMaperase(prefid,frequently);
            frequently[fid]=1;
            unorderedMaperase(prefid,memery2cache);
            if(block.dirty){
                ++accessCount;
            }

            store(getPos(static_cast<block_itertor>(&block)),fid);
            insertTail(frequency2List[1],n);
        }

        void ClockVisit(frame_id_t fid,bool tag){
            if(memery2cache.find(fid)!=memery2cache.end()){
                clockupdate(fid);
            }else if(!isfull()){
                clockadd(fid);
            }else{
                clockswap(fid);
            }
            if(tag){
                setDirty(fid);
            }
        }

        void clockupdate(frame_id_t fid){
            ++hit;
            cache_id_t  cid=memery2cache[fid];
            _cache[cid].visits= true;
        }

        void clockswap(frame_id_t fid){
            cache_id_t  cid=findblock();
            if(_cache[cid].dirty){
                ++accessCount;
            }
            unorderedMaperase(_cache[cid].fid,memery2cache);
            _cache[cid].fid=fid;
            memery2cache[fid]=cid;
        }

        cache_id_t findblock(){
            size_t tmp=swap;
            size_t circle=0;
            while(circle<=2){
                if(!_cache[tmp+offset].dirty&&circle<=1&&!_cache[tmp+offset].visits){
                    swap=(tmp+1)%groupsize_;
                    break;
                }else if(!_cache[tmp+offset].dirty&&circle==2){
                    swap=(tmp+1)%groupsize_;
                    break;
                }
                _cache[tmp+offset].visits= false;
                tmp=(tmp+1)%groupsize_;
                if(tmp==swap){
                    ++circle;
                }
            }
            return tmp+offset;
        }

        void clockadd(frame_id_t fid){
            memery2cache[fid]=pos+offset;
            pos=(pos+1)%groupsize_;
            store(pos+offset,fid);
        }

        std::unordered_map<frame_id_t,size_t>  memery2cache;
        std::unordered_map<frame_id_t,size_t> frequently;
        std::unordered_map<size_t,iterator>frequency2List;

        Algorithms alg;
        size_t groupsize_;
        cache_id_t pos;
        size_t hit;
        size_t visits;//command
        size_t used;
        iterator begin;
        cache_id_t offset;
        size_t accessCount;
        size_t swap;
        inline static std::array<block,_cacheSize> _cache;
    };


}




#endif //CACHE_SCHEDULER_H