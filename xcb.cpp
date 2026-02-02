#include "xcb.h"

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
