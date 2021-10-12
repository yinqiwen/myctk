/*
*Copyright (c) 2020, qiyingwang <qiyingwang@tencent.com>
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
*  * Neither the name of rimos nor the names of its contributors may be used
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

#pragma once
#include "common_types.hpp"
#include <msgpack.hpp>

namespace msgpack
{
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
    namespace adaptor
    {
    template <>
    struct convert<cmod::SHMString>
    {
        msgpack::object const &operator()(msgpack::object const &o, cmod::SHMString &v) const
        {
            switch (o.type)
            {
            case msgpack::type::BIN:
                v.assign(o.via.bin.ptr, o.via.bin.size);
                break;
            case msgpack::type::STR:
                v.assign(o.via.str.ptr, o.via.str.size);
                break;
            default:
                throw msgpack::type_error();
                break;
            }
            if (o.type != msgpack::type::STR)
            {
                throw msgpack::type_error();
            }
            if (o.via.str.size != 0)
            {
                v.assign(o.via.str.ptr, o.via.str.size);
            }
            return o;
        }
    };
    template <>
    struct pack<cmod::SHMString>
    {
        template <typename Stream>
        msgpack::packer<Stream> &operator()(msgpack::packer<Stream> &o, const cmod::SHMString &v) const
        {
            uint32_t size = msgpack::checked_get_container_size(v.size());
            o.pack_str(size);
            o.pack_str_body(v.data(), v.size());
            return o;
        }
    };

    template <typename K, typename V>
    struct convert<boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, cmod::Allocator<std::pair<const K, V>>>>
    {
        typedef boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, cmod::Allocator<std::pair<const K, V>>> LocalMapType;
        msgpack::object const &operator()(msgpack::object const &o, boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, cmod::Allocator<std::pair<const K, V>>> &v) const
        {
            if (o.type != msgpack::type::MAP)
            {
                throw msgpack::type_error();
            }
            if (o.via.map.size != 0)
            {
                v.reserve(o.via.map.size);
                msgpack::object_kv *p = o.via.map.ptr;
                msgpack::object_kv *const pend = o.via.map.ptr + o.via.map.size;
                for (; p < pend; ++p)
                {
                    cmod::SHMConstructor<K> key(v.get_allocator());
                    p->key.convert(key.value);
                    auto found = v.find(key.value);
                    if (found != v.end())
                    {
                        p->val.convert(found->second);
                    }
                    else
                    {
                        cmod::SHMConstructor<V> value(v.get_allocator());
                        p->val.convert(value.value);
                        v.insert(typename LocalMapType::value_type(key.value, value.value));
                    }
                }
            }
            return o;
        }
    };
    template <typename K, typename V>
    struct pack<boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, cmod::Allocator<std::pair<const K, V>>>>
    {
        typedef boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, cmod::Allocator<std::pair<const K, V>>> LocalMapType;
        template <typename Stream>
        msgpack::packer<Stream> &operator()(msgpack::packer<Stream> &o, const boost::unordered_map<K, V, boost::hash<K>, std::equal_to<K>, cmod::Allocator<std::pair<const K, V>>> &v) const
        {
            uint32_t size = msgpack::checked_get_container_size(v.size());
            o.pack_map(size);
            typename LocalMapType::const_iterator it = v.begin();
            while (it != v.end())
            {
                o.pack(it->first);
                o.pack(it->second);
                it++;
            }
            return o;
        }
    };

    } // namespace adaptor
} // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack