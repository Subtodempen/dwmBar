#pragma once

#include <xcb/xcb.h>
#include <iostream>
#include <string>

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
