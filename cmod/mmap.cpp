/*
 *Copyright (c) 2017-2018, yinqiwen <yinqiwen@gmail.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Redis nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mmap.hpp"
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

namespace cmod
{
int MMapBuf::Init(const std::string &path, uint64_t size, bool readonly, bool reate_if_notexist)
{
    int mode = O_RDONLY, permission = S_IRUSR;
    int mmap_mode = PROT_READ;
    if (!readonly)
    {
        mode = O_RDWR;
        permission |= S_IWUSR;
        mmap_mode |= PROT_WRITE;
        if (reate_if_notexist)
        {
            mode |= O_CREAT;
        }
    }
    int fd = open(path.c_str(), mode, permission);
    if (fd < 0)
    {
        const char *err = strerror(errno);
        err_msg = err;
        return -1;
    }

    struct stat file_st;
    memset(&file_st, 0, sizeof(file_st));
    fstat(fd, &file_st);

    bool create_file = false;
    if (readonly)
    {
        //ignore passed size para
        if (0 == file_st.st_size)
        {
            char tmp[1024];
            snprintf(tmp, sizeof(tmp) - 1, "Failed to open read empty mmap file:%s", path.c_str());
            err_msg = tmp;
            close(fd);
            return -1;
        }
        size = file_st.st_size;
    }
    else
    {
        //            if (file_st.st_size != 0 && (size_t) file_st.st_size < size)
        //            {
        //                char tmp[1024];
        //                snprintf(tmp, sizeof(tmp) - 1, "Failed to open write  mmap file:%s since passed size:%llu while file size:%llu", path.c_str(), size, file_st.st_size);
        //                err_msg = tmp;
        //                close(fd);
        //                return -1;
        //            }
        if ((uint64_t)file_st.st_size != size)
        {
            if (-1 == ftruncate(fd, size))
            {
                const char *err = strerror(errno);
                err_msg = err;
                close(fd);
                return -1;
            }
            create_file = file_st.st_size == 0;
        }
        else
        {
            size = file_st.st_size;
        }
    }

    char *mbuf = (char *)mmap(NULL, size, mmap_mode, MAP_SHARED, fd, 0);
    close(fd);
    if (mbuf == MAP_FAILED)
    {
        const char *err = strerror(errno);
        err_msg = err;
        return -1;
    }
    else
    {
        this->buf = mbuf;
        this->size = size;
    }
    return create_file ? 1 : 0;
}

int MMapBuf::OpenWrite(const std::string &path, int64_t size, bool create_if_notexist)
{
    return Init(path, size, false, create_if_notexist);
}

int MMapBuf::OpenRead(const std::string &path)
{
    return Init(path, 0, true, false);
}

int MMapBuf::Close()
{
    munmap(buf, size);
    return 0;
}

MMapBuf::~MMapBuf()
{
    if (atuoclose)
    {
        Close();
    }
}

} // namespace cmod
