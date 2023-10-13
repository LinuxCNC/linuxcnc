#include "spline.h"

/* Cubic Bezier Spline (B-Spline) Implementation
 * Based on this article:
 * http://www.particleincell.com/blog/2012/bezier-splines/
 */
class BezierSpline : public SplineCurve
{
    private:
        std::vector<Vector3d> p1_points_;   // First Control Points for a curve
        std::vector<Vector3d> p2_points_;   // Control Points for the curve
       
    public:
        BezierSpline()
        {
            p1_points_.reserve(NOM_SIZE);
            p2_points_.reserve(NOM_SIZE);
        }

        // Inherited Functions //
        std::tuple<Vector3d,Vector3d,Vector3d,double>  Evaluate(int seg, double t) ;
        void ResetDerived() override;
        bool ComputeSpline() override;
        void PrintDerivedData() override;
        bool BuildSpline(std::vector<Vector3d> path, int divisions);
};
