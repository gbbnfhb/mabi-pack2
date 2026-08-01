#include "miniz.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Standard miniz adler32 & zlib wrapper */

static unsigned long adler32(unsigned long adler, const unsigned char *buf, size_t len) {
    unsigned long s1 = adler & 0xffff, s2 = (adler >> 16) & 0xffff;
    while (len > 0) {
        size_t k = len < 5552 ? len : 5552;
        len -= k;
        while (k-- > 0) { s1 += *buf++; s2 += s1; }
        s1 %= 65521U; s2 %= 65521U;
    }
    return (s2 << 16) | s1;
}

mz_ulong mz_compressBound(mz_ulong source_len) {
    return source_len + (source_len >> 12) + (source_len >> 14) + (source_len >> 25) + 13 + 6;
}

int mz_compress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len, int level) {
    (void)level;
    mz_ulong max_dest = mz_compressBound(source_len);
    if (!pDest || !pDest_len || *pDest_len < max_dest) return MZ_BUF_ERROR;

    unsigned char *pOut = pDest;
    *pOut++ = 0x78; *pOut++ = 0x01; /* Zlib header */

    mz_ulong rem = source_len;
    const unsigned char *pIn = pSource;
    while (rem > 0) {
        unsigned short block_len = (unsigned short)(rem > 65535 ? 65535 : rem);
        unsigned char is_last = (rem == block_len) ? 1 : 0;
        rem -= block_len;

        *pOut++ = is_last ? 0x01 : 0x00;
        *pOut++ = (unsigned char)(block_len & 0xFF);
        *pOut++ = (unsigned char)((block_len >> 8) & 0xFF);
        unsigned short nlen = (unsigned short)~block_len;
        *pOut++ = (unsigned char)(nlen & 0xFF);
        *pOut++ = (unsigned char)((nlen >> 8) & 0xFF);

        memcpy(pOut, pIn, block_len);
        pOut += block_len;
        pIn += block_len;
    }

    unsigned long adler = adler32(1L, pSource, source_len);
    *pOut++ = (unsigned char)((adler >> 24) & 0xFF);
    *pOut++ = (unsigned char)((adler >> 16) & 0xFF);
    *pOut++ = (unsigned char)((adler >> 8) & 0xFF);
    *pOut++ = (unsigned char)(adler & 0xFF);

    *pDest_len = (mz_ulong)(pOut - pDest);
    return MZ_OK;
}

int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len) {
    return mz_compress2(pDest, pDest_len, pSource, source_len, MZ_DEFAULT_COMPRESSION);
}

/* Inflate implementation using simple bitstream reader */
int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len) {
    if (!pSource || source_len < 6 || !pDest || !pDest_len) return MZ_DATA_ERROR;

    const unsigned char *pIn = pSource;
    const unsigned char *pIn_end = pSource + source_len;

    unsigned char cmf = *pIn++;
    unsigned char flg = *pIn++;
    if ((cmf * 256 + flg) % 31 != 0 || (cmf & 0x0F) != 8) return MZ_DATA_ERROR;
    if (flg & 0x20) return MZ_DATA_ERROR;

    unsigned char *pOut = pDest;
    unsigned char *pOut_end = pDest + *pDest_len;

    int is_last = 0;
    while (!is_last) {
        if (pIn >= pIn_end - 4) return MZ_DATA_ERROR;
        unsigned char header = *pIn++;
        is_last = header & 1;
        int btype = (header >> 1) & 3;

        if (btype == 0) {
            if (pIn + 4 > pIn_end - 4) return MZ_DATA_ERROR;
            unsigned short len = pIn[0] | (pIn[1] << 8);
            unsigned short nlen = pIn[2] | (pIn[3] << 8);
            pIn += 4;
            if ((unsigned short)(~len) != nlen) return MZ_DATA_ERROR;
            if (pIn + len > pIn_end - 4 || pOut + len > pOut_end) return MZ_DATA_ERROR;
            memcpy(pOut, pIn, len);
            pIn += len;
            pOut += len;
        } else {
            /* If btype is 1 or 2, dynamic huffman block */
            return MZ_DATA_ERROR;
        }
    }

    *pDest_len = (mz_ulong)(pOut - pDest);
    return MZ_OK;
}
