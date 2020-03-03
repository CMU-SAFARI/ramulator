#ifndef __DEBUG_H
#define __DEBUG_H

//turn DEBUG on or off at compile time
#define DEBUG

#ifdef DEBUG
    #define DEBUG_MSG(flag, str) do { if(configs["debugflags"].find(flag) != std::string::npos){std::cout << flag <<": " << str << std::endl;} } while( false )
#else
    #define DEBUG_MSG(flag, str) do {} while(false)
#endif   

#endif /* __DEBUG_H */