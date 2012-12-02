/*
 * Copyright (c) 2012. The Regents of the University of California. All rights reserved.
 * Licensed pursuant to the terms and conditions available for viewing at:
 * http://opensource.org/licenses/BSD-3-Clause
 * and the terms and conditions of the GNU Lesser General Public License Version 2.1.
 *
 * File: nccsearch.cpp
 * Author: Jonathan Ventura
 * Last Modified: 12.2.2012
 */

#include <PatchTracker/nccsearch.h>
#include <PatchTracker/ncc.h>

#include <cvd/image.h>
#include <cvd/image_interpolate.h>
#include <cvd/image_convert.h>
#include <cvd/vision.h>

#include <TooN/svd.h>

#ifdef USE_DISPATCH
#include <dispatch/dispatch.h>
#endif

namespace vrlt
{
    using namespace std;
    using namespace TooN;
    using namespace CVD;
        
    PatchSearchNCC::PatchSearchNCC( int maxnumpoints )
    {
        lowThreshold = highThreshold = 0.7;
        templates = new byte[8 * 8 * maxnumpoints];
        targets = new byte[8 * 8 * maxnumpoints];
        templateA = new float[maxnumpoints];
        templateC = new float[maxnumpoints];
    }
    
    PatchSearchNCC::~PatchSearchNCC()
    {
        delete [] templates;
        delete [] targets;
        delete [] templateA;
        delete [] templateC;
    }
    
    static void makeNCCTemplate( void *context, size_t i )
    {
        PatchSearchNCC *searcher = (PatchSearchNCC*)context;
        Patch *patch = *(searcher->begin+i);
        
        byte *templatePtr = searcher->templates + i*64;
        float *Aptr = searcher->templateA + i;
        float *Cptr = searcher->templateC + i;
        
        BasicImage<byte> templatePatch( templatePtr, ImageRef(8,8) );
        
        int level = 0;
        float myscale = patch->scale;
        while ( myscale > 3. ) {
            if ( level == NLEVELS-1 ) break;
            level++;
            myscale /= 4.;
        }
        if ( myscale > 3. || myscale < 0.25 ) {
            patch->shouldTrack = false;
            return;
        }
        float levelScale = powf( 2.f, -level );
        
        bool good = patch->sampler.samplePatch( patch->source->pyramid.levels[level].image, patch->warpcenter, patch->warp, levelScale, templatePatch );
        
        if ( !good ) {
            patch->shouldTrack = false;
            return;
        }
        
        patch->index = i;
        
        unsigned int A = computeSum( templatePtr );
        unsigned int B = computeSumSq( templatePtr );
        
        float Af = A;
        float Bf = B;
        
        float Cf = 1.f / sqrtf( 64 * Bf - Af * Af );
        *Aptr = Af;
        *Cptr = Cf;
    }
    
    int PatchSearchNCC::makeTemplates( int count )
    {
#ifdef USE_DISPATCH
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_apply_f( count, queue, this, makeNCCTemplate );
#else
        for ( int i = 0; i < count; i++ ) makeNCCTemplate( this, i );
#endif
        
        int newcount = 0;
        for ( int i = 0; i < count; i++ )
        {
            Patch *patch = *(begin+i);
            if ( patch->shouldTrack ) newcount++;
        }
        
        return newcount;
    }
    
    static inline float getNCC( byte *targetPtr, byte *templatePtr, float templateA, float templateC )
    {
        unsigned int A = computeSum( targetPtr );
        unsigned int B = computeSumSq( targetPtr );
        unsigned int D = computeDotProduct( targetPtr, templatePtr );
        
        float Af = A;
        float Bf = B;
        float Cf = 1.f / sqrtf( 64 * Bf - Af * Af );
        float Df = D;
        float score = ( 64 * Df - Af * templateA ) * Cf * templateC;
        return score;
    }
    
    static inline float getNCC( byte *targetPtr, int targetRowStep, byte *templatePtr, float templateA, float templateC )
    {
        unsigned int A = computeSum( targetPtr, targetRowStep );
        unsigned int B = computeSumSq( targetPtr, targetRowStep );
        unsigned int D = computeDotProduct( targetPtr, targetRowStep, templatePtr );
        
        float Af = A;
        float Bf = B;
        float Cf = 1.f / sqrtf( 64 * Bf - Af * Af );
        float Df = D;
        float score = ( 64 * Df - Af * templateA ) * Cf * templateC;
        return score;
    }
    
    void searchNCCTemplate( void *context, size_t i )
    {
        PatchSearchNCC *searcher = (PatchSearchNCC*)context;
        Patch *patch = *(searcher->begin+i);
        
        size_t index = patch->index;
        
        const int w = patch->target->image.size().x;
        
        byte *templatePtr = searcher->templates + index*64;
        float *Aptr = searcher->templateA + index;
        float *Cptr = searcher->templateC + index;
        
        byte *targetPtr;
        int targetRowStep;
        
        if ( searcher->subsample ) {
            targetPtr = searcher->targets + index*64;
            targetRowStep = 8;
        } else {
            targetPtr = NULL;
            targetRowStep = w;
        }
        
        BasicImage<byte> templatePatch( templatePtr, ImageRef(8,8) );
        BasicImage<byte> targetPatch( targetPtr, ImageRef(8,8) );
        
        ImageRef bestLoc(0,0);
        float nextBestScore = -INFINITY;
        float bestScore = 0;
        int num_inliers = 0;
        
        Vector<2,float> offset = makeVector<float>( 3.5, 3.5 );
        Vector<2,float> center = patch->center - offset;
        
        ImageRef ir_offset( 4, 4 );
        ImageRef ir_origin = ImageRef( (int) round( center[0] ), (int) round( center[1] ) ) - ir_offset;
        
        Matrix<8,8,float> scores = Zeros;
        
        int lower = 0;
        int upper = 8;
        
        for ( int y = lower; y < upper; y++ ) {
            for ( int x = lower; x < upper; x++ ) {
                if ( searcher->subsample ) {
                    Vector<2> pt = center + makeVector( x, y );
                    bool good = patch->sampler.samplePatch( patch->target->image, pt, targetPatch );
                    if ( !good ) continue;
                } else {
                    ImageRef my_origin = ir_origin + ImageRef(x,y);
                    if ( !patch->target->image.in_image( my_origin ) ) continue;
                    if ( !patch->target->image.in_image( my_origin + targetPatch.size() ) ) continue;
                    
                    targetPtr = patch->target->image.data() + my_origin.y * w + my_origin.x;
                }
                
                float score = getNCC( targetPtr, targetRowStep, templatePtr, (*Aptr), (*Cptr) );
                
                if ( isnan( score ) ) score = 0.f;
                scores(y,x) = score;
                
                if ( score >= searcher->lowThreshold )
                {
                    num_inliers++;
                }
                if ( score > bestScore )
                {
                    bestLoc = ImageRef(x,y);
                    nextBestScore = bestScore;
                    bestScore = score;
                }
            }
        }
        
        if ( num_inliers == 0 ) {
            patch->shouldTrack = false;
            return;
        }

        patch->bestLevel = searcher->level;
        
        if ( searcher->subsample ) {
            patch->targetPos = center + makeVector<float>( bestLoc.x, bestLoc.y );
        } else {
            patch->targetPos = makeVector<float>( ir_origin.x + ir_offset.x, ir_origin.y + ir_offset.y ) + makeVector<float>( bestLoc.x, bestLoc.y );
        }
        
        
        patch->targetScore = bestScore;
    }
    
    int PatchSearchNCC::doSearch( int count )
    {
#ifdef USE_DISPATCH
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_apply_f( count, queue, this, searchNCCTemplate );
#else
        for ( int i = 0; i < count; i++ ) searchNCCTemplate( this, i );
#endif
        
        int newcount = 0;
        for ( int i = 0; i < count; i++ )
        {
            Patch *patch = *(begin+i);
            if ( patch->bestLevel == level ) newcount++;
        }
        
        return newcount;
    }
}
