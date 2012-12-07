/*
 * Copyright (c) 2012. The Regents of the University of California. All rights reserved.
 * Licensed pursuant to the terms and conditions available for viewing at:
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * File: featurematcher.cpp
 * Author: Jonathan Ventura
 * Last Modified: 11.17.2012
 */

#include <FeatureMatcher/featurematcher.h>

#include <vector>
#include <set>

#include <cvd/timer.h>

namespace vrlt {
    
    using namespace std;

    static void addFeatures( Camera *camera, bool triangulated, vector<Feature*> &features )
    {
        ElementList::iterator it;
        for ( it = camera->features.begin(); it != camera->features.end(); it++ )
        {
            Feature *feature = (Feature *)it->second;
            if ( triangulated )
            {
                if ( feature->track == NULL ) continue;
                if ( feature->track->point == NULL ) continue;
            }
            features.push_back( feature );
        }
    }
    
    void addFeatures( Node *node, bool triangulated, vector<Feature*> &features )
    {
        if ( node->camera != NULL )
        {
            addFeatures( node->camera, triangulated, features );
        }
        
        ElementList::iterator it;
        for ( it = node->children.begin(); it != node->children.end(); it++ )
        {
            Node *child = (Node *)it->second;
            
            addFeatures( child, triangulated, features );
        }
    }

    FeatureMatcher::FeatureMatcher( NN *nn, bool _deleteNN ) : index( nn ), deleteNN( _deleteNN ), deleteFeatures( false ), data( NULL )
    {
    }

    void FeatureMatcher::init( Node *node, bool triangulated, bool averageDescriptors )
    {
        if ( !features.empty() ) {
            delete [] data;
            data = NULL;
            if ( deleteFeatures ) {
                for ( int i = 0; i < features.size(); i++ ) {
                    delete [] features[i]->descriptor;
                    delete features[i];
                }
            }
            features.clear();
        }
        
        addFeatures( node, triangulated, features );
        if ( features.empty() ) return;
        
        if ( averageDescriptors )
        {
            set<Point*> points;
            for ( int i = 0; i < features.size(); i++ ) points.insert( features[i]->track->point );
            vector<Feature*> newfeatures;
            for ( set<Point*>::iterator it = points.begin(); it != points.end(); it++ )
            {
                Point *point = *it;
                Feature *feature = new Feature;
                feature->track = point->track;
                unsigned int sum[128];
                for ( int j = 0; j < 128; j++ ) sum[j] = 0;
                int count = 0;
                for ( int i = 0; i < features.size(); i++ )
                {
                    if ( features[i]->track->point != point ) continue;
                    for ( int j = 0; j < 128; j++ ) sum[j] += features[i]->descriptor[j];
                    count++;
                }
                feature->descriptor = new unsigned char[128];
                for ( int j = 0; j < 128; j++ ) feature->descriptor[j] = sum[j] / count;
                newfeatures.push_back( feature );
            }
            features = newfeatures;
            deleteFeatures = true;
        }
        
        
        int count = features.size();
        data = new unsigned char[count*128];
        unsigned char *ptr = data;
        for ( int i = 0; i < count; i++,ptr+=128 ) {
            memcpy( ptr, features[i]->descriptor, 128 );
        }
        
        index->setData( count, data );
    }
        
    FeatureMatcher::~FeatureMatcher()
    {
	if ( deleteNN ) delete index;
        delete [] data;
        if ( deleteFeatures ) {
            for ( int i = 0; i < features.size(); i++ ) {
                delete [] features[i]->descriptor;
                delete features[i];
            }
        }
    }
    
    void FeatureMatcher::search( Feature *feature, int k, int *neighbors, unsigned int *distances_sq )
    {
        unsigned char *query = feature->descriptor;
        if ( k == 1 ) {
            index->findnn( 1, query, neighbors, distances_sq );
        } else {
            index->findknn( 1, query, k, neighbors, distances_sq );
        }
    }
    
    void FeatureMatcher::search( vector<Feature*> &_features, int *neighbors, unsigned int *distances_sq )
    {
        int num_queries = _features.size();
        unsigned char *queries = new unsigned char[num_queries*128];
        unsigned char *ptr = queries;
        for ( int i = 0; i < num_queries; i++,ptr+=128 ) {
            memcpy( ptr, _features[i]->descriptor, 128 );
        }
        index->findnn( num_queries, queries, neighbors, distances_sq );
        delete [] queries;
    }
    
    void FeatureMatcher::searchconsistent( vector<Feature*> &_features, int *neighbors, unsigned int *distances_sq )
    {
        int num_queries = _features.size();
        unsigned char *queries = new unsigned char[num_queries*128];
        unsigned char *ptr = queries;
        for ( int i = 0; i < num_queries; i++,ptr+=128 ) {
            memcpy( ptr, _features[i]->descriptor, 128 );
        }
        index->findconsistentnn( num_queries, queries, neighbors, distances_sq );
        delete [] queries;
        
        return;
    }
    
    void FeatureMatcher::search( vector<Feature*> &_features, int k, int *neighbors, unsigned int *distances_sq )
    {
        int num_queries = _features.size();
        unsigned char *queries = new unsigned char[num_queries*128];
        unsigned char *ptr = queries;
        for ( int i = 0; i < num_queries; i++,ptr+=128 ) {
            memcpy( ptr, _features[i]->descriptor, 128 );
        }
        index->findknn( num_queries, queries, k, neighbors, distances_sq );
        delete [] queries;
    }
    
    struct SortMatches
    {
        bool operator()( Match *a, Match *b ) { return a->score < b->score; }
    };
    
    void reverseMatches( vector<Match*> &matches )
    {
        for ( int i = 0; i < matches.size(); i++ )
        {
            Match *match = matches[i];
            Feature *feature1 = match->feature1;
            Feature *feature2 = match->feature2;
            match->feature1 = feature2;
            match->feature2 = feature1;
        }
    }
    
    void findMatches( FeatureMatcher &matcher, std::vector<Feature*> &features, std::vector<Match*> &matches )
    {
        int num_queries = features.size();
        
        int *neighbors = new int[num_queries];
        unsigned int *distances_sq = new unsigned int[num_queries];
        
        matcher.search( features, neighbors, distances_sq );
        
        for ( int i = 0; i < num_queries; i++ )
        {
            if ( neighbors[i] < 0 ) continue;
            
            Match *match = new Match;
            match->score = sqrtf( distances_sq[i] );
            match->feature1 = matcher.features[neighbors[i]];
            match->feature2 = features[i];
            matches.push_back( match );
        }
        
        sort( matches.begin(), matches.end(), SortMatches() );
        
        delete [] neighbors;
        delete [] distances_sq;
    }
    
    void findMatches( FeatureMatcher &matcher, std::vector<Feature*> &features, int k, std::vector<Match*> &matches )
    {
        int num_queries = features.size();
        
        int *neighbors = new int[num_queries*k];
        unsigned int *distances_sq = new unsigned int[num_queries*k];
        
        matcher.search( features, k, neighbors, distances_sq );
        
        int *neighbors_ptr = neighbors;
        unsigned int *distances_sq_ptr = distances_sq;
        
        for ( int i = 0; i < num_queries; i++,neighbors_ptr+=k,distances_sq_ptr+=k )
        {
            for ( int j = 0; j < k; j++ )
            {
                Match *match = new Match;
                match->score = sqrtf( distances_sq_ptr[j] );
                match->feature1 = matcher.features[ neighbors_ptr[j] ];
                match->feature2 = features[i];
                matches.push_back( match );
            }
        }
        
        delete [] neighbors;
        delete [] distances_sq;
        
        sort( matches.begin(), matches.end(), SortMatches() );
    }
        
    void findUniqueMatches( FeatureMatcher &matcher, std::vector<Feature*> &features, double max_ratio, std::vector<Match*> &matches )
    {
        int num_queries = features.size();
        
        int *neighbors = new int[2*num_queries];
        unsigned int *distances_sq = new unsigned int[2*num_queries];
        
        matcher.search( features, 2, neighbors, distances_sq );
        
        int *neighbors_ptr = neighbors;
        unsigned int *distances_sq_ptr = distances_sq;
        
        for ( int i = 0; i < features.size(); i++,neighbors_ptr+=2,distances_sq_ptr+=2 )
        {
            double distance0 = sqrt( distances_sq_ptr[0] );
            double distance1 = sqrt( distances_sq_ptr[1] );
            if ( distance0 > distance1 * max_ratio ) continue;
            
            Match *match = new Match;
            match->score = distance0 / distance1;
            match->feature1 = matcher.features[ neighbors_ptr[0] ];
            match->feature2 = features[i];
            matches.push_back( match );
        }
        
        sort( matches.begin(), matches.end(), SortMatches() );

        delete [] neighbors;
        delete [] distances_sq;
    }
    
    void findConsistentMatches( FeatureMatcher &matcher, std::vector<Feature*> &features, std::vector<Match*> &matches )
    {
        int num_queries = features.size();
        
        int *neighbors = new int[num_queries];
        unsigned int *distances_sq = new unsigned int[num_queries];
        
        matcher.searchconsistent( features, neighbors, distances_sq );
        
        for ( int i = 0; i < num_queries; i++ )
        {
            if ( neighbors[i] == -1 ) continue;
            
            Match *match = new Match;
            match->score = sqrtf( distances_sq[i] );
            match->feature1 = matcher.features[neighbors[i]];
            match->feature2 = features[i];
            matches.push_back( match );
        }
        
        sort( matches.begin(), matches.end(), SortMatches() );
        
        delete [] neighbors;
        delete [] distances_sq;
    }
};
