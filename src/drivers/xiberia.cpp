// XIBERIA K03S Wireless 7.1, receiver 36F9:C001, FF90:0001 (64-byte reports).
// Read-only BLK battery command from src/drivers/mchose_v9pro.cpp and MCHOSE M HUB.
// Hardware lead: https://github.com/wushuai0802/myfinder-device-battery/issues/528
// Local K03S capture: 55 65 01 00 ... -> 55 65 2D 02 ... (45%, discharging).
// The T7 request 55 65 00 00 is echoed unchanged on this receiver: NOT a 0% sample.
// Match the product string as C001 is also shared by S21/K05S, not tested here.
#include "drivers.h"

namespace {

constexpr USHORT kVid = 0x36F9;
constexpr USHORT kPid = 0xC001;
constexpr int kTimeoutMs = 1000;

bool ParseBattery(const std::vector<BYTE>& r, DeviceStatus& s) {
    if (r.size() != 64 || r[0] != 0x55 || r[1] != 0x65 || r[2] > 100) return false;
    if (r[3] == 2) {
        s.percent = r[2];
        s.asleep = false;
        return true;
    }
    // K03S while plugged in: 55 65 34 00 (52), user confirmed charging.
    // State 00 with level 0/1 is ambiguous with the echoed T7/BLK requests: reject it.
    // The charging percentage has not been validated: expose charging, not a fresh level.
    if (r[3] == 0 && r[2] > 1) {
        s.charging = true;
        s.asleep = false;
        return true;
    }
    // Reject query echoes and all states not observed on this K03S hardware.
    return false;
}

}  // namespace

void PollXiberia(PollContext& ctx) {
    if (ctx.IsClaimed(kVid, kPid)) return;
    for (const HidColl* c : ctx.Find(kVid, kPid, [](const HidColl& c) {
             return c.iface == 0 && c.page() == 0xFF90 && c.usage() == 1 &&
                    c.inLen() == 64 && c.outLen() == 64;
         })) {
        if (HidProduct(*c) != L"XIBERIA K03S Wireless 7.1") continue;
        DeviceStatus s;
        s.key = L"xiberia:k03s:c001";
        s.name = L"XIBERIA K03S Wireless 7.1";
        s.kind = DeviceKind::Headset;
        s.via = L"2.4 GHz receiver";
        s.asleep = true;  // no response is never a fresh battery value
        s.graceSec = 1800;

        HidDev d;
        if (d.Open(*c)) {
            d.Flush();
            std::vector<BYTE> request(64, 0), reply;
            request[0] = 0x55;
            request[1] = 0x65;
            request[2] = 0x01;
            const auto deadline = GetTickCount64() + kTimeoutMs;
            bool written = false;
            int n = d.WriteAndRead(request, reply, kTimeoutMs, &written);
            for (int count = 0; written && n > 0 && count < 32; ++count) {
                if (ParseBattery(reply, s)) break;
                const auto now = GetTickCount64();
                if (now >= deadline || count == 31) break;
                n = d.Read(reply, static_cast<int>(deadline - now));
            }
        }
        ctx.Add(s, kVid, kPid);
        return;
    }
}
