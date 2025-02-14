#include "stream_reassembler.hh"

#include <limits>
#include <stdexcept>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _reassembler_stream(capacity), _eof_index(std::numeric_limits<size_t>::max()) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 1. 首先计算 reassembler_stream 存储的开头和结尾 index。根据两边的 index 更新 reassembler_stream
    // 1.1 已经被 reassembled 的 index 就不必再重新 assemble 一遍
    auto _start_index = max(index, this->_reassembled_index);
    // 1.2 大于 eof 的不必 assemble；大于 capacity 的不能 assemble
    // 根据上面两条规则，计算两边的 index
    auto _index_of_remain_buffer_capacity = this->_reassembled_index + this->_capacity - this->_output.buffer_size();
    // end 结尾不计算 eof，应该是当前 reassembled_index（已经读取到的 index）+ 剩余未进入 BufferStream 缓冲区的容量
    auto _end_index = min(index + data.length(), min(_index_of_remain_buffer_capacity, this->_eof_index));

    // 2. 更新 eof
    if (eof) {
        this->_eof_index = min(this->_eof_index, index + data.length());
    }

    // 执行 1 中对 reassembler_stream 的 assemble 操作
    for (size_t i = _start_index, j = _start_index - index; i < _end_index; ++i, ++j) {
        auto &t = this->_reassembler_stream.at(i % this->_capacity);
        if (t.second) {
            // 重复进入了缓冲区，代表着重复读取了元素，直接抛弃
            if (t.first != data.at(j)) {
                throw runtime_error("StreamReassembler::push_substring: Current char has been written!");
            }
        } else {
            // 代表着进入了 reassembler 的缓冲区
            t = make_pair(data.at(j), true);
            this->_disassembled_count++;
        }
    }

    string _str;

    // 3. 接下来需要处理已经在缓冲区的新元素，reassembled_index「一路向前」，直到碰到没有 assembled 的元素为止。
    while (this->_reassembled_index < this->_eof_index && this->_reassembler_stream.at(this->_reassembled_index % this->_capacity).second) {
        _str.push_back(this->_reassembler_stream.at(this->_reassembled_index % this->_capacity).first);
        this->_reassembler_stream[this->_reassembled_index % this->_capacity] = {0, false};
        this->_reassembled_index++;
        this->_disassembled_count--;
    }

    // 4. 当我们收集了一定的 _str 后，向 ByteStream 中写入这些字符串
    this->_output.write(_str);

    // 5. 最后，如果 reassembled_index「追上了」eof_index，则表明当前当前读取完毕
    if (this->_reassembled_index == this->_eof_index) {
        this->_eof = true;
        this->_output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return this->_disassembled_count; }

bool StreamReassembler::empty() const { return this->unassembled_bytes() == 0; }

size_t StreamReassembler::reassembled_index() const { return this->_reassembled_index; }
