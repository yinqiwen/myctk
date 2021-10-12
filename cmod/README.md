Common Memory Object Dataset
======

A C++ library to store&read complex data sturctures in files.  

## Dependency

[Boost Interprocess](http://www.boost.org/doc/libs/1_63_0/doc/html/interprocess.html)

## Features
- Designed for apps wanting to store many complex data sturctures on locally in shared memory or disk files.
- Very fast, which have similar read/write performance compared to same data structure in memory.

## Example

```cpp
#include <stdio.h>
#include "cmod.hpp"

using namespace cmod;

struct ValueObject
{
        SHMString v0;
        int64_t v1;
        double v2;
        SHMVector<int>::Type v3;
        ValueObject(const CharAllocator& alloc = CharAllocator() )
                : v0(alloc), v1(0), v2(1.0), v3(alloc)
        {
        }
};
typedef SHMHashMap<SHMString, ValueObject>::Type RootTable;

static int64_t image_maxsize = 100 * 1024 * 1024;
static std::string image_file = "./test.img";
static void build_data_image()
{
    CMODFile mfile(image_file, image_maxsize);
    RootTable* table = mfile.LoadRootWriteObject<RootTable>();
    CharAllocator& alloc = mfile.GetAllocator();
    for (size_t i = 0; i < 12345; i++)
    {
        SHMString key;
        char tmp[100];
        sprintf(tmp, "key%d", i);
        key.assign(tmp);
        ValueObject v(alloc);
        sprintf(tmp, "value%d", i);
        v.v0.assign(tmp);
        v.v1 = i + 100;
        v.v2 = i * 1000;
        v.v3.push_back(i);
        v.v3.push_back(i + 1);
        v.v3.push_back(i + 2);
        table->insert(RootTable::value_type(key, v));
    }
    int64_t n = mfile.ShrinkWriteFile();
    printf("Build data image file with size:%lld\n", n);
}

static void load_data_image()
{
    CMODFile mfile(image_file);
    const RootTable* read_table = mfile.LoadRootReadObject<RootTable>();
    printf("Total %llu entry in data image.\n", read_table->size());
    RootTable::const_iterator found = read_table->find("key100");
    if (found != read_table->end())
    {
        const SHMString& key = found->first;
        const ValueObject& value = found->second;
        printf("FoundEntry: key:%s v0:%s v1:%lld  v3:[%d %d %d]\n", key.c_str(), value.v0.c_str(), value.v1, value.v3[0], value.v3[1],
                value.v3[2]);
    }
}

int main()
{
    build_data_image();  // build a local disk file which store data
    
    load_data_image();   // load data into memory 
    return 0;
}
```

## Embedding cmod

Just copy all header&source files into your project. 
