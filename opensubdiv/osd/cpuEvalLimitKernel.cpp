//
//     Copyright (C) Pixar. All rights reserved.
//
//     This license governs use of the accompanying software. If you
//     use the software, you accept this license. If you do not accept
//     the license, do not use the software.
//
//     1. Definitions
//     The terms "reproduce," "reproduction," "derivative works," and
//     "distribution" have the same meaning here as under U.S.
//     copyright law.  A "contribution" is the original software, or
//     any additions or changes to the software.
//     A "contributor" is any person or entity that distributes its
//     contribution under this license.
//     "Licensed patents" are a contributor's patent claims that read
//     directly on its contribution.
//
//     2. Grant of Rights
//     (A) Copyright Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free copyright license to reproduce its contribution,
//     prepare derivative works of its contribution, and distribute
//     its contribution or any derivative works that you create.
//     (B) Patent Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free license under its licensed patents to make, have
//     made, use, sell, offer for sale, import, and/or otherwise
//     dispose of its contribution in the software or derivative works
//     of the contribution in the software.
//
//     3. Conditions and Limitations
//     (A) No Trademark License- This license does not grant you
//     rights to use any contributor's name, logo, or trademarks.
//     (B) If you bring a patent claim against any contributor over
//     patents that you claim are infringed by the software, your
//     patent license from such contributor to the software ends
//     automatically.
//     (C) If you distribute any portion of the software, you must
//     retain all copyright, patent, trademark, and attribution
//     notices that are present in the software.
//     (D) If you distribute any portion of the software in source
//     code form, you may do so only under this license by including a
//     complete copy of this license with your distribution. If you
//     distribute any portion of the software in compiled or object
//     code form, you may only do so under a license that complies
//     with this license.
//     (E) The software is licensed "as-is." You bear the risk of
//     using it. The contributors give no express warranties,
//     guarantees or conditions. You may have additional consumer
//     rights under your local laws which this license cannot change.
//     To the extent permitted under your local laws, the contributors
//     exclude the implied warranties of merchantability, fitness for
//     a particular purpose and non-infringement.
//

#include "../osd/cpuEvalLimitKernel.h"

#include <math.h>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <algorithm>
#include <vector>
#include <cassert>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

inline void
evalCubicBSpline(float u, float B[4], float BU[4])
{
    float t = u;
    float s = 1.0f - u;

    float C0 =                      s * (0.5f * s);
    float C1 = t * (s + 0.5f * t) + s * (0.5f * s + t);
    float C2 = t * (    0.5f * t);

    B[0] =                                     1.f/3.f * s                * C0;
    B[1] = (2.f/3.f * s +           t) * C0 + (2.f/3.f * s + 1.f/3.f * t) * C1;
    B[2] = (1.f/3.f * s + 2.f/3.f * t) * C1 + (          s + 2.f/3.f * t) * C2;
    B[3] =                1.f/3.f * t  * C2;

    if (BU) {
        BU[0] =    - C0;
        BU[1] = C0 - C1;
        BU[2] = C1 - C2;
        BU[3] = C2;
    }
}

void
evalBSpline(float u, float v, 
            unsigned int const * vertexIndices,
            OsdVertexBufferDescriptor const & inDesc,
            float const * inQ, 
            OsdVertexBufferDescriptor const & outDesc,
            float * outQ, 
            float * outDQU,
            float * outDQV ) {

    // make sure that we have enough space to store results
    assert( inDesc.length <= (outDesc.stride-outDesc.offset) );

    bool evalDeriv = (outDQU or outDQV);

    // XXX these dynamic allocs won't work w/ VC++
    float B[4], D[4],
          *BU=(float*)alloca(inDesc.length*4*sizeof(float)),
          *DU=(float*)alloca(inDesc.length*4*sizeof(float));
    
    memset(BU, 0, inDesc.length*4*sizeof(float));
    memset(DU, 0, inDesc.length*4*sizeof(float));

    evalCubicBSpline(u, B, evalDeriv ? D : 0);

    float const * inOffset = inQ + inDesc.offset;

    for (int i=0; i<4; ++i) {
        for (int j=0; j<4; ++j) {
        
            float const * in = inOffset + vertexIndices[i+j*4]*inDesc.stride;
            
            for (int k=0; k<inDesc.length; ++k) {
            
                BU[i*inDesc.length+k] += in[k] * B[j];
                
                if (evalDeriv)
                    DU[i*inDesc.length+k] += in[k] * D[j];                
            }
        }
    }

    evalCubicBSpline(v, B, evalDeriv ? D : 0);

    float * Q = outQ + outDesc.offset,
          * dQU = outDQU + outDesc.offset,
          * dQV = outDQV + outDesc.offset;

    // clear result 
    memset(Q, 0, inDesc.length*sizeof(float));
    if (evalDeriv) {
        memset(dQU, 0, inDesc.length*sizeof(float));
        memset(dQV, 0, inDesc.length*sizeof(float));
    }

    for (int i=0; i<4; ++i) {
        for (int k=0; k<inDesc.length; ++k) {
            Q[k] += BU[inDesc.length*i+k] * B[i];
            
            if (evalDeriv) {
                dQU[k] += DU[inDesc.length*i+k] * B[i];
                dQV[k] += BU[inDesc.length*i+k] * D[i];
            }
        }
    }    
}             

inline void
univar4x4(float u, float B[4], float D[4])
{
    float t = u;
    float s = 1.0f - u;

    float A0 = s * s;
    float A1 = 2 * s * t;
    float A2 = t * t;

    B[0] = s * A0;
    B[1] = t * A0 + s * A1;
    B[2] = t * A1 + s * A2;
    B[3] = t * A2;

    if (D) {
        D[0] =    - A0;
        D[1] = A0 - A1;
        D[2] = A1 - A2;
        D[3] = A2;
    }
}

inline float 
csf(unsigned int n, unsigned int j)
{
    if (j%2 == 0) {
        return cosf((2.0f * float(M_PI) * float(float(j-0)/2.0f))/(float(n)+3.0f));
    } else {
        return sinf((2.0f * float(M_PI) * float(float(j-1)/2.0f))/(float(n)+3.0f));
    }
}


void
evalGregory(float u, float v,
            int const * vertexValenceBuffer,
            unsigned int const  * quadOffsetBuffer,
            int maxValence,
            unsigned int const * vertexIndices,
            OsdVertexBufferDescriptor const & inDesc,
            float const * inQ, 
            OsdVertexBufferDescriptor const & outDesc,
            float * outQ, 
            float * outDQU,
            float * outDQV )
{
    static float const ef[7] = {
        0.813008f, 0.500000f, 0.363636f, 0.287505f,
        0.238692f, 0.204549f, 0.179211f
    };
    

    // make sure that we have enough space to store results
    assert( inDesc.length <= (outDesc.stride-outDesc.offset) );

    bool evalDeriv = (outDQU or outDQV);

    int valences[4], length=inDesc.length;
    
    float const * inQo = inQ + inDesc.offset;
    
    float  *r=(float*)alloca(length*4*maxValence*sizeof(float)), *rp=r,
          *e0=(float*)alloca(length*4*sizeof(float)),
          *e1=(float*)alloca(length*4*sizeof(float));
          
    float *opos=(float*)alloca(length*4*sizeof(float));
    
    for (int vid=0; vid < 4; ++vid, rp+=maxValence*length) {
    
        int vertexID = vertexIndices[vid];
        
        const int *valenceTable = vertexValenceBuffer + vertexID * (2*maxValence+1);
        int valence = valenceTable[0];
        valences[vid] = valence;
        
        float  *f=(float*)alloca(maxValence*length*sizeof(float)), *fp=f, 
               *Q=(float*)alloca(length*sizeof(float)),
              *oQ=(float*)alloca(length*sizeof(float));
        memcpy(Q, inQo + vertexID*inDesc.stride, length*sizeof(float));
        memset(oQ, 0, length*sizeof(float));
        
              
        for (int i=0; i<valence; ++i) {
            int im = (i+valence-1)&valence;
            int ip = (i+1)%valence;
            
            int idx_neighbor   = valenceTable[2*i + 0 + 1];
            int idx_diagonal   = valenceTable[2*i + 1 + 1];
            int idx_neighbor_p = valenceTable[2*ip + 0 + 1];
            int idx_neighbor_m = valenceTable[2*im + 0 + 1];
            int idx_diagonal_m = valenceTable[2*im + 1 + 1];

            float const * neighbor   = inQo + idx_neighbor   * inDesc.stride;
            float const * diagonal   = inQo + idx_diagonal   * inDesc.stride;
            float const * neighbor_p = inQo + idx_neighbor_p * inDesc.stride;
            float const * neighbor_m = inQo + idx_neighbor_m * inDesc.stride;
            float const * diagonal_m = inQo + idx_diagonal_m * inDesc.stride;
            
            for (int k=0; k<length; ++k, ++fp, ++rp) {
                *fp = (Q[k]*float(valence) + (neighbor_p[k]+neighbor[k])*2.0f + diagonal[k])/(float(valence)+5.0f);
                oQ[k] += *fp;
                // XXXX manuelk rp indexing is clunky
                *rp = (neighbor_p[k]-neighbor_m[k])/3.0f + (diagonal[k]-diagonal_m[k])/6.0f;
            }
            
        }
        
        for (int k=0; k<length; ++k)
            opos[vid*length+k] = oQ[k]/valence;

        for (int i=0; i<valence; ++i) {

            int im = (i+valence-1)%valence;
            for (int k=0; k<length; ++k) {
            
                float e = 0.5f*(f[i*length+k]+f[im*length+k]);
                e0[vid*length+k] += csf(valence-3, 2*i) * e;
                e1[vid*length+k] += csf(valence-3, 2*i+1) * e;
            }
        }
        
        for (int k=0; k<length; ++k) {
            e0[vid*length+k] *= ef[valence-3];
            e1[vid*length+k] *= ef[valence-3];
        }       
    }
    
    // tess control
    
    float *Ep=(float*)alloca(length*4*sizeof(float)), 
          *Em=(float*)alloca(length*4*sizeof(float)), 
          *Fp=(float*)alloca(length*4*sizeof(float)), 
          *Fm=(float*)alloca(length*4*sizeof(float));
    for (int vid=0; vid<4; ++vid) {
        int ip = (vid+1)%4;
        int im = (vid+3)%4;
        int n = valences[vid];
        const unsigned int *quadOffsets = quadOffsetBuffer;

        int start = quadOffsets[vid] & 0x00ff;
        int prev = (quadOffsets[vid] & 0xff00) / 256;
        
        for (int k=0, ofs=vid*length; k<length; ++k, ++ofs) {
        
            Ep[ofs] = opos[ofs] + e0[ofs] * csf(n-3, 2*start) + e1[ofs]*csf(n-3, 2*start +1);
            Em[ofs] = opos[ofs] + e0[ofs] * csf(n-3, 2*prev ) + e1[ofs]*csf(n-3, 2*prev + 1);
        }
        
        unsigned int np = valences[ip],
                     nm = valences[im];

        unsigned int prev_p = quadOffsets[ip] & 0xff00 / 256;
        
        
        float *Em_ip=(float*)alloca(length*sizeof(float)), 
              *Ep_im=(float*)alloca(length*sizeof(float));
        
        unsigned int start_m = quadOffsets[im] & 0x00ff;
        
        for (int k=0, ipofs=ip*length, imofs=im*length; k<length; ++k, ++ipofs, ++imofs) {
                    
            Em_ip[k] = opos[ipofs] + e0[ipofs]*csf(np-3, 2*prev_p) + e1[ipofs]*csf(np-3, 2*prev_p+1);
            Ep_im[k] = opos[imofs] + e0[imofs]*csf(nm-3, 2*start_m) + e1[imofs]*csf(nm-3, 2*start_m+1);
        }
        
        float s1 = 3.0f - 2.0f*csf(n-3,2)-csf(np-3,2),
              s2 = 2.0f*csf(n-3,2),
              s3 = 3.0f -2.0f*cos(2.0f*float(M_PI)/float(n)) - cos(2.0f*float(M_PI)/float(nm));

        rp = r + vid*maxValence*length;
        for (int k=0, ofs=vid*length; k<length; ++k, ++ofs) {
            Fp[ofs] = (csf(np-3,2)*opos[ofs] + s1*Ep[ofs] + s2*Em_ip[k] + rp[start*length+k])/3.0f;
            Fm[ofs] = (csf(nm-3,2)*opos[ofs] + s3*Em[ofs] + s2*Ep_im[k] - rp[prev*length+k])/3.0f;
        }
    }
    
    float * p[20];    
    for (int i=0, ofs=0; i<4; ++i, ofs+=length) {    
        p[i*5+0] = opos + ofs;
        p[i*5+1] =   Ep + ofs;
        p[i*5+2] =   Em + ofs;
        p[i*5+3] =   Fp + ofs;
        p[i*5+4] =   Fm + ofs;
    }    

    float U = 1-u, V=1-v;
    float d11 = u+v; if(u+v==0.0f) d11 = 1.0f;
    float d12 = U+v; if(U+v==0.0f) d12 = 1.0f;
    float d21 = u+V; if(u+V==0.0f) d21 = 1.0f;
    float d22 = U+V; if(U+V==0.0f) d22 = 1.0f;
    
    float *q=(float*)alloca(length*16*sizeof(float));
    for (int k=0; k<length; ++k) {
        q[ 5*length+k] = (u*p[ 3][k] + v*p[ 4][k])/d11;
        q[ 6*length+k] = (U*p[ 9][k] + v*p[ 8][k])/d12;
        q[ 9*length+k] = (u*p[19][k] + V*p[18][k])/d21;
        q[10*length+k] = (U*p[13][k] + V*p[14][k])/d22;        
    }
    
    memcpy(q+0*length, p[0], length*sizeof(float));
    memcpy(q+1*length, p[1], length*sizeof(float));
    memcpy(q+2*length, p[7], length*sizeof(float));
    memcpy(q+3*length, p[5], length*sizeof(float));
    memcpy(q+4*length, p[2], length*sizeof(float));
    memcpy(q+7*length, p[6], length*sizeof(float));
    memcpy(q+8*length, p[16], length*sizeof(float));
    memcpy(q+11*length, p[12], length*sizeof(float));
    memcpy(q+12*length, p[15], length*sizeof(float));
    memcpy(q+13*length, p[17], length*sizeof(float));
    memcpy(q+14*length, p[11], length*sizeof(float));
    memcpy(q+15*length, p[10], length*sizeof(float));

    float B[4], D[4], 
          *BU=(float*)alloca(inDesc.length*4*sizeof(float)), 
          *DU=(float*)alloca(inDesc.length*4*sizeof(float));
    
    memset(BU, 0, inDesc.length*4*sizeof(float));
    memset(DU, 0, inDesc.length*4*sizeof(float));

    univar4x4(u, B, evalDeriv ? D : 0);

    float const * inOffset = inQ + inDesc.offset;

    for (int i=0; i<4; ++i) {
        for (int j=0; j<4; ++j) {
        
            float const * in = inOffset + vertexIndices[i+j*4]*inDesc.stride;
            
            for (int k=0; k<inDesc.length; ++k) {
            
                BU[i*inDesc.length+k] += in[k] * B[j];
                
                if (evalDeriv)
                    DU[i*inDesc.length+k] += in[k] * D[j];                
            }
            in += inDesc.stride;
        }
    }

    univar4x4(v, B, evalDeriv ? D : 0);

    float * Q = outQ + outDesc.offset;
    float * dQU = outDQU + outDesc.offset;
    float * dQV = outDQV + outDesc.offset;

    // clear result 
    memset(Q, 0, outDesc.length*sizeof(float));
    if (evalDeriv) {
        memset(dQU, 0, outDesc.length*sizeof(float));
        memset(dQV, 0, outDesc.length*sizeof(float));
    }

    for (int i=0; i<4; ++i) {
        for (int k=0; k<inDesc.length; ++k) {
            Q[k] += BU[i] * B[i];
            
            if (evalDeriv) {
                dQU[k] += DU[i] * B[i];
                dQV[k] += BU[i] * D[i];
            }
        }
    }    
}

}  // end namespace OPENSUBDIV_VERSION
}  // end namespace OpenSubdiv
