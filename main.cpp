#include <iostream>
#include <functional>
#include <queue>

#include <mutex>
#include <condition_variable>
#include <thread>

#include <ctime>
#include <sys/mman.h>

#include <cerrno>
#include <cstring>

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
  auto mapFile(const std::string &fName, size_t &length) noexcept  {
    // open file
    auto fd = open(fName.c_str(), O_RDONLY);
    if (fd == -1)
      std::cout << ("Can not open file");

    // get size
    struct stat sb;
    if (fstat(fd, &sb) == -1)
      std::cout << ("Can not get file size");
    
    length = sb.st_size;

    // call Mmap
    char *addr = static_cast<char *>(
        mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0));
    if (addr == MAP_FAILED)
      std::cout << ("MMAP failed");
     
    close(fd);
    return addr;
  }

  auto unmapFile(char* addr, const size_t length) noexcept {
    if (munmap(addr, length) == -1) {
    }
  }

  std::string readVirtualFile(const std::string &fName) {
    std::ifstream file(fName);
    return std::string(std::istreambuf_iterator<char>(file),
           std::istreambuf_iterator<char>());
  }

  
  std::string readFile(const std::string &fName) {
    size_t length;
    auto data = mapFile(fName, length);

    std::string file(data, length);

    unmapFile(data, length);
    return file;
  }
}


namespace getterFuns{
  std::string dateTime(){
	time_t timestamp;
	time(&timestamp);
	
	return std::string(ctime(&timestamp));
  }
  
  std::string cpuFreq() {
    // read in /proc/cpuinfo
    auto cpu = readVirtualFile("/proc/cpuinfo");

    // output the find cpu MHz 
    auto location = cpu.find("cpu MHz");
    std::string line = cpu.substr(location, cpu.find("\n"));
    
    return line;
  }

  std::string battery() {
    auto batteryPower =
      readVirtualFile("/sys/class/power_supply/BAT0/capacity");
    return batteryPower;
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

  Bar bar = {Battery{}, CpuFreq{}, Date{}};
  std::string barString = "";
 
  auto t1 = createThread<Date>(getterFuns::dateTime, q, REFRESHRATE);
  auto t2 = createThread<CpuFreq>(getterFuns::cpuFreq, q, REFRESHRATE);
  auto t3 = createThread<Battery>(getterFuns::battery, q, 30000);

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
