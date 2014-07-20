/*
 * Copyright (c) 2012. The Regents of the University of California. All rights reserved.
 * Licensed pursuant to the terms and conditions available for viewing at:
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * File: tracker.h
 * Author: Jonathan Ventura
 * Last Modified: 12.2.2012
 */

#ifndef TRACKER_H
#define TRACKER_H

#include <MultiView/multiview.h>

#include "timer.h"

namespace vrlt {
    class Patch;
    class PatchSearch;

/**
 * \addtogroup PatchTracker
 * \brief Patch-based tracking methods.
 * @{
 */
    
    class Tracker
    {
    public:
        Tracker( Node *_root, int _maxnumpoints, std::string method = "ncc", double threshold = 0.7 );
        ~Tracker();
        
        bool do_pose_update;
        
        bool track( Camera *camera_in, bool _use_new_cameras = true );
        
        int firstlevel;
        int lastlevel;
        int maxnumpoints;
        bool verbose;
        int niter;
        int ntracked;
        int nnew;
        int minnumpoints;
        int nattempted;
        float minratio;
        
        Timer cullTimer;

        cv::Mat grid;
        cv::Size gridstep;
        
//        CVD::Image<bool> grid8;
//        CVD::ImageRef gridstep8;
//        CVD::Image<bool> grid16;
//        CVD::ImageRef gridstep16;
//        CVD::Image<bool> grid32;
//        CVD::ImageRef gridstep32;
//        CVD::Image<bool> grid64;
//        CVD::ImageRef gridstep64;
        
    //protected:
        Node *root;
        
        bool use_new_cameras;
        
        Node *mynode;
        Camera *mycamera;
        Calibration *mycalibration;
        
        Eigen::Matrix<float,6,6> cov;
        bool recompute_sigmasq;
        float sigmasq;
        
        Camera *prev_camera;
        Sophus::SE3d prev_pose;
        
        void precomputeGlobalPoses( Node *node );
        void updateMatrices( Camera *source, Camera *target );
        void updateMatrices( Camera *target );
        
        std::vector<Camera*> cameras;
        std::vector<Patch*> patches;
        std::vector<Patch*> visiblePatches;
        std::vector<Patch*> searchPatches;
        PatchSearch *patchSearcher;
        
        void setCurrentCamera( Camera *c );
        Camera *current_camera;
        Eigen::Matrix<float,3,4> poseMatrix;
        float f, u, v, k1, k2;
        friend void checkPoint( void *context, size_t i );
        friend void preparePatch( void *context, size_t i );
        friend void updatePatch( void *context, size_t i );
        
        std::vector< Eigen::Vector2d > prev_features;
        std::vector< Eigen::Vector2d > next_features;
    };
    
/**
 * @}
 */

}

#endif
