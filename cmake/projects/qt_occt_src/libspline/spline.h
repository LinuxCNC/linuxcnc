#ifndef SPLINE_H
#define SPLINE_H

//internals
#include <iostream>
#include <tuple>
#include <vector>
#include <math.h>
#include <array>
#include <assert.h>
// externals 
#include <eigen3/Eigen/Dense>

using namespace Eigen;
class SplineCurve
{
    private:
        std::vector<Vector3d> points_;              // array of primary setpoints added to plot
        bool elim_colinear_pts_;                    // removes colinear points if enabled

    protected:
        int segments_per_pt_;

        std::vector<Vector3d> pos_profile_;         // Curve of all positions 
        std::vector<Vector3d> vel_profile_;         // Curve velocity profile
        std::vector<Vector3d> accel_profile_;       // Curve acceleration profile
        std::vector<double>   curvature_profile_;   // Curvature (k) of spline between knots. 
                                                    // Each curvature index represents a segment
        std::vector<double>   spline_lengths_;      // lengths of all the spline segments in an array.
        
        /* Override */
        virtual void ResetDerived() = 0;
        enum
        {
            NOM_SIZE = 32   // Max size of the waypoint list
        };

    public:
         
        
        SplineCurve()
        {
            segments_per_pt_ = 50;
            points_.reserve(NOM_SIZE);
            elim_colinear_pts_ = false ;
        }
        SplineCurve(std::vector<Vector3d> points)
        {
            segments_per_pt_ = 50;
            points_.reserve(points.size());
            for(int i = 0; i<points.size(); i++)
            {
                AddPoint(points[i]);
            }
            elim_colinear_pts_ = false ;
        }
        SplineCurve(std::vector<Vector3d> points, int segments_per_pt)
        {
            segments_per_pt_ = segments_per_pt;   
            points_.reserve(points.size());
            for(int i = 0; i<points.size(); i++)
            {
                AddPoint(points[i]);
            }
            elim_colinear_pts_ = false ;
        }
        ~SplineCurve(){}

        /* Getter and Setter Functions */
        const std::vector<Vector3d>& GetPoints() { return points_; }
        const std::vector<Vector3d>& GetPositionProfile() { return pos_profile_; }
        const std::vector<Vector3d>& GetVelocityProfile() { return vel_profile_; }
        const std::vector<Vector3d>& GetAccelerationProfile() { return accel_profile_; }
        const std::vector<double>&   GetCurvatureProfile() { return curvature_profile_; }
        const std::vector<double>&   GetSplineLengths(){return spline_lengths_;}
        
        bool GetElimColinearPoints() { return elim_colinear_pts_; }
        void SetelimColinearPoints(bool elim) { elim_colinear_pts_ = true;}

        /* Virtual Spline Properties */
        /** Determine the cubic function for a spline segmenet between two setpoints 
         * Evaluate will automatically be run from BuildSpline() to determine the cubic functions.
         * 
         * This function returns the state information regarding a specific spline segmeent
         * and save the state information to create a motion profile for 
         * position, velocity, acceleration, and curvature for all t in the list.
         * Evaluate the spline for the ith segment for parameter. 
         * The value of parameter t must be between 0 and 1.
         * 
         */
        virtual std::tuple<Vector3d,Vector3d,Vector3d,double>  EvaluateSplineAtTime(double t) = 0;
        
        /**This function will create your entire spline from an array of waypoints
         * based on the number of steps(divisons) desired for the splines.
         * 
         * The maximum allowable size for a waypoint list is 32. Realistically it
         * shouldn't need to go past that. But if necessary change NOM_SIZE.
         */
        virtual bool BuildSpline(std::vector<Vector3d> setpoints)=0;
        virtual bool BuildSpline()=0;
        
        /**Triangulizes the "A" matrix of a cubic function*/
        virtual bool ComputeSpline() = 0;

        // virtual double ArcLengthIntegrand(int spline, double t) = 0;
        // virtual double Integrate(int spline, double t) = 0;
        // virtual Vector3d ConstVelocitySplineAtTime(double t) = 0;
        
        /** Prints out data pertaining to a specific spline type. */
        virtual void PrintDerivedData() {}

        /** Erase the current waypoint list and state information */
        void Reset();

        /** Adds a waypoint to the end of the spline list
         * Make sure you run BuildSpline() again after you use this
         * to update the relevant state information.
        */
        void AddPoint(const Vector3d& pt);

        /**Prints the data relating to a specific spline segment */
        void PrintData(int segments);

};

#endif 
