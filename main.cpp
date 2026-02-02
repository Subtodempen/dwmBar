#include <iostream>
#include <functional>
#include <queue>

#include <mutex>
#include <condition_variable>
#include <thread>

#include <ctime>

#include <fstream>
#include <streambuf>

#include <signal.h>

#include <chrono>
#include <thread>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "xcb.h"
#include "config.h"



/*
class Observer{
public:
  Observer() = default;
  Observer(genericGetterFun gf): _f(gf) {}

  virtual std::string update(){
	return _f();
  }
  void setGetterFunction(const genericGetterFun gf){
	_f = gf;
  }

  genericGetterFun _f;
};
*/

template<class T>
class ThreadSafeQueue{
public:
  void push(const T& item);
  T pop();

private:
  std::queue<T> _q;
  std::mutex _m;
  std::condition_variable _cv;
};


template <class T>
void ThreadSafeQueue<T>::push(const T& item){
  {
    std::unique_lock<std::mutex> lock(_m);
    _q.push(item);
  }

   _cv.notify_one();
}

template <class T>
T ThreadSafeQueue<T>::pop(){
   std::unique_lock<std::mutex> lock(_m);
   _cv.wait(lock, [this] {return !_q.empty(); });

   auto item = std::move(_q.front());
   _q.pop();
   
   return item;
}


// anymous namespace for localilty
namespace {
  std::string readVirtualFile(const std::string &fName) {
    std::ifstream file(fName);
    return std::string(std::istreambuf_iterator<char>(file),
           std::istreambuf_iterator<char>());
  }

  std::string parseProcFS(const std::string &proc,
                          const std::string &lineStart,
                          const std::string &delim
			  ) {
    auto location = proc.find(lineStart);
    size_t end = proc.find(delim, location);

    return proc.substr(location, end - location);
}
  /*
  std::string readFile(const std::string &fName) {
    size_t length;
    auto data = mapFile(fName, length);

    std::string file(data, length);

    unmapFile(data, length);
    return file;
  }
  */
}


namespace getterFuns{
  std::string dateTime(){
	time_t timestamp = std::time(nullptr);
   	return std::asctime(std::localtime(&timestamp));
  }
  
  std::string cpuFreq() {
    // read in /proc/cpuinfo
    auto cpu = readVirtualFile("/proc/cpuinfo");    
    return parseProcFS(cpu, "cpu MHz", "\n");
  }

  std::string battery() {
    auto batteryPower =
      readVirtualFile("/sys/class/power_supply/BAT0/capacity");
    return batteryPower;
  }

  std::string memory() {
    auto memInfo = readVirtualFile("/proc/meminfo");

    const std::string memAID = "MemAvailable:";
    const std::string memTID = "MemTotal:";
    
    std::string memAvailable = parseProcFS(memInfo, memAID, "kB");
    std::string memTotal = parseProcFS(memInfo, memTID, "kB");

    memAvailable = memAvailable.substr(memAID.length());
    memTotal = memTotal.substr(memTID.length());

    auto totalKB = stol(memTotal);
    auto availKB = stoi(memAvailable);

    auto usedMB  = (totalKB - availKB) / 1024;
    auto totalMB = totalKB / 1024;
    
    return std::to_string(usedMB) + "/" + std::to_string(totalMB);
  }
};


std::string formatBar(const std::vector<Block>& bar){
    std::string barString = "";
    
    for(auto block : bar){
      barString += "|" + 
	std::visit([](auto&& arg) -> std::string {
	  return arg.stat; 
	}, block);
    }
    
    return barString;
}

template <class bType>
auto createThread(std::function<std::string()> func, ThreadSafeQueue<Block>& q, const uint sleepTime) {
  return
    std::jthread([func, sleepTime, &q](std::stop_token stoken) {
      while (!stoken.stop_requested()) {
	auto block = bType{ func() };
	q.push(block);
      
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
      }
    });
}


volatile sig_atomic_t stop;

int main(){
  // open a X Display & Window
  xcbWrapper x;
  ThreadSafeQueue<Block> q;

  Bar bar = {Memory{}, CpuFreq{}, Date{}};
  std::string barString = "";
 
  auto t1 = createThread<Date>(getterFuns::dateTime, q, REFRESHRATE);
  auto t2 = createThread<CpuFreq>(getterFuns::cpuFreq, q, 250);
  auto t3 = createThread<Memory>(getterFuns::memory, q, 500);

  //auto t3 = createThread<Battery>(getterFuns::battery, q, 30000);

  stop = false;
  
  auto signalHandle = [](int){ stop = true; };
  signal(SIGINT, signalHandle);
  signal(SIGTERM, signalHandle);
  
  while(!stop){
    auto blockUpdate = q.pop();

    // Checks for objects of the same type in the bar and updates them
    for (auto& b : bar) {
      std::visit(
 [&](auto &&arg, auto &&localBU) {
            using currType = std::decay_t<decltype(arg)>;
	    using updateType = std::decay_t<decltype(localBU)>;

	    if constexpr (std::is_same_v<currType, updateType>)
               b = blockUpdate;
         }, b, blockUpdate);
    }
    
    barString = formatBar(bar);
    x.writeToBar(barString);
  }

}
