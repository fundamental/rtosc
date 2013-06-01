/*
 * Copyright (c) 2012 Mark McCurry
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef RTOSC_THREAD_LINK
#define RTOSC_THREAD_LINK

#include <jack/ringbuffer.h>

#include <cstring>
#include <cassert>
#include <cstdio>
#include <rtosc/rtosc.h>

namespace rtosc {
typedef const char *msg_t;

/**
 * ThreadLink - A simple wrapper around jack's ringbuffers desinged to make
 * sending messages via rt-osc trivial.
 * This class provides the basics of reading and writing events via fixed sized
 * buffers, which can be specified at compile time.
 */
class ThreadLink
{
    public:
        ThreadLink(size_t max_message_length, size_t max_messages)
            :MaxMsg(max_message_length),
            BufferSize(MaxMsg*max_messages),
            write_buffer(new char[BufferSize]),
            read_buffer(new char[BufferSize])
        {
            ring = jack_ringbuffer_create(BufferSize);
            jack_ringbuffer_mlock(ring);
            memset(write_buffer, 0, BufferSize);
            memset(read_buffer, 0, BufferSize);
        }

        ~ThreadLink(void)
        {
            jack_ringbuffer_free(ring);
            delete[] write_buffer;
            delete[] read_buffer;
        }

        /**
         * Write message to ringbuffer
         * @see rtosc_message()
         */
        void write(const char *dest, const char *args, ...)
        {
            va_list va;
            va_start(va,args);
            const size_t len =
                rtosc_vmessage(write_buffer,MaxMsg,dest,args,va);
            if(jack_ringbuffer_write_space(ring) >= len)
                jack_ringbuffer_write(ring,write_buffer,len);
        }

        /**
         * Write an arary of arguments to ringbuffer
         * @see rtosc_amessage()
         */
        void writeArray(const char *dest, const char *args, const rtosc_arg_t *aargs)
        {
            const size_t len =
                rtosc_amessage(write_buffer, MaxMsg, dest, args, aargs);
            if(jack_ringbuffer_write_space(ring) >= len)
                jack_ringbuffer_write(ring,write_buffer,len);
        }

        /**
         * Directly write message to ringbuffer
         */
        void raw_write(const char *msg)
        {
            const size_t len = rtosc_message_length(msg, -1);//assumed valid
            if(jack_ringbuffer_write_space(ring) >= len)
                jack_ringbuffer_write(ring,msg,len);
        }

        /**
         * @returns true iff there is another message to be read in the buffer
         */
        bool hasNext(void) const
        {
            return jack_ringbuffer_read_space(ring);
        }

        /**
         * Read a new message from the ringbuffer
         */
        msg_t read(void) {
            ring_t r[2];
            jack_ringbuffer_get_read_vector(ring,(jack_ringbuffer_data_t*)r);
            const size_t len =
                rtosc_message_ring_length(r);
            assert(jack_ringbuffer_read_space(ring) >= len);
            assert(len <= MaxMsg);
            jack_ringbuffer_read(ring, read_buffer, len);
            return read_buffer;
        }

        /**
         * Peak at last message read without reading another
         */
        msg_t peak(void) const
        {
            return read_buffer;
        }

        /**
         * Raw write buffer access for more complicated task
         */
        char *buffer(void) {return write_buffer;}
        /**
         * Access to write buffer length
         */
        size_t buffer_size(void) const {return BufferSize;}
    private:
        const size_t MaxMsg;
        const size_t BufferSize;
        char *write_buffer;
        char *read_buffer;

        //assumes jack ringbuffer
        jack_ringbuffer_t *ring;
};
};
#endif
