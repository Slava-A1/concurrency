#ifndef PREFIX_SCAN_H
#define PREFIX_SCAN_H

#include <vector>
#include <iostream>
#include <pthread.h>
#include <algorithm>
#include <atomic>

extern int nThreads;
extern bool myBarrier;
extern bool doTwoLevel; //Could this just be a flag for launch_threads ?

template<typename T>
struct ElemAdder{
    T operator()(const T& first, const T& second){
        return first + second;
    }
};

//The additional vectors/copies created here are intentional
template<typename T>
struct VectorAdder{
    std::vector<T> operator()(const std::vector<T>& first, const std::vector<T>& second){
        
        const int n = first.size();
        if(n == 0){
            return second;
        }
        else if(second.size() == 0){
            return first;  
        }
        
        std::vector<T> output(n);
        
        for(int i = 0; i < n; ++i){
            output[i] = first[i] + second[i];
        }

        return output;
    }
};

//Dest is intentionaly a copy so that we can return it
template<typename T>
struct MutVectorAdder{
    std::vector<T> operator()(std::vector<T> dest, std::vector<T>& other){
        
        const int n = dest.size();
        if(n == 0){
            //Copy should ideally be fine here?
            return other;
        }
        else if(other.size() == 0){
            return dest;
        }

        for(int i = 0; i < n; ++i){
            dest[i] += other[i];
        }
        
        return dest;
    }
};

template<typename T, typename Func>
T scan_op(T& first, T& second){
    Func operation;
    return operation(first, second);
}


class Sbarrier{
private:
    int baseCount;
    std::atomic_int counter;
    pthread_spinlock_t lock1, lock2;
public:
    Sbarrier() = delete; //Should not be able to make an empty barrier
    
    //Hopefully this should be fine
    //They are all technically deleted
    Sbarrier(const Sbarrier& other) = default;
    Sbarrier(Sbarrier&& other) = default;
    Sbarrier& operator=(const Sbarrier& other) = default;
    Sbarrier& operator=(Sbarrier&& other) = default;

    Sbarrier(int count){
        this->baseCount = count;
        counter.store(count);
        pthread_spin_init(&lock1, PTHREAD_PROCESS_PRIVATE);
        pthread_spin_init(&lock2, PTHREAD_PROCESS_PRIVATE);

        //Prevent incoming threads from leaving
        pthread_spin_lock(&lock2);
    }
    
    ~Sbarrier(){
        pthread_spin_destroy(&lock1);
        pthread_spin_unlock(&lock2);
        pthread_spin_destroy(&lock2);
    }

    void wait(){
        pthread_spin_lock(&lock1);

        int code = counter.fetch_add(-1, std::memory_order_seq_cst);
        //last thread has reached barrier
        if(code == 1){
            //Allow threads in current iter to leave barrier
            //but any threads that loop around will be stopped by lock1
            pthread_spin_unlock(&lock2);
        }
        else{
            pthread_spin_unlock(&lock1);
            pthread_spin_lock(&lock2);
            pthread_spin_unlock(&lock2);
        }
        
        code = counter.fetch_add(1, std::memory_order_seq_cst);
        if(code == (baseCount - 1)){
            //All threads in current iter have left barrier,
            //Can let in any threads that have looped around by
            //unlocking lock1
            pthread_spin_lock(&lock2);
            pthread_spin_unlock(&lock1);
        }

    }

};

union BarPtr{
    Sbarrier* sbar;
    pthread_barrier_t* pbar;
};

//Maybe make this better later
class Bar{
private:
    BarPtr barPtr;
public:
    //Hopefully this is fine
    Bar() = default;
    Bar(const Bar& other) = default;
    Bar(Bar&& other) = default;
    Bar& operator=(const Bar& other) = default;
    Bar& operator=(Bar&& other) = default;

    Bar(Sbarrier* sbar, pthread_barrier_t* pbar){
        if(myBarrier){
            barPtr.sbar = sbar;
        }
        else{
            barPtr.pbar = pbar;
        }
    }

    void wait(){
        if(myBarrier){
            barPtr.sbar->wait();
        }
        else{
            pthread_barrier_wait(barPtr.pbar);
        }
    }


};

//Credit to Stanford graphics bit hacks for power of 2 rounding function here
inline size_t roundToPow2(size_t x){

    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;

    return x + 1;
}

template<typename T, typename F>
struct ThreadArgs{
    int id;
    std::vector<T>* data;
    std::vector<T>* intermediate;
    Bar* bar = nullptr;
    F operation;

    ThreadArgs(){
        this->id = 0;
        this->data = nullptr;
        this->intermediate = nullptr;
        this->bar = nullptr;
        operation = 0;
    }

    ThreadArgs(int id, std::vector<T>* data, std::vector<T>* intermediate, Bar* bar, 
               F operation) : id(id), data(data), intermediate(intermediate), bar(bar), operation(operation) {}
    //Too lazy for rule of 5
};

//Function for threads to run in the case of the requested number of threads being greater
//than the input size
void* doStuff(void* args){
    return nullptr;
}

template <typename T, typename F>
void prefixScanSeq(std::vector<T>& data, F operation, int start=0, int endEx=-1){

    if(endEx == -1){
        endEx = data.size();
    }

    for(int i = start + 1; i < endEx; ++i){
        data[i] = (*operation)(data[i], data[i - 1]);
    }
}

template <typename T, typename F>
void* prefixScanPar(void* args){
    ThreadArgs<T, F>* tArgs = (ThreadArgs<T, F>*)args;
    
    const int id = tArgs->id;
    //lmao
    std::vector<T>& data = *(tArgs->data);
    Bar* bar = tArgs->bar;
    F operation = tArgs->operation;
    

    const int n = data.size();
    int offset = 1;
    
    //Number of active threads should decrease
    for(int i = (n / 2); i > 0; i /= 2){
        
        int amtSummed = 2 * offset;
        int range = amtSummed * nThreads;

        int ind1 = offset * (2 * id + 1) - 1;
        int ind2 = offset * (2 * id + 2) - 1;
        
        bar->wait(); //hmm
        
        if(id < i){
            while(ind2 < n){
                data[ind2] = (*operation)(data[ind2], data[ind1]);
                ind1 += range;
                ind2 += range;
            }
        }
        
        offset *= 2;
    }

    for(int i = 2; i < n; i *= 2){
        offset /= 2;

        int range = offset * nThreads;
        int ind1 = offset * (id + 1) - 1;
        int ind2 = ind1 + (offset / 2);
        
        bar->wait();

        if(id < (i - 1)){
            while(ind2 < n){

                data[ind2] = (*operation)(data[ind2], data[ind1]);
                ind1 += range;
                ind2 += range;
            }
        }
    }
    
    return nullptr;
}

template<typename T, typename F>
void* twoLevel(void* args){
    ThreadArgs<T, F>* tArgs = (ThreadArgs<T, F>*)args;
    const int id = tArgs->id;
    //lmao
    std::vector<T>& data = *(tArgs->data);
    std::vector<T>& intermediate = *(tArgs->intermediate);
    Bar* bar = tArgs->bar;
    F operation = tArgs->operation;

    int toSum = data.size() / nThreads;
    int startPoint = id * toSum;
    int endPoint = std::min((size_t)(startPoint + toSum), data.size());
    
    prefixScanSeq<T,F>(data, operation, startPoint, endPoint);
    intermediate[id] = data[endPoint - 1];
    bar->wait();
    //Want to compute the prefix sums on the intermediate array in parallel
    tArgs->data = tArgs->intermediate;
    prefixScanPar<T, F>((void*)tArgs);
    bar->wait();
    
    if(id > 0){
        for(int i = startPoint; i < endPoint; ++i){
            data[i] = (*operation)(data[i], intermediate[id - 1]);
        }
    }
    
    return nullptr;
}

template <typename T, typename F>
void launch_threads(std::vector<T>& data, F operation){

    if(nThreads == 0){
        prefixScanSeq<T,F>(data, operation, 0, data.size());
        return;
    }
    
    void* (*algorithm)(void*);
    std::vector<T>* intermediate = nullptr;
    const int n = data.size();

    int totalThreads = nThreads;
    nThreads = std::min(n, nThreads);

    std::vector<pthread_t> threads(totalThreads);
    std::vector<ThreadArgs<T, F>> tArgs(nThreads);

    if(doTwoLevel){
        algorithm = twoLevel<T,F>;
        if((n % nThreads) != 0){
            size_t nSize = ((n / nThreads) + 1) * nThreads;
            data.resize(nSize);
        }

        size_t intermediateSize = roundToPow2(nThreads);
        intermediate = new std::vector<T>(intermediateSize, T());

    }
    else{
        algorithm = prefixScanPar<T,F>;

        //Check for power of 2
        if((n & (n - 1)) != 0){
            size_t nSize = roundToPow2((size_t)n);
            data.resize(nSize);;
        }
    }

   
    pthread_barrier_t pbar;
    Sbarrier* sbar;

    if(myBarrier){
        sbar = new Sbarrier(nThreads);
    }
    else{
        pthread_barrier_init(&pbar, NULL, nThreads);
    }
    
    Bar bar(sbar, &pbar);

    for(int i = 0; i < nThreads; ++i){
        tArgs[i].id = i;
        tArgs[i].data = &data;
        tArgs[i].intermediate = intermediate;
        tArgs[i].bar = &bar;
        tArgs[i].operation = operation;
    }


    for(int i = 0; i < nThreads; ++i){
        pthread_create(&threads[i], NULL, algorithm, &tArgs[i]);
    }

    for(int i = nThreads; i < totalThreads; ++i){
        pthread_create(&threads[i], NULL, doStuff, nullptr);
    }
    
    for(int i = 0; i < totalThreads; ++i){
        int errorCode = pthread_join(threads[i], NULL);
        if(errorCode != 0){
            std::cout << "Error in pthread " << i << " exit code: " << errorCode << '\n';
        }
    }

    if(myBarrier){
        delete sbar;
    }
    else{ 
        pthread_barrier_destroy(&pbar);
    }

    if(doTwoLevel){
        delete intermediate;
    }

    data.resize(n);
}

#endif //PREFIX_SCAN_H
