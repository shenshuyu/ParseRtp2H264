#pragma once
#include <functional>
#define MAX_FRAME_SIZE (800 * 1024)
namespace nw
{
using RtpParseFrmCB = std::function<void(uint8_t *EsFrame, int len, void *userdata)>;
class RtpParse
{
public:
    void RtpSetDataCallCallback(RtpParseFrmCB cb, void *user)      {
        _frmCb = cb;
        _userdata = user;
    }
    int ParseH264Packet(uint8_t *frame, int len);
private:
    RtpParseFrmCB _frmCb    = nullptr;
    void *_userdata         = nullptr;
    uint8_t *bufptr         = nullptr;
    uint8_t  buffer[MAX_FRAME_SIZE];
};
}