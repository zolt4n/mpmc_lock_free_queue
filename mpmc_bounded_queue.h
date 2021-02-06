#pragma once

/*
So the idea created a lock free bounded queue for multiple consumer and multiple producer.

we need to index reader and writer positions.
 - if reader == writer ==> queue is empty
 - if reader == writer + 1 ==> queue is full

each cell will be given an index. this is how we know something is available for writing/reading.

 R:0
|1|2|3|3|4|5|6|7|
       W:3

When writing we check the cell index is equal to writer pos, if so it can increase is idx by 1 and write.
When we Read we check the cell is equal to reader pos + 1, is so we return the object and increment it by SIZE.

           R:5
|8|9|10|11|5|6|6|7|
               W : 6

This way we can know in one compare and swap if queue is empty/full or is another consumer/producer tried to take the cell before us.


 Full scenario :
            R : 5
|9|10|11|12|5|6|7|8|
            W : 13

 Empty scenario :
             R : 13
|17|18|19|20|13|14|15|16|
             W : 13
*/

#include <atomic>

namespace zoltan
{
    template<size_t SIZE, class T>
	class mcmp_bounded_queue
	{
	public:
	    struct cell
        {
	        T _data;
	        std::atomic_size_t _index;
        };

        mcmp_bounded_queue()
        {
            // make sure it's power of 2 so we can use SIZE_MASK as modulo.
            static_assert(SIZE >= 2 and ((SIZE & (SIZE - 1)) == 0));
            m_queue = new cell[SIZE];

            for (auto idx = 0; idx < SIZE; ++idx)
            {
                m_queue[idx]._index.store(idx, std::memory_order_relaxed);
            }
            m_read.store(0, std::memory_order_relaxed);
            m_write.store(0, std::memory_order_relaxed);
        }

        ~mcmp_bounded_queue()
        {
            delete [] m_queue;
        }

        mcmp_bounded_queue(const mcmp_bounded_queue&) = delete;
        mcmp_bounded_queue(const mcmp_bounded_queue&&) = delete;
        mcmp_bounded_queue& operator=(const mcmp_bounded_queue&) = delete;
        mcmp_bounded_queue& operator=(const mcmp_bounded_queue&&) = delete;
        
        bool enqueue(T& data)
        {
            cell* c = nullptr;
            size_t write_pos = m_write.load(std::memory_order_relaxed);

            while (true)
            {
                c = &m_queue[write_pos & SIZE_MASK];
                size_t cell_pos = c->_index.load(std::memory_order_acquire);
                long diff = cell_pos - write_pos;
                if (diff == 0)
                {
                    // ok cell is available to write
                    // try to "lock" that cell by increasing the writer pos.
                    if (m_write.compare_exchange_weak(write_pos, write_pos + 1, std::memory_order_relaxed))
                    {
                        // succeed we can write here.
                        break;
                    }
                    write_pos = m_write.load(std::memory_order_relaxed);
                }
                else if (diff < 0)
                {
                    // queue is full
                    return false;
                }
            }
            c->_data = data;
            c->_index.store(write_pos + 1, std::memory_order_release);
            return true;
        }
        
        bool dequeue(T& data)
        {
            cell* c = nullptr;
            size_t read_pos = m_read.load(std::memory_order_relaxed);

            while (true)
            {
                c = &m_queue[read_pos & SIZE_MASK];
                size_t cell_pos = c->_index.load(std::memory_order_acquire);
                long diff = cell_pos - read_pos;
                if (diff == 1)
                {
                    // ok there is something to read, try to lock that cell for reading.
                    if (m_read.compare_exchange_weak(read_pos, read_pos + 1, std::memory_order_relaxed))
                    {
                        // ok we can read from it.
                        break;
                    }
                }
                else if (diff == 0)
                {
                    // queue is empty.
                    return false;
                }
                read_pos = m_read.load(std::memory_order_relaxed);
            }
            data = c->_data;
            c->_index.store(read_pos + SIZE, std::memory_order_release);
            return true;
        }
        
	private:
	    using cache_line = char[64];
        constexpr static size_t SIZE_MASK = SIZE - 1;
        cache_line _pad0;
        cell* m_queue;
        cache_line _pad1;
        std::atomic_size_t m_read;
        cache_line _pad2;
        std::atomic_size_t m_write;
        cache_line _pad3;
	};
}
