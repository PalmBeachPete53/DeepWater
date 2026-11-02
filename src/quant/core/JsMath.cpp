// Math.log, Math.log10 and Math.exp as V8 computes them (src/base/ieee754.cc, a port of fdlibm),
// so indicators built on them (HV, TRIX, Fisher, Choppiness, ALMA) match Node to the last bit.
// The platform libm is more accurate in places, which is exactly what makes it differ.
//
// ====================================================
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
// ====================================================
#include "JsCompat.h"

#include <cstdint>
#include <cstring>

// Unlike the rest of the port (JS arithmetic never fuses), these mirror V8's *compiled C++*: clang
// builds ieee754.cc with its default contraction, which on arm64 turns a*b+c inside an expression
// into one fused multiply-add. Same expressions, same pragma, same bits (checked on 200k inputs).
#if defined(__clang__)
#if defined(__aarch64__) || defined(__arm64__)
#pragma clang fp contract(on)
#else
#pragma clang fp contract(off)
#endif
#endif

namespace qtm::js {

namespace {

inline uint32_t highWord(double x)
{
    uint64_t b;
    std::memcpy(&b, &x, 8);
    return uint32_t(b >> 32);
}
inline uint32_t lowWord(double x)
{
    uint64_t b;
    std::memcpy(&b, &x, 8);
    return uint32_t(b);
}
inline double withHighWord(double x, uint32_t hi)
{
    uint64_t b;
    std::memcpy(&b, &x, 8);
    b = (uint64_t(hi) << 32) | (b & 0xFFFFFFFFu);
    std::memcpy(&x, &b, 8);
    return x;
}
inline double fromWords(uint32_t hi, uint32_t lo)
{
    const uint64_t b = (uint64_t(hi) << 32) | lo;
    double x;
    std::memcpy(&x, &b, 8);
    return x;
}

} // namespace

double log(double x)
{
    static const double ln2_hi = 6.93147180369123816490e-01, /* 3fe62e42 fee00000 */
        ln2_lo = 1.90821492927058770002e-10,                 /* 3dea39ef 35793c76 */
        two54 = 1.80143985094819840000e+16,                  /* 43500000 00000000 */
        Lg1 = 6.666666666666735130e-01,                      /* 3FE55555 55555593 */
        Lg2 = 3.999999999940941908e-01,                      /* 3FD99999 9997FA04 */
        Lg3 = 2.857142874366239149e-01,                      /* 3FD24924 94229359 */
        Lg4 = 2.222219843214978396e-01,                      /* 3FCC71C5 1D8E78AF */
        Lg5 = 1.818357216161805012e-01,                      /* 3FC74664 96CB03DE */
        Lg6 = 1.531383769920937332e-01,                      /* 3FC39A09 D078C69F */
        Lg7 = 1.479819860511658591e-01;                      /* 3FC2F112 DF3E5244 */
    static const double zero = 0.0;
    static volatile double vzero = 0.0;

    double hfsq, f, s, z, R, w, t1, t2, dk;
    int32_t k, hx, i, j;
    uint32_t lx;

    hx = int32_t(highWord(x));
    lx = lowWord(x);

    k = 0;
    if (hx < 0x00100000) { /* x < 2**-1022  */
        if (((hx & 0x7FFFFFFF) | lx) == 0) return -two54 / vzero; /* log(+-0)=-inf */
        if (hx < 0) return (x - x) / zero;                          /* log(-#) = NaN */
        k -= 54;
        x *= two54; /* subnormal number, scale up x */
        hx = int32_t(highWord(x));
    }
    if (hx >= 0x7FF00000) return x + x;
    k += (hx >> 20) - 1023;
    hx &= 0x000FFFFF;
    i = (hx + 0x95F64) & 0x100000;
    x = withHighWord(x, uint32_t(hx | (i ^ 0x3FF00000))); /* normalize x or x/2 */
    k += (i >> 20);
    f = x - 1.0;
    if ((0x000FFFFF & (2 + hx)) < 3) { /* -2**-20 <= f < 2**-20 */
        if (f == zero) {
            if (k == 0) return zero;
            dk = double(k);
            return dk * ln2_hi + dk * ln2_lo;
        }
        R = f * f * (0.5 - 0.33333333333333333 * f);
        if (k == 0) return f - R;
        dk = double(k);
        return dk * ln2_hi - ((R - dk * ln2_lo) - f);
    }
    s = f / (2.0 + f);
    dk = double(k);
    z = s * s;
    i = hx - 0x6147A;
    w = z * z;
    j = 0x6B851 - hx;
    t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    i |= j;
    R = t2 + t1;
    if (i > 0) {
        hfsq = 0.5 * f * f;
        if (k == 0) return f - (hfsq - s * (hfsq + R));
        return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    }
    if (k == 0) return f - s * (f - R);
    return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}

// log10(x) := n*log10_2hi + (n*log10_2lo + ivln10*log(x)), n = ilogb(x) (+1 when negative).
double log10(double x)
{
    static const double two54 = 1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
        ivln10 = 4.34294481903251816668e-01,
        log10_2hi = 3.01029995663611771306e-01, /* 0x3FD34413, 0x509F6000 */
        log10_2lo = 3.69423907715893078616e-13; /* 0x3D59FEF3, 0x11F12B36 */
    static const double zero = 0.0;
    static volatile double vzero = 0.0;

    double y, z;
    int32_t i, k, hx;
    uint32_t lx;

    hx = int32_t(highWord(x));
    lx = lowWord(x);

    k = 0;
    if (hx < 0x00100000) { /* x < 2**-1022  */
        if (((hx & 0x7FFFFFFF) | lx) == 0) return -two54 / vzero; /* log(+-0)=-inf */
        if (hx < 0) return (x - x) / zero;                          /* log(-#) = NaN */
        k -= 54;
        x *= two54; /* subnormal number, scale up x */
        hx = int32_t(highWord(x));
        lx = lowWord(x);
    }
    if (hx >= 0x7FF00000) return x + x;
    if (hx == 0x3FF00000 && lx == 0) return zero; /* log(1) = +0 */
    k += (hx >> 20) - 1023;

    i = int32_t((uint32_t(k) & 0x80000000u) >> 31);
    hx = (hx & 0x000FFFFF) | ((0x3FF - i) << 20);
    y = k + i;
    x = fromWords(uint32_t(hx), lx);

    z = y * log10_2lo + ivln10 * log(x);
    return z + y * log10_2hi;
}

double exp(double x)
{
    static const double one = 1.0, halF[2] = { 0.5, -0.5 },
                        o_threshold = 7.09782712893383973096e+02,  /* 0x40862E42, 0xFEFA39EF */
        u_threshold = -7.45133219101941108420e+02,                 /* 0xC0874910, 0xD52D3051 */
        ln2HI[2] = { 6.93147180369123816490e-01,                   /* 0x3FE62E42, 0xFEE00000 */
                     -6.93147180369123816490e-01 },                /* 0xBFE62E42, 0xFEE00000 */
        ln2LO[2] = { 1.90821492927058770002e-10,                   /* 0x3DEA39EF, 0x35793C76 */
                     -1.90821492927058770002e-10 },                /* 0xBDEA39EF, 0x35793C76 */
        invln2 = 1.44269504088896338700e+00,                       /* 0x3FF71547, 0x652B82FE */
        P1 = 1.66666666666666019037e-01,                           /* 0x3FC55555, 0x5555553E */
        P2 = -2.77777777770155933842e-03,                          /* 0xBF66C16C, 0x16BEBD93 */
        P3 = 6.61375632143793436117e-05,                           /* 0x3F11566A, 0xAF25DE2C */
        P4 = -1.65339022054652515390e-06,                          /* 0xBEBBBD41, 0xC5D26BF1 */
        P5 = 4.13813679705723846039e-08,                           /* 0x3E663769, 0x72BEA4D0 */
        E = 2.718281828459045;                                     /* 0x4005BF0A, 0x8B145769 */
    static volatile double huge = 1.0e+300, twom1000 = 9.33263618503218878990e-302, /* 2**-1000 */
        two1023 = 8.988465674311579539e307;                                        /* 0x1p1023 */

    double y, hi = 0.0, lo = 0.0, c, t, twopk;
    int32_t k = 0, xsb;
    uint32_t hx;

    hx = highWord(x);
    xsb = int32_t((hx >> 31) & 1); /* sign bit of x */
    hx &= 0x7FFFFFFF;              /* high word of |x| */

    /* filter out non-finite argument */
    if (hx >= 0x40862E42) { /* if |x|>=709.78... */
        if (hx >= 0x7FF00000) {
            const uint32_t lx = lowWord(x);
            if (((hx & 0xFFFFF) | lx) != 0) return x + x; /* NaN */
            return (xsb == 0) ? x : 0.0;                  /* exp(+-inf)={inf,0} */
        }
        if (x > o_threshold) return huge * huge;         /* overflow */
        if (x < u_threshold) return twom1000 * twom1000; /* underflow */
    }

    /* argument reduction */
    if (hx > 0x3FD62E42) {     /* if  |x| > 0.5 ln2 */
        if (hx < 0x3FF0A2B2) { /* and |x| < 1.5 ln2 */
            if (x == 1.0) return E;   // V8 special-cases exp(1)
            hi = x - ln2HI[xsb];
            lo = ln2LO[xsb];
            k = 1 - xsb - xsb;
        } else {
            k = int32_t(invln2 * x + halF[xsb]);
            t = k;
            hi = x - t * ln2HI[0]; /* t*ln2HI is exact here */
            lo = t * ln2LO[0];
        }
        x = hi - lo;
    } else if (hx < 0x3E300000) {         /* when |x|<2**-28 */
        if (huge + x > one) return one + x; /* trigger inexact */
    } else {
        k = 0;
    }

    /* x is now in primary range */
    t = x * x;
    if (k >= -1021)
        twopk = fromWords(uint32_t(0x3FF00000 + (k << 20)), 0);
    else
        twopk = fromWords(uint32_t(0x3FF00000 + ((k + 1000) << 20)), 0);
    c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
    if (k == 0) return one - ((x * c) / (c - 2.0) - x);
    y = one - ((lo - (x * c) / (2.0 - c)) - hi);
    if (k >= -1021) {
        if (k == 1024) return y * 2.0 * two1023;
        return y * twopk;
    }
    return y * twopk * twom1000;
}

} // namespace qtm::js
