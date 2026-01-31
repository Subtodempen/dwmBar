#include <iostream>
#include <functional>
#include <queue>

#include <mutex>
#include <condition_variable>
#include <thread>

#include <xcb/xcb.h>
#include <ctime>

#include <chrono>
#include <thread>

#include <iostream>
#include <fstream>
#include <sstream>


#include "config.h"

class xcbWrapper{
public:
  xcbWrapper(); 
  void writeToBar(const std::string&);

private:
  void startConn();
  void findRootWin();

  void deconstructWindow();
  
  xcb_connection_t *t;
  xcb_window_t *w;
};

xcbWrapper::xcbWrapper(){
  startConn();
  findRootWin();
}

void xcbWrapper::startConn(){
  int discard;
  t = xcb_connect(NULL, &discard);
}

void xcbWrapper::findRootWin(){    
  auto *iter = xcb_setup_roots_iterator(xcb_get_setup(t)).data;

  if(!iter)
    throw std::runtime_error("Can not get root Window");
  
  w = &iter->root;
}

void xcbWrapper::writeToBar(const std::string& stat){
  xcb_change_property(t,
		       XCB_PROP_MODE_REPLACE,
		       *w,
		       XCB_ATOM_WM_NAME,
		       XCB_ATOM_STRING,
		       8,
		       stat.length(),
		       stat.c_str());

  xcb_flush(t);
}

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
std::string getCpuInfo() {
  std::ifstream procCpu("/proc/cpuinfo");
  std::stringstream buffer;

  buffer << procCpu.rdbuf();
  procCpu.close();
  return buffer.str();
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
    auto cpu = getCpuInfo();

    // output the find cpu MHz 
    auto location = cpu.find("cpu MHz");
    std::string line = cpu.substr(location, cpu.find("\n"));
    
    return line;
  }
};


template <class bType>
auto createThread(std::function<std::string()> func, ThreadSafeQueue<Block>& q) {
  return
    std::thread([func, &q]() {
      while (true) {
	auto block = bType{ func() };
	q.push(block);
          
	// sleep for 5 seconds
	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
      }
    });
}

int main(){
  // open a X Display & Window
  xcbWrapper x;
  ThreadSafeQueue<Block> q;

  Bar bar = {Date{}, CpuFreq{} };
  std::string barString = "";

  auto t1 = createThread<Date>(getterFuns::dateTime, q);
  auto t2 = createThread<CpuFreq>(getterFuns::cpuFreq, q);
  
  while(true){
    auto blockUpdate = q.pop();

    // Checks for objects of the same type in the bar and updates them
    for (auto& b : bar) {
      using blockType = std::decay_t<decltype(blockUpdate)>; 
      using currType = std::decay_t<decltype(b)>;

      if constexpr (std::is_same_v<currType, blockType>)
	b = blockUpdate;

      /*
      std::visit(
 [&](auto &&arg, auto &&localBU) {
            using currType = std::decay_t<decltype(arg)>;
	    using updateType = std::decay_t<decltype(localBU)>;

	    if constexpr (std::is_same_v<currType, updateType>)
               barString += "|" + localBU.stat;
         }, b, blockUpdate);
      */
    }

    x.writeToBar(barString);
  }
}
