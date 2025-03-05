#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \returns the number of `bytes` that can be written right now.
size_t TCPConnection::remaining_outbound_capacity() const {
    return this->_cfg.send_capacity - this->_sender.stream_buffer();
}

size_t TCPConnection::bytes_in_flight() const { return this->_sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return this->_receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const {
    if (!this->_active) {
        return 0;
    }

    return this->_now_timestamp - this->_last_segment_received_timestamp;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 不满足情况的需要关闭
    if (!this->_active) {
        return;
    }

    // RST 为真，表示连接出错被重置
    if (seg.header().rst) {
        this->close();
        this->_sender.stream_in().set_error();
        this->_receiver.stream_out().set_error();
        return;
    }

    // 更新上次 segment received 的时间戳
    this->_last_segment_received_timestamp = this->_now_timestamp;

    // 将字段传给 Receiver 进行处理
    this->_receiver.segment_received(seg);

    // keep-alive
    // 在发送 segment 之前，
    // TCPConnection 会读取 TCPReceiver 中关于 ackno 和 window_size 相关的信息。
    // 如果当前 TCPReceiver 里有一个合法的 ackno，
    // TCPConnection 会更改 TCPSegment 里的 ACK 标志位，将其设置为真。
    if (this->_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == this->_receiver.ackno().value() - 1) {
        this->_sender.send_empty_segment();
        this->send_segment_from_sender_with_ack();
    }

    // 如果 ACK 标志位为真，通知 TCPSender 有 segment 被确认
    if (seg.header().ack) {
        // TODO 这里实现的有点疑问
        if (!this->_sender.has_sent_syn()) {
            return;
        }
        this->_sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // 写入对象
    if (seg.payload().size() > 0) {
        this->write(seg.payload().copy());
    }

    // 如果收到的 segment 不为空，TCPConnection 必须确保至少给这个 segment 回复一个 ACK
    this->_sender.fill_window();
    size_t count = this->send_segment_from_sender_with_ack();
    // sender 此时如果不发送数据，则至少要发送一个 ACK 的数据
    if (count == 0 && (!seg.header().ack || seg.header().syn || seg.header().fin) && seg.length_in_sequence_space() > 0) {
        this->_sender.send_empty_segment();
        this->send_segment_from_sender_with_ack();
    }

    // 这种情况下，我方为被动关闭
    // 因为 receiver 中 steam 已经被读取完毕但是 sender 中 stream 还没发送完
    if (!this->_sender.stream_in().eof() && this->_receiver.stream_out().input_ended()) {
        this->_linger_after_streams_finish = false;
    }

    // 这个时候表示关闭的判断
    // 如果没有必要 linger，那么直接关闭就可以了
    // 如果需要 linger，时间大于 120 秒的时候，在 tick 中关闭
    if (this->check_all_data_ready()) {
        if (!this->_linger_after_streams_finish) {
            this->close();
        }
    }
}

bool TCPConnection::active() const { return this->_active; }

// 写入数据的同时，也要触发一下发送的机制
size_t TCPConnection::write(const string &data) {
    if (!this->_active) {
        return 0;
    }
    size_t size = this->_sender.stream_in().write(data);
    this->_sender.fill_window();
    this->send_segment_from_sender_with_ack();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!this->_active) {
        return;
    }

    // 时间流逝
    this->_now_timestamp += ms_since_last_tick;
    this->_sender.tick(ms_since_last_tick);

    // 如果同一个 segment 连续重传的次数超过 TCPConfig::MAX_RETX_ATTEMPTS，终止连接，
    // 并且给对方发送一个 reset segment（一个 RST 为真的空 segment）。
    if (this->_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        this->send_rst_segment();
        return;
    }

    // linger 等待 10 * the_initial_retransmission_timeout 的时间后，主动关闭
    if (this->_sender.has_sent_fin() &&
        this->_linger_after_streams_finish &&
        this->time_since_last_segment_received() >= 10 * this->_cfg.rt_timeout) {
        this->close();
    }

    // 这个是因为 sender 重传的时候有可能把 segment 放入了 sender 的 segment_out
    // 所以我们这边也需要跟着重传发送一下
    this->send_segment_from_sender_with_ack();
}

// 主动发起关闭连接的请求
void TCPConnection::end_input_stream() {
    this->_sender.stream_in().end_input();
    this->_sender.fill_window();
    this->send_segment_from_sender_with_ack();

    // 如果此时 receiver 已经读完所有的字节流，则无需进入 linger 状态，发送完 fin 后，直接关闭
    // if (this->_receiver.stream_out().input_ended()) {
    //    this->close();
    //    return;
    // }

    // 等待 linger 计时，TCPConnection 要等待 10 * the_initial_retransmission_timeout 的时间
    // this->_linger_after_streams_finish = true;
}

// 发送一个带有 SYN 标志位的数据，表示开启建立连接
void TCPConnection::connect() {
    if (!this->_active) {
        return;
    }
    this->_sender.fill_window();
    this->send_segment_from_sender_with_ack();
}

// 检查当前的状态是否可以主动关闭连接了
// 它表示：我方已经收到了对面的所有 data，并且我方的数据已经发完了
// 如果要关闭连接，则还需要确认：出向 stream 发出的 segment 都收到了来自远端的 ACK。
bool TCPConnection::check_all_data_ready() const {
    // 入向 stream 已经被完全地接收、排列整齐，并被上层调用者读取完毕。
    bool receiver_stream_finished_flag = this->_receiver.stream_out().input_ended();

    // 出向 stream 已经被上层应用关闭，即 stream 不会再被写入新的字节
    // 且 stream 里的字节流已经全被发送了出去
    // 且出向 stream 发出的 segment 都收到了来自远端的 ACK
    bool sender_stream_finished_flag = this->_sender.all_data_ready();

    return receiver_stream_finished_flag && sender_stream_finished_flag;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Need to send an RST segment to the peer
            this->send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// 理论上来说，只要触发了 sender 中 fill_window 和 send_empty_segment 的操作
// 我们就要进行这个方法
// 这个方法中会将所有要发送的数据，携带 receiver 的 window_size 和 ackno
size_t TCPConnection::send_segment_from_sender_with_ack() {
    size_t count = this->_sender.segments_out().size();
    while (!this->_sender.segments_out().empty()) {
        TCPSegment segment = this->_sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            segment.header().ackno = this->_receiver.ackno().value();
            segment.header().ack = true;
        }
        segment.header().win = min<uint16_t>(UINT16_MAX, this->_receiver.window_size());
        this->_sender.segments_out().pop();
        this->_segments_out.push(segment);
    }
    return count;
}

void TCPConnection::send_rst_segment() {
    // 由于自己已经变成了错误状态，因此原来那些能发送的 segment 也变成了不能发送
    auto empty_segment_with_rst = TCPSegment();
    empty_segment_with_rst.header().rst = true;
    empty_segment_with_rst.header().ackno = this->_receiver.ackno().value();
    empty_segment_with_rst.header().seqno = this->_sender.next_seqno();

    // 清空发送数据结构，之后发送
    while (!this->_sender.segments_out().empty()) {
        this->_sender.segments_out().pop();
    }
    while (!this->segments_out().empty()) {
        this->segments_out().pop();
    }
    this->segments_out().push(empty_segment_with_rst);

    // 发送完 RST 后表示自己有问题了，所以需要把自己也设置为 ERROR 状态
    this->_active = false;
    this->_sender.stream_in().set_error();
    this->_receiver.stream_out().set_error();
}

// 清空发送数据结构后关闭
void TCPConnection::close() {
    while (!this->_sender.segments_out().empty()) {
        this->_sender.segments_out().pop();
    }
    while (!this->segments_out().empty()) {
        this->segments_out().pop();
    }
    this->_active = false;
}
