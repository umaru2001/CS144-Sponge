#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _count_written(0), _count_read(0),
    _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    auto _safe_len = min(data.size(), this->remaining_capacity());
    for (size_t i = 0; i < _safe_len; ++i) {
        this->_buffer_deque.push_back(data.at(i));
        this->_count_written++;
    }
    return _safe_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    auto _safe_len = min(len, this->_buffer_deque.size());
    string _str;
    for (size_t i = 0; i < _safe_len; ++i) {
        _str += this->_buffer_deque.at(i);
    }
    return _str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto _safe_len = min(len, this->_buffer_deque.size());
    for (size_t i = 0; i < _safe_len; ++i) {
        this->_buffer_deque.pop_front();
        this->_count_read++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
    string _str = peek_output(len);
    this->pop_output(len);
    return _str;
}

void ByteStream::end_input() {
    this->_input_finished_flag = true;
}

bool ByteStream::input_ended() const { return this->_input_finished_flag; }

size_t ByteStream::buffer_size() const { return this->_count_written - this->_count_read; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return this->_input_finished_flag && this->buffer_empty(); }

size_t ByteStream::bytes_written() const { return this->_count_written; }

size_t ByteStream::bytes_read() const { return this->_count_read; }

size_t ByteStream::remaining_capacity() const { return this->_capacity - this->buffer_size(); }