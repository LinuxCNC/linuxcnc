#ifndef CUBIC_SPLINE_H
#define CUBIC_SPLINE_H
#include "spline.h"

using namespace Eigen;

/** Cubic Spline 
  * Works best with tightly packed waypoint list
  * */
class CubicSpline : public SplineCurve
{
    private:   
        /* The system of linear equations found by solving
         * for the 3 order spline polynomial is given by:
         * A*x = b.  The "x" is represented by x_col_ and the
         * "b" is represented by waypoints in the code.
         *
         * The "A" is formulated with diagonal elements  and
         * symmetric off-diagonal elements.  The
         * general structure (for six  waypoints) looks like:
         *
         *
         *  |  d1  u1   0   0   0  |      | x1 |    | b1 |
         *  |  u1  d2   u2  0   0  |      | x2 |    | b2 |
         *  |  0   u2   d3  u3  0  |   *  | x3 |  = | b3 |
         *  |  0   0    u3  d4  u4 |      | x4 |    | b4 |
         *  |  0   0    0   u4  d5 |      | x5 |    | b5 |
         *
         *
         *  The general derivation for this can be found
         *  in Robert Sedgewick's "Algorithms in C++".
         *
         */
        Vector3d x_col_[NOM_SIZE][4]; // column full of constants to solve for trinomial

    public:
        /** Overall arc length integrand of the curve */
        double ArcLengthIntegrand(double t);

        /** Arc length integrand of a spline segment and point in time */
        double SplineArcLengthIntegrand(int spline, double t);

        /** Integrates for the area under a spline numerically  */
        double IntegrateSpline(int spline, double t);

        /** Integrates for the area under the entire curve by summing the 
         * integrations of all the independent splines.
          */
        double IntegrateCurve(float t0, float t1);

        /** Determines what spline is being used at a specific parametric time 
         * along the curve. Use this to convert from global time to spline time.
         */
        Vector3d SplineAtTime(double t);

        /** Determines the constant velocity along a spline segment. 
         * Use this to determine the velocity from a global time perspective 
         * rather than a segmented time perspective.
         */
        Vector3d ConstVelocitySplineAtTime(double t, double speed);

        /** Determine the length of the entire trajectory curve */
        double EvaluateCurveLength();

        CubicSpline();
        CubicSpline(std::vector<Vector3d> points);
        CubicSpline(std::vector<Vector3d> points, int segments_per_pt);
        ~CubicSpline(){}

        /**
         * Evaluate spline for the ith segment for x,y,z params. 
         * The value of param t must be (0<=t<=1)
         */
        std::tuple<Vector3d,Vector3d,Vector3d,double>  EvaluateSplineAtTime(double t) override;

        // Inherited Functions from spline.h //
        /* Clear out all the data.*/
        void ResetDerived() override;
        bool ComputeSpline() override;
        void PrintDerivedData() override;
        bool BuildSpline() override;
        bool BuildSpline(std::vector<Vector3d> points) override;
        
};

#endif