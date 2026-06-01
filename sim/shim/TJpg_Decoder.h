// Host shim for <TJpg_Decoder.h> — no-op. The camera overlay compiles, but the
// sim never decodes a frame (getJpgSize reports "not enough input"), so the
// viewer just shows its empty chrome. Real camera decode is hardware-only.
#pragma once
#include <Arduino.h>

typedef enum {
    JDR_OK = 0, JDR_INTR, JDR_INP, JDR_MEM1, JDR_MEM2, JDR_PAR,
    JDR_FMT1, JDR_FMT2, JDR_FMT3
} JRESULT;

typedef bool (*SketchCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

class TJpg_Decoder {
   public:
    void setJpgScale(uint8_t) {}
    void setSwapBytes(bool) {}
    void setCallback(SketchCallback cb) { cb_ = cb; (void)cb_; }
    JRESULT getJpgSize(uint16_t* w, uint16_t* h, const uint8_t*, uint32_t) {
        if (w) *w = 0; if (h) *h = 0; return JDR_INP;
    }
    JRESULT drawJpg(int32_t, int32_t, const uint8_t*, uint32_t) { return JDR_OK; }

   private:
    SketchCallback cb_ = nullptr;
};
extern TJpg_Decoder TJpgDec;
