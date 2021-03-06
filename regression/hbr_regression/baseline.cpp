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

#include <stdio.h>
#include <typeinfo>
#include <iostream>

#include <hbr/mesh.h>
#include <hbr/face.h>
#include <hbr/vertex.h>
#include <hbr/halfedge.h>
#include <hbr/bilinear.h>
#include <hbr/catmark.h>
#include <hbr/loop.h>

#include "../common/shape_utils.h"

//
// Generates a baseline data set for the hbr_regression tool
//

//------------------------------------------------------------------------------
// Vertex class implementation
struct xyzVV {

    xyzVV() { }

    xyzVV( int /*i*/ ) { }

    xyzVV( float x, float y, float z ) { _pos[0]=x; _pos[1]=y; _pos[2]=z; }

    xyzVV( const xyzVV & src ) { _pos[0]=src._pos[0]; _pos[1]=src._pos[1]; _pos[2]=src._pos[2]; }

   ~xyzVV( ) { }

    void     AddWithWeight(const xyzVV& src, float weight, void * =0 ) { 
        _pos[0]+=weight*src._pos[0]; 
        _pos[1]+=weight*src._pos[1]; 
        _pos[2]+=weight*src._pos[2]; 
    }

    void     AddVaryingWithWeight(const xyzVV& , float, void * =0 ) { }

    void     Clear( void * =0 ) { _pos[0]=_pos[1]=_pos[2]=0.0f; }

    void     SetPosition(float x, float y, float z) { _pos[0]=x; _pos[1]=y; _pos[2]=z; }

    void     ApplyVertexEdit(const OpenSubdiv::HbrVertexEdit<xyzVV> & edit) {
                 const float *src = edit.GetEdit();
                 switch(edit.GetOperation()) {
                   case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Set:
                     _pos[0] = src[0];
                     _pos[1] = src[1];
                     _pos[2] = src[2];
                     break;
                   case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Add:
                     _pos[0] += src[0];
                     _pos[1] += src[1];
                     _pos[2] += src[2];
                     break;
                   case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Subtract:
                     _pos[0] -= src[0];
                     _pos[1] -= src[1];
                     _pos[2] -= src[2];
                     break;
                 }
             }

    void     ApplyMovingVertexEdit(const OpenSubdiv::HbrMovingVertexEdit<xyzVV> &) { }

    const float * GetPos() const { return _pos; }

private:
    float _pos[3];
};

//------------------------------------------------------------------------------
class xyzFV;
typedef OpenSubdiv::HbrMesh<xyzVV>           xyzmesh;
typedef OpenSubdiv::HbrFace<xyzVV>           xyzface;
typedef OpenSubdiv::HbrVertex<xyzVV>         xyzvertex;
typedef OpenSubdiv::HbrHalfedge<xyzVV>       xyzhalfedge;
typedef OpenSubdiv::HbrFaceOperator<xyzVV>   xyzFaceOperator;
typedef OpenSubdiv::HbrVertexOperator<xyzVV> xyzVertexOperator;

#include "./init_shapes.h"

//------------------------------------------------------------------------------
static void generate( char const * shapeStr, char const * name, int levels, Scheme scheme=kCatmark ) {

    assert(shapeStr);
    
    xyzmesh * mesh = simpleHbr<xyzVV>(shapeStr, scheme, 0);
    
    //int nvf = 4;
    //if ( typeid(*(mesh->GetSubdivision())) ==
    //    typeid( OpenSubdiv::HbrLoopSubdivision<xyzVV>) )
    //    nvf = 3;
    
    int firstface=0, lastface=mesh->GetNumFaces(),
        firstvert=0, lastvert=mesh->GetNumVertices();
    
    for (int l=0; l<levels; ++l ) {
    
        std::stringstream fname ;
        fname << name << "_level" << l << ".obj";
    
        printf("    writing \"%s\"\n", fname.str().c_str());
    
        FILE * handle = fopen( fname.str().c_str(), "w" );
        if (not handle) {
            printf("Could not open \"%s\" - aborting.\n", fname.str().c_str());
            exit(0);
        }
    
        // subdivide up to current level
        for (int i=firstface; i<lastface; ++i) {
            xyzface * f = mesh->GetFace(i);
            f->Refine();
        }
        
        firstface = lastface;
        lastface = mesh->GetNumFaces();
        //nfaces = lastface - firstface;

        firstvert = lastvert;
        lastvert = mesh->GetNumVertices();
        //nverts = lastvert - firstvert;

        //fprintf(handle, "static char const * %s = \n", fname.str().c_str());
        fprintf(handle, "# This file uses centimeters as units for non-parametric coordinates.\n");
        
        for (int i=firstvert; i<lastvert; ++i) {
            const float * pos = mesh->GetVertex(i)->GetData().GetPos();
            fprintf(handle, "v  %.*g %.*g %.*g\n", 9, pos[0], 9, pos[1], 9, pos[2]);
        }
              
        fprintf(handle, "s off\n");
        
        for (int i=firstface; i<lastface; ++i) {
            xyzface * f = mesh->GetFace(i);
            
            fprintf(handle, "f ");
            for (int j=0; j<f->GetNumVertices();) {
            
                int vert = f->GetVertex(j)->GetID()-firstvert;
                
                fprintf(handle, "%d/%d/%d", vert, vert, vert);
                
                if (++j<f->GetNumVertices())
                    fprintf(handle, " ");
            }
            fprintf(handle, "\n");
        }
       fprintf(handle, ";\n");
       fclose(handle);
    }

    delete mesh;
}

//------------------------------------------------------------------------------
static void usage(char const * appname) {
    printf("Usage : %s [-shape <x> -scheme <bilinear, catmark, loop>] [file.obj]\n", appname);
    printf("    Valid shapes :\n");
    for (int i=0; i<(int)g_shapes.size(); ++i)
        printf("        %d : %s\n", i, g_shapes[i].name.c_str());
    printf("        %ld : all shapes\n", g_shapes.size());
}

int g_shapeindex=-1;

std::string g_objfile;

Scheme g_scheme=kCatmark;

//------------------------------------------------------------------------------
static void parseArgs(int argc, char ** argv) {

    if (argc==1)
        usage(argv[0]);
    
    for (int i=1; i<argc; ++i) {
        if (not strcmp(argv[i],"-shape")) {
            if (i<(argc-1))
                g_shapeindex =  atoi( argv[++i] );
            if ( g_shapeindex<0 or g_shapeindex>(int)g_shapes.size()) {
                printf("-shape : index must be within [%ld %ld]\n", 0L, g_shapes.size());
                exit(0);
            }
        } else if (not strcmp(argv[i],"-scheme")) {
        
            const char * scheme = NULL;
            
            if (i<(argc-1))
                scheme = argv[++i];
            
            if (not strcmp(scheme,"bilinear"))
                g_scheme = kBilinear;
            else if (not strcmp(scheme,"catmark"))
                g_scheme = kCatmark;
            else if (not strcmp(scheme,"loop"))
                g_scheme = kLoop;
            else {
                printf("-scheme : must be one of (\"bilinear\", \"catmark\", \"loop\")\n");
                exit(0);
            }
        } else {
            if (i<(argc=1))
                g_objfile = argv[++i];
            else
                usage(argv[0]);
        }
    }
}

//------------------------------------------------------------------------------
int main(int argc, char ** argv) {

    int levels=5;

    initShapes();

    parseArgs(argc, argv);
    
    if ( g_objfile.size() ) {
        
        FILE * handle = fopen( g_objfile.c_str(), "rt" );
        if (not handle) {
            printf("Could not open \"%s\" - aborting.\n", g_objfile.c_str());
            exit(0);
        }

        fseek( handle, 0, SEEK_END );
        size_t size = ftell(handle);
        fseek( handle, 0, SEEK_SET );

        char * shapeStr = new char[size];

        if ( fread( shapeStr, size, 1, handle)!=1 ) {
            printf("Error reading \"%s\" - aborting.\n", g_objfile.c_str());
            exit(0);
        }

        fclose(handle);
        
        generate( shapeStr, 
                  g_objfile.c_str(), 
                  levels, 
                  g_scheme );
        
        delete [] shapeStr;
    } else if (g_shapeindex>=0) {
        
        if (g_shapeindex==(int)g_shapes.size()) {
            for (int i=0; i<(int)g_shapes.size(); ++i)
                 generate( g_shapes[i].data.c_str(), 
                           g_shapes[i].name.c_str(), 
                           levels, 
                           g_shapes[i].scheme);
                
        } else
            generate( g_shapes[g_shapeindex].data.c_str(), 
                      g_shapes[g_shapeindex].name.c_str(), 
                      levels, 
                      g_shapes[g_shapeindex].scheme);
    }
}
