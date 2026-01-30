

struct Date { std::string }
struct CpuFreq {std::string }

using Block = std::varient<Date, CpuFreq>;
using Bar = std::vector<Block>;


Bar.push_back{Date{"19/78/09"}};

// The program will iterate through the vector see which type each one is and display it in order !!!!!   
