#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <string.h>
#include <dirent.h>
#include <iostream>
#include <unistd.h>

class directory
{
public:
    directory();
    std::string currentdir();
};

#endif // DIRECTORY_H
