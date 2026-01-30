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


namespace getterFuns{
  std::string dateTime(){
	time_t timestamp;
	time(&timestamp);
	
	return std::string(ctime(&timestamp));
  }
};

inline std::string formatBar(const std::vector<Block>& bar){
   std::string barString = "";

   for(const auto block : bar){
     barString += std::get<0>(block).stat + "|"; 
   }

   return barString;
}


int main(){
  // open a X Display & Window
  xcbWrapper x;
  ThreadSafeQueue<Block> q;

  Bar bar = {Date{} };
  std::string prevInsert = "";

  auto dateTimeThread = [&]() {
    while (true) {
      auto dateBlock = Date{getterFuns::dateTime() };
      q.push(dateBlock);
          
      // sleep for 5 seconds
      std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
  };
  std::thread t1(dateTimeThread);

  while(true){
    auto blockUpdate = q.pop();

    // find the curr object and then replace with blockUpdate using std::visit
    for (auto &b : bar) {
      std::visit([&](auto &&arg) {
        using currType = std::decay_t<decltype(arg)>;
        using updateType = std::decay_t<decltype(blockUpdate)>;

        std::cout << typeid(Block).name() << std::endl;
	std::cout << typeid(updateType).name() << std::endl;
	
        if constexpr (std::is_same_v<Block, updateType>)
          b = blockUpdate;
      }, b);
    }

    auto barString = formatBar(bar);
    x.writeToBar(barString);
  }

  t1.join();
}
