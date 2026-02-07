#include <vector>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <thread>

using namespace std;

class ThreadPool {
    private:
        queue<function<void()>> funcs;
        vector<thread> workers;
        int num_threads;
        int num_stocks;
        mutex mtx;
        condition_variable cv;
        bool stop;
        void create_things(){
            while(true){
                function<void()> task;
                {
                    unique_lock<mutex> lock(mtx);
                    cv.wait(lock, [this]{
                        return stop||!tasks.empty();
                    });
                    if (stop && tasks.empty()){
                    return;
                    }
                    task = move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        }
    public:
        ThreadPool(int num_threads, int num_stocks): num_threads(num_threads), num_stocks(num_stocks), stop(false){
            for (int i = 0; i < num_threads; i++) {
                workers.emplace_back([this] { worker(); });
            }
        }
        void enqueue(function<void()> task) {
            {
                lock_guard<mutex> lock(mtx);
                tasks.push(move(task));
            }
            cv.notify_one();
        }
        ~ThreadPool() {
            {
                lock_guard<mutex> lock(mtx);
                stop = true;
            }
            cv.notify_all();
            for (auto& t : workers) {
                if (t.joinable())
                    t.join();
            }
        }
};