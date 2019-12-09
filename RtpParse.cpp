#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <Winsock2.h>
#include <ws2ipdef.h>
#include "RtpParse.h"
using namespace nw;
typedef struct rtp_header {
#ifdef __BIGENDIAN__
    uint16_t version : 2;
    uint16_t padbit : 1;
    uint16_t extbit : 1;
    uint16_t cc : 4;
    uint16_t markbit : 1;
    uint16_t paytype : 7;
#else
    uint16_t cc : 4;
    uint16_t extbit : 1;
    uint16_t padbit : 1;
    uint16_t version : 2;
    uint16_t paytype : 7;
    uint16_t markbit : 1;
#endif
    uint16_t seq_number;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[0];
} rtp_header_t;

static const uint8_t start_sequence[] = { 0, 0, 0, 1 };
int RtpParse::ParseH264Packet(uint8_t *rtpPacket, int len)
{   
    if (!bufptr) {
        bufptr = &buffer[0];
    }
    rtp_header *pRtpHdr = (rtp_header*)rtpPacket;
    uint8_t *buf = rtpPacket + 12;
    len -= 12;

    uint8_t nal = buf[0];
    uint8_t type = nal & 0x1f;
    if (type >= 1 && type <= 23)
        type = 1;

    switch (type) {
    case 0:
    case 1:
        memcpy(bufptr, start_sequence, sizeof(start_sequence));
        bufptr += sizeof(start_sequence);
        memcpy(bufptr, buf, len);
        bufptr += len;

        break;
    case 24:
        /* consume the STAP-A NAL */
        buf++;
        len--;
        /* first we are going to figure out the total size */
        {
            const uint8_t *src = buf;
            int src_len = len;

            while (src_len > 2) {
                uint16_t nal_size = ntohs((uint16_t)*src);

                /* consume the length of the aggregate */
                src += 2;
                src_len -= 2;

                if (nal_size <= src_len) {
                    /* copying */
                    memcpy(bufptr, start_sequence, sizeof(start_sequence));
                    bufptr += sizeof(start_sequence);
                    memcpy(bufptr, src, nal_size);
                    bufptr += nal_size;
                } else {
                    printf("nal size exceeds length: %d %d\n", nal_size, src_len);
                }

                /* eat what we handled */
                src += nal_size;
                src_len -= nal_size;

                if (src_len < 0)
                    printf("Consumed more bytes than we got! (%d)\n", src_len);
            }
        }

        //  printf("type is %d, STAP-A NAL packet\n", type);
        break;
    case 25:
        /* consume the STAP-B NAL and DON */
        buf += 3;
        len -= 3;
        /* first we are going to figure out the total size */
        {
            const uint8_t *src = buf;
            int src_len = len;

            while (src_len > 2) {
                uint16_t nal_size = ntohs((uint16_t)*src);

                /* consume the length of the aggregate */
                src += 2;
                src_len -= 2;

                if (nal_size <= src_len) {
                    /* copying */
                    memcpy(bufptr, start_sequence, sizeof(start_sequence));
                    bufptr += sizeof(start_sequence);
                    memcpy(bufptr, src, nal_size);
                    bufptr += nal_size;
                } else {
                    printf("nal size exceeds length: %d %d\n", nal_size, src_len);
                }

                /* eat what we handled */
                src += nal_size;
                src_len -= nal_size;

                if (src_len < 0)
                    printf("Consumed more bytes than we got! (%d)\n", src_len);
            }
        }

        //  printf("type is %d, STAP-B NAL packet\n", type);
        break;
    case 26:
    case 27:
    case 29:
        printf("Unhandled type (%d)\n", type);
        return -1;
    case 28:
        buf++;
        len--;                 /* skip the fu_indicator */
        if (len > 1) {
            // these are the same as above, we just redo them here for clarity
            uint8_t fu_indicator = nal;
            uint8_t fu_header = *buf;
            uint8_t start_bit = fu_header >> 7;
            uint8_t end_bit = (fu_header & 0x40) >> 6;
            uint8_t nal_type = fu_header & 0x1f;
            uint8_t reconstructed_nal;

            /* Reconstruct this packet's true nal; only the data follows. */
            /* The original nal forbidden bit and NRI are stored in this
            * packet's nal. */
            reconstructed_nal = fu_indicator & 0xe0;
            reconstructed_nal |= nal_type;

            /* skip the fu_header */
            buf++;
            len--;

            if (start_bit) {
                /* copy in the start sequence, and the reconstructed nal */
                memcpy(bufptr, start_sequence, sizeof(start_sequence));
                bufptr += sizeof(start_sequence);

                *(bufptr) = reconstructed_nal;
                bufptr += sizeof(nal);

                memcpy(bufptr, buf, len);
                bufptr += len;
            } else {
                memcpy(bufptr, buf, len);
                bufptr += len;
            }

            if (end_bit) {
                /* dummy */
            }
        } else {
            printf("Too short data for FU-A H264 RTP packet\n");
        }

        //  printf("type is %d, FU-A NAL packet\n", type);
        break;
    default:
        printf("Undefined type (%d)\n", type);
        return -1;
    }
    if (pRtpHdr->markbit) {
        if (_frmCb) {
            _frmCb(buffer, bufptr - &buffer[0], _userdata);
        }
        bufptr = buffer;
    }
    return 0;
}