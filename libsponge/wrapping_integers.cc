#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    auto n_32 = static_cast<uint32_t>(n);
    return isn + n_32;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    auto wrapped_checkpoint = wrap(checkpoint, isn);
    auto diff = n - wrapped_checkpoint;
    uint64_t unsigned_unwrapped = checkpoint + diff;
    int64_t signed_unwrapped = checkpoint + diff;
    // diff < 0 && checkpoint < INT64_MAX 的时候，signed_unwrapped 的转换一定是对的
    // diff >= 0 || checkpoint >= INT64_MAX 的时候，signed_unwrapped 的转换会错，但也一定不是负数
    if ((diff < 0 && checkpoint < INT64_MAX) && signed_unwrapped < 0) {
        unsigned_unwrapped = static_cast<uint32_t>(diff) + checkpoint;
    }
    return unsigned_unwrapped;
}
