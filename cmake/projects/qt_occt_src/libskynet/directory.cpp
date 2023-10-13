#include "directory.h"

directory::directory()
{

}

std::string directory::currentdir(){
    char* cwd = getcwd( 0, 0 ) ; // **** microsoft specific ****
    std::string working_directory(cwd) ;
    std::free(cwd) ;
    return working_directory ;
}
