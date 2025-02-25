#include "receiver_harness.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std;

int main() {
    try {
        auto rd = get_random_generator();

        {
            TCPReceiver receiver = TCPReceiver(10);
            TCPSegment seg1 = TCPSegment();
            seg1.header().syn = true;
            seg1.header().seqno = WrappingInt32(0);
            receiver.segment_received(seg1);

            if (receiver.ackno() != WrappingInt32(1)) {
                cout << "self case 1" << endl;
                return EXIT_FAILURE;
            }

            // 此时已经注入了 0 个字符进去
            if (receiver.window_size() != 10) {
                cout << "self case 2" << endl;
                return EXIT_FAILURE;
            }

            TCPSegment seg2 = TCPSegment();
            seg2.payload() = string("-");
            seg2.header().seqno = WrappingInt32(1);
            receiver.segment_received(seg2);

            if (receiver.ackno() != WrappingInt32(2)) {
                cout << "self case 3" << endl;
                return EXIT_FAILURE;
            }

            // 此时已经注入了 1 个字符进去
            if (receiver.window_size() != 9) {
                cout << "self case 4" << endl;
                return EXIT_FAILURE;
            }

            TCPSegment seg3 = TCPSegment();
            seg3.payload() = string("--");
            seg3.header().seqno = WrappingInt32(3);
            receiver.segment_received(seg3);

            if (receiver.ackno() != WrappingInt32(2)) {
                cout << "self case 5" << endl;
                return EXIT_FAILURE;
            }

            // 此时已经注入了 1 个字符进去
            if (receiver.window_size() != 9) {
                cout << "self case 6" << endl;
                return EXIT_FAILURE;
            }

            if (receiver.unassembled_bytes() != 2) {
                cout << "self case 7" << endl;
                return EXIT_FAILURE;
            }

            string s = receiver.stream_out().read(1);
            if (s != "-") {
                cout << "self case 8" << endl;
                return EXIT_FAILURE;
            }

            if (receiver.window_size() != 10) {
                cout << "self case 9" << endl;
                return EXIT_FAILURE;
            }
        }

        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn + 0).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(SegmentArrives{}.with_fin().with_seqno(isn + 1).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{isn + 2}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectBytes{""});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
        }

        {
            uint32_t isn = uniform_int_distribution<uint32_t>{0, UINT32_MAX}(rd);
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectState{TCPReceiverStateSummary::LISTEN});
            test.execute(SegmentArrives{}.with_syn().with_seqno(isn + 0).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::SYN_RECV});
            test.execute(
                SegmentArrives{}.with_fin().with_seqno(isn + 1).with_data("a").with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
            test.execute(ExpectAckno{WrappingInt32{isn + 3}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectBytes{"a"});
            test.execute(ExpectTotalAssembledBytes{1});
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
        }

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
