#ifndef FILE_OBJECT_H
#define FILE_OBJECT_H

#include <string>
#include <string.h>
#include <iostream>
#include <map>
#include <vector>


using namespace std;

class File_object {
    public:
        string sdfs_filename;
        int hash_key;
        int timeStamp;

        File_object(string sdfs_name);
        File_object(string sdfs_name, int hash_key, int timeStamp);

};

#endif