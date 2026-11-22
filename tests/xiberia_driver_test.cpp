#include "../src/drivers/drivers.h"
#include <cassert>
#include <cstdio>

namespace {

std::vector<std::vector<BYTE>> pending;
std::wstring product = L"XIBERIA K03S Wireless 7.1";
int opens = 0, reads = 0;
bool openOk = true, writeOk = true;

HidColl Collection() {
    HidColl c;
    c.vid = 0x36F9;
    c.pid = 0xC001;
    c.iface = 0;
    c.caps.UsagePage = 0xFF90;
    c.caps.Usage = 1;
    c.caps.InputReportByteLength = c.caps.OutputReportByteLength = 64;
    return c;
}

std::vector<BYTE> Frame(BYTE level, BYTE state) {
    std::vector<BYTE> r(64, 0);
    r[0] = 0x55;
    r[1] = 0x65;
    r[2] = level;
    r[3] = state;
    return r;
}

PollContext Poll(std::vector<std::vector<BYTE>> frames, HidColl c = Collection(), bool claimed = false) {
    pending = std::move(frames);
    opens = reads = 0;
    PollContext ctx;
    ctx.colls = { c };
    if (claimed) ctx.Claim(c.vid, c.pid);
    PollXiberia(ctx);
    return ctx;
}

}  // namespace

// Production driver linked against a simulated transport; no device access.
bool HidDev::Open(const HidColl& c) { c_ = c; ++opens; return openOk; }
void HidDev::Close() {}
void HidDev::Flush() {}
std::wstring HidProduct(const HidColl&) { return product; }
int HidDev::Read(std::vector<BYTE>& r, int timeout) {
    assert(timeout > 0 && timeout <= 1000);
    ++reads;
    if (pending.empty()) return -1;
    r = pending.front();
    pending.erase(pending.begin());
    return static_cast<int>(r.size());
}
int HidDev::WriteAndRead(const std::vector<BYTE>& req, std::vector<BYTE>& r, int timeout, bool* written) {
    auto expected = Frame(1, 0);
    assert(req == expected);  // only the fixed, read-only battery request is permitted
    *written = writeOk;
    if (!writeOk) return -1;
    return Read(r, timeout);
}
int main() {
    auto ctx = Poll({Frame(45, 2)});
    assert(ctx.out.size() == 1 && ctx.IsClaimed(0x36F9, 0xC001));
    assert(ctx.out[0].percent == 45 && !ctx.out[0].asleep && !ctx.out[0].charging);
    assert(ctx.out[0].kind == DeviceKind::Headset && ctx.out[0].key == L"xiberia:k03s:c001");
    // Echoes, unknown states, malformed frames and missing reads never become fake percentages.
    for (const auto& frame : { Frame(0, 0), Frame(1, 0), Frame(45, 3), Frame(100, 4), Frame(0, 0x1A),
                              Frame(45, 9), Frame(101, 2), std::vector<BYTE>{0x55, 0x65, 45, 2} }) {
        ctx = Poll({frame});
        assert(ctx.out[0].percent == -1 && ctx.out[0].asleep && !ctx.out[0].charging);
    }
    ctx = Poll({}); assert(ctx.out[0].percent == -1 && ctx.out[0].asleep);
    ctx = Poll({Frame(0, 0), Frame(45, 2)}); assert(ctx.out[0].percent == 45);
    ctx = Poll({Frame(52, 0)}); assert(ctx.out[0].charging && !ctx.out[0].asleep && ctx.out[0].percent == -1);
    ctx = Poll({Frame(0, 0x1A)}); assert(ctx.out[0].asleep && ctx.out[0].percent == -1);
    for (BYTE level : {BYTE(0), BYTE(100)}) { ctx = Poll({Frame(level, 2)}); assert(ctx.out[0].percent == level); }
    product = L"XIBERIA S21 Wireless 7.1";
    ctx = Poll({Frame(45, 2)}); assert(ctx.out.empty() && opens == 0);
    product = L"XIBERIA K03S Wireless 7.1";
    for (int change = 0; change < 7; ++change) {
        auto c = Collection();
        if (change == 0) c.pid = 0xF016;
        if (change == 1) c.caps.UsagePage = 0xFF82;
        if (change == 2) c.caps.Usage = 2;
        if (change == 3) c.caps.OutputReportByteLength = 65;
        if (change == 4) c.caps.InputReportByteLength = 65;
        if (change == 5) c.iface = 1;
        if (change == 6) c.vid = 0x291D;
        ctx = Poll({Frame(45, 2)}, c); assert(ctx.out.empty() && opens == 0);
    }
    ctx = Poll({Frame(45, 2)}, Collection(), true); assert(ctx.out.empty() && opens == 0);
    ctx = Poll(std::vector<std::vector<BYTE>>(100, Frame(0, 0)));
    assert(ctx.out[0].percent == -1 && reads <= 32);
    openOk = false;
    ctx = Poll({Frame(45, 2)});
    assert(ctx.out[0].percent == -1 && reads == 0);
    openOk = true;
    writeOk = false;
    ctx = Poll({Frame(45, 2)});
    assert(ctx.out[0].percent == -1 && reads == 0);
    writeOk = true;
    puts("XIBERIA K03S driver tests passed");
}
