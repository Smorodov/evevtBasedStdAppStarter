#include "eventpp/eventqueue.h"
#include "eventpp/utilities/orderedqueuelist.h"
#include "main.h"
#include <iostream>
#include <thread>
#include <functional>
#include <memory>
#include <mutex>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h" // support for loading levels from the environment variable

// to make order wuth console output
std::mutex coutMutex;
// event map
constexpr int stopEvent = 1;
constexpr int otherEvent = 2;

// First let's define the event struct. e is the event type, priority determines the priority.
struct MyEvent
{
    int e;
    int priority;
};

// The comparison function object used by eventpp::OrderedQueueList.
// The function compares the event by priority.
struct MyCompare
{
    template <typename T>
    bool operator() (const T& a, const T& b) const
    {
        return a.template getArgument<0>().priority > b.template getArgument<0>().priority;
    }
};

// Define the EventQueue policy
struct MyPolicy
{
    template <typename Item>
    using QueueList = eventpp::OrderedQueueList<Item, MyCompare >;

    static int getEvent(const MyEvent& event)
    {
        return event.e;
    }
};
using EQ = eventpp::EventQueue<int, void(const MyEvent&), MyPolicy>;


class CallbackClass
{
public:
    CallbackClass()
    {
        setValue(0);
    }
    CallbackClass(int value_)
    {
        setValue(value_);
    }
    void callback(int new_value)
    {
        coutMutex.lock();
        spdlog::info(u8"Callback: value = {0}  new_value={1}", value, new_value);
        coutMutex.unlock();
        setValue(new_value);
    }

    void setValue(int v)
    {
        value = v;
    }

    int getValue(int v)
    {
        return value;
    }
private:
    int value;
};

class WorkerClass
{
public:
    std::function<void(int)> callback;
    bool shouldStop;
    std::thread* thread;
    WorkerClass(EQ& queue) : queue_(queue)
    {
        shouldStop = false;
        thread = nullptr;
        coutMutex.lock();
        spdlog::info(u8"Constructor.");        
        coutMutex.unlock();
    }
    void Run()
    {
        coutMutex.lock();
        spdlog::info(u8"Run.");        
        coutMutex.unlock();
        thread = new std::thread(&WorkerClass::worker, this);
    }
    void Stop()
    {
        coutMutex.lock();
        spdlog::info(u8"Stop.");        
        coutMutex.unlock();
        if (thread->joinable())
        {
            coutMutex.lock();
            spdlog::info(u8"Joining.");            
            coutMutex.unlock();
            shouldStop = true;
            thread->join();
        }
        else
        {
            coutMutex.lock();
            spdlog::warn(u8"Already joined.");            
            coutMutex.unlock();
        }
    }
    ~WorkerClass()
    {
        coutMutex.lock();
        spdlog::info(u8"Destructor.");        
        coutMutex.unlock();
        Stop();
        delete thread;
    }

    void worker(void)
    {

        while (!shouldStop)
        {
            coutMutex.lock();
            spdlog::info(u8"Worker: enqueue otherEvent(11).");
            spdlog::info(u8"Worker: enqueue stopEvent(12).");
            spdlog::info(u8"Worker: enqueue otherEvent(13).");
            coutMutex.unlock();
            // priority 3 (highest)
            queue_.enqueue(otherEvent, MyEvent{ 11, 3 });
            // priority 1 (lowest)
            queue_.enqueue(stopEvent, MyEvent{ 12, 1 });
            // priority 2
            queue_.enqueue(otherEvent, MyEvent{ 13, 2 });

            std::this_thread::sleep_for(std::chrono::seconds(1));
            coutMutex.lock();
            spdlog::info(u8"Worker: calling callback.");
            coutMutex.unlock();
            callback(33);

        }
    }
private:
    EQ& queue_;
};

void f1(const MyEvent& event)
{
    coutMutex.lock();
    spdlog::info(u8"Stop event processed with argument = {0}.", event.e);    
    coutMutex.unlock();
}

void f2(const MyEvent& event)
{
    coutMutex.lock();
    spdlog::info(u8"Other event processed with argument = {0}.", event.e);    
    coutMutex.unlock();
}

// for russian console output in spdlog
#ifdef _WIN32
#include <windows.h>
#endif
#include <locale>
void setRussianConsole()
{
    // for russian console output in spdlog
    setlocale(LC_ALL, "ru_RU.UTF8");
    setlocale(LC_NUMERIC, "C");
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void main()
{
    setRussianConsole();
    spdlog::set_level(spdlog::level::info);
    spdlog::info(u8"Старт.");

    EQ queue;
    CallbackClass* callbackClass=new CallbackClass(50);
    WorkerClass* w1=new WorkerClass(queue);

    w1->callback = std::bind(&CallbackClass::callback, callbackClass, std::placeholders::_1);

    queue.appendListener(stopEvent, f1);
    queue.appendListener(otherEvent, f2);

    w1->Run();
    for (int i = 0; i < 5; ++i)
    {
        queue.wait();
        queue.process();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    spdlog::info(u8"Стоп.");
    w1->Stop();    
    delete w1;
    delete callbackClass;
    spdlog::shutdown();
}