#include <cstdint>
#include <cstdio>
extern "C" {
void RunDeathBridge();
extern uint64_t BridgeCapture[28],ExpectedFlags,CollectorStatuesDropReturn;
}
int main() {
    RunDeathBridge();
    bool ok=BridgeCapture[0]==0x123456789abcdefULL
        &&BridgeCapture[1]==0x11111111&&BridgeCapture[2]==0x22222222
        &&BridgeCapture[3]==0x33333333&&BridgeCapture[4]==0x44444444
        &&BridgeCapture[5]==0x55555555&&BridgeCapture[6]==0x66666666
        &&BridgeCapture[7]==CollectorStatuesDropReturn&&BridgeCapture[8]==ExpectedFlags;
    for(unsigned i=0;i<6;++i)ok=ok&&BridgeCapture[9+i]==i+5;
    for(unsigned i=16;i<28;++i)ok=ok&&BridgeCapture[i]==UINT64_MAX;
    std::puts(ok?"PASS: actual MASM bridge preserves volatile registers, flags, XMM0..5, six stack arguments and original return address.":"FAIL: native adapter changed original treasure-call state.");
    return ok?0:1;
}
