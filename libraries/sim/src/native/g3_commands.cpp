/* SPDX-License-Identifier: MIT */
#include <simulator/g3_handler.h>
#include <simulator/drawmsg.h>
#include <cstring>
#include <cstdint>

namespace {
int words(unsigned op)
{
    switch(op) {
    case 0: case 0x11: case 0x15: case 0x41: return 0;
    case 0x10: case 0x12: case 0x13: case 0x14:
    case 0x20: case 0x21: case 0x22: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28:
    case 0x29: case 0x2a: case 0x2b: case 0x30: case 0x31: case 0x32: case 0x33:
    case 0x40: case 0x50: case 0x60: case 0x72: return 1;
    case 0x23: case 0x71: return 2;
    case 0x1b: case 0x1c: case 0x70: return 3;
    case 0x1a: return 9;
    case 0x17: case 0x19: return 12;
    case 0x16: case 0x18: return 16;
    case 0x34: return 32;
    default: return -1;
    }
}
void execute(unsigned op,const uint32_t *p)
{
    draw_msg_t msg={};
    switch(op) {
    case 0: return;
#define COMMAND(opcode,kind) case opcode: msg.type=kind; break
    COMMAND(0x10,DRAW_CMD_G3_MTXMODE);
    COMMAND(0x11,DRAW_CMD_G3_PUSHMTX);
    COMMAND(0x12,DRAW_CMD_G3_POPMTX);
    COMMAND(0x13,DRAW_CMD_G3_STOREMTX);
    COMMAND(0x14,DRAW_CMD_G3_RESTOREMTX);
    COMMAND(0x15,DRAW_CMD_G3_IDENTITY);
    COMMAND(0x16,DRAW_CMD_G3_LOADMTX44);
    COMMAND(0x17,DRAW_CMD_G3_LOADMTX43);
    COMMAND(0x18,DRAW_CMD_G3_MTXMULT44);
    COMMAND(0x19,DRAW_CMD_G3_MTXMULT43);
    COMMAND(0x1a,DRAW_CMD_G3_MTXMULT33);
    COMMAND(0x1b,DRAW_CMD_G3_SCALE);
    COMMAND(0x1c,DRAW_CMD_G3_TRANSLATE);
    COMMAND(0x20,DRAW_CMD_G3_COLOR);
    COMMAND(0x21,DRAW_CMD_G3_NORMAL);
    COMMAND(0x22,DRAW_CMD_G3_TEXCOORD);
    COMMAND(0x23,DRAW_CMD_G3_VTX16);
    COMMAND(0x24,DRAW_CMD_G3_VTX10);
    COMMAND(0x25,DRAW_CMD_G3_VTXXY);
    COMMAND(0x26,DRAW_CMD_G3_VTXXZ);
    COMMAND(0x27,DRAW_CMD_G3_VTXYZ);
    COMMAND(0x28,DRAW_CMD_G3_VTXDIFF);
    COMMAND(0x29,DRAW_CMD_G3_POLYGONATTR);
    COMMAND(0x2a,DRAW_CMD_G3_TEXIMAGEPARAM);
    COMMAND(0x2b,DRAW_CMD_G3_TEXPLTTBASE);
    COMMAND(0x30,DRAW_CMD_G3_DIFF_AMB);
    COMMAND(0x31,DRAW_CMD_G3_SPEC_EMI);
    COMMAND(0x32,DRAW_CMD_G3_LIGHTVECTOR);
    COMMAND(0x33,DRAW_CMD_G3_LIGHTCOLOR);
    COMMAND(0x34,DRAW_CMD_G3_SHININESS);
    COMMAND(0x40,DRAW_CMD_G3_BEGIN);
    COMMAND(0x41,DRAW_CMD_G3_END);
    COMMAND(0x50,DRAW_CMD_G3_SWAPBUFFERS);
    COMMAND(0x60,DRAW_CMD_G3_VIEWPORT);
    COMMAND(0x70,DRAW_CMD_G3_BOXTEST);
    COMMAND(0x71,DRAW_CMD_G3_POSITIONTEST);
    COMMAND(0x72,DRAW_CMD_G3_VECTORTEST);
#undef COMMAND
    }
    int n=words(op);
    if(n)std::memcpy(msg.data.numsU32,p,n*4);
    if(op==0x23||op==0x71) {
        msg.data.xyz.x=(int16_t)p[0]; msg.data.xyz.y=(int16_t)(p[0]>>16); msg.data.xyz.z=(int16_t)p[1];
    } else if(op==0x25) { msg.data.xyz.x=(int16_t)p[0]; msg.data.xyz.y=(int16_t)(p[0]>>16); }
    else if(op==0x26) { msg.data.xyz.x=(int16_t)p[0]; msg.data.xyz.z=(int16_t)(p[0]>>16); }
    else if(op==0x27) { msg.data.xyz.y=(int16_t)p[0]; msg.data.xyz.z=(int16_t)(p[0]>>16); }
    SIM_HandleG3Command(&msg);
}
}
int G3Native_CommandList(const void *data,size_t size)
{
    if((size && !data)||size%4)return 0;
    const auto *bytes=static_cast<const uint8_t *>(data);
    /* Validate before changing geometry state. Every FIFO parameter is a word,
     * including POP, partial vertices and BOX_TEST's packed signed halfwords. */
    for(unsigned pass=0;pass<2;++pass) {
        size_t offset=0;
        while(offset<size) {
            uint32_t packed; std::memcpy(&packed,bytes+offset,4); offset+=4;
            for(unsigned i=0;i<4;++i) {
                unsigned op=(packed>>(8*i))&255; int n=words(op);
                if(n<0||size-offset<size_t(n)*4)return 0;
                if(pass) { uint32_t params[32]={}; std::memcpy(params,bytes+offset,n*4); execute(op,params); }
                offset+=n*4;
            }
        }
    }
    return 1;
}

extern "C" int G3Native_Execute(u32 op,const u32 *params,size_t count)
{
    int n=words(op);
    if(n<0||count!=(size_t)n||(count&&!params))return 0;
    execute(op,params); return 1;
}
