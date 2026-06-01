// Host shim for <TJpg_Decoder.h> — a REAL JPEG decoder (stb_image) so the sim
// renders the printer camera the same way the device's TJpg_Decoder does:
//   getJpgSize() reports the frame dimensions,
//   drawJpg()    decodes, downscales by the power-of-two scale set via
//                setJpgScale(), and feeds the RGB565 frame to the sketch
//                callback (overlays.cpp's cam_tjpg_cb).
//
// The 565 values produced here are LVGL-native (R<<11 | G<<5 | B). The device
// additionally calls setSwapBytes(true) to match its panel's byte order, but in
// the sim LVGL renders native 565 straight into the framebuffer — so we ignore
// the swap. That's exactly why on-panel colour order remains a hardware-only
// detail even though the sim can now show the picture, framing and scale.
#pragma once
#include <Arduino.h>
#include <cstdlib>

#ifndef STBI_NO_STDIO
#define STBI_NO_STDIO
#endif
#ifndef STBI_ONLY_JPEG
#define STBI_ONLY_JPEG
#endif
#include <stb_image.h>

typedef enum {
    JDR_OK = 0, JDR_INTR, JDR_INP, JDR_MEM1, JDR_MEM2, JDR_PAR,
    JDR_FMT1, JDR_FMT2, JDR_FMT3
} JRESULT;

typedef bool (*SketchCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

class TJpg_Decoder {
   public:
    void setJpgScale(uint8_t s) { scale_ = s ? s : 1; }
    void setSwapBytes(bool s)   { swap_ = s; (void)swap_; }
    void setCallback(SketchCallback cb) { cb_ = cb; }

    JRESULT getJpgSize(uint16_t* w, uint16_t* h, const uint8_t* data, uint32_t len) {
        int iw = 0, ih = 0, comp = 0;
        if (!data || !len ||
            !stbi_info_from_memory(data, (int)len, &iw, &ih, &comp) || iw <= 0 || ih <= 0) {
            if (w) *w = 0;
            if (h) *h = 0;
            return JDR_INP;
        }
        if (w) *w = (uint16_t)iw;
        if (h) *h = (uint16_t)ih;
        return JDR_OK;
    }

    JRESULT drawJpg(int32_t /*x*/, int32_t /*y*/, const uint8_t* data, uint32_t len) {
        if (!cb_ || !data || !len) return JDR_INP;
        int iw = 0, ih = 0, comp = 0;
        uint8_t* rgb = stbi_load_from_memory(data, (int)len, &iw, &ih, &comp, 3);
        if (!rgb) return JDR_INP;
        uint16_t dw = (uint16_t)(iw / scale_);
        uint16_t dh = (uint16_t)(ih / scale_);
        if (dw && dh) {
            uint16_t* px = (uint16_t*)malloc((size_t)dw * dh * 2);
            if (px) {
                for (uint16_t ry = 0; ry < dh; ++ry) {
                    const uint8_t* srow = rgb + (size_t)(ry * scale_) * iw * 3;
                    for (uint16_t rx = 0; rx < dw; ++rx) {
                        const uint8_t* p = srow + (size_t)(rx * scale_) * 3;
                        // LVGL-native RGB565: R[15:11] G[10:5] B[4:0]
                        px[(size_t)ry * dw + rx] =
                            (uint16_t)(((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3));
                    }
                }
                cb_(0, 0, dw, dh, px);
                free(px);
            }
        }
        stbi_image_free(rgb);
        return JDR_OK;
    }

   private:
    SketchCallback cb_   = nullptr;
    uint8_t        scale_ = 1;
    bool           swap_  = false;
};
extern TJpg_Decoder TJpgDec;
