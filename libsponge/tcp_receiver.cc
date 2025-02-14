#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!this->_isn.has_value()) {
        if (seg.header().syn) {
            this->_isn = seg.header().seqno;
            this->_reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
            this->_update_ackno();
        }
        return;
    }

    auto absolute_seqno = unwrap(seg.header().seqno, this->_isn.value(), this->unassembled_bytes());

    // absolute_seqno 已经包含了一个 syn，因此需要 -1 才能得到正确的起始索引
    this->_reassembler.push_substring(seg.payload().copy(), absolute_seqno - 1, seg.header().fin);
    this->_update_ackno();
}

optional<WrappingInt32> TCPReceiver::ackno() const { return this->_ackno; }

size_t TCPReceiver::window_size() const { return this->_capacity - this->_reassembler.stream_out().buffer_size(); }

void TCPReceiver::_update_ackno() {
    if (this->stream_out().input_ended()) {
        this->_ackno = wrap(this->_reassembler.reassembled_index() + 2, this->_isn.value());
        return;
    }

    if (this->_isn.has_value()) {
        this->_ackno = wrap(this->_reassembler.reassembled_index() + 1, this->_isn.value());
    }
}
