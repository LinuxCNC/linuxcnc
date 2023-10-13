
#include "bezier_spline.h"
//http://www2.informatik.uni-freiburg.de/~lau/students/Sprunk2008.pdf
std::tuple<Vector3d,Vector3d,Vector3d,double> BezierSpline::Evaluate(int seg, double t)
{
    double omt = 1.0 - t;

    // Based on the two knots (setpoints) and 2 control points,
    // create a spline for a given segment
    Vector3d p0(GetPoints()[seg]);
    Vector3d p1(p1_points_[seg]);
    Vector3d p2(p2_points_[seg]);
    Vector3d p3(GetPoints()[seg+1]);

    /* Compute the bezier curve in 3-Space */
    Vector3d spline_positions = omt*omt*omt*p0 + 3*omt*omt*t*p1 +3*omt*t*t*p2+t*t*t*p3;
    Vector3d spline_velocity = 3*omt*omt*(p1-p0)+6*t*omt*(p2-p1)+3*t*t*(p3-p2);
    Vector3d spline_acceleration = 6*omt*(p2-2*p1+p0)+6*t*(p3-2*p2+p1);
    double   spline_curvature = (spline_velocity.x()*spline_acceleration.y())-(spline_velocity.y()*spline_acceleration.x())/
                                pow((spline_velocity.x()*spline_velocity.x()+spline_velocity.y()*spline_velocity.y()),(3/2));
    return std::make_tuple( spline_positions, 
                            spline_velocity,
                            spline_acceleration,
                            spline_curvature );
}

void BezierSpline::ResetDerived()
{
    p1_points_.clear();
    p2_points_.clear();
}

/*computes control points given knots K. */
bool BezierSpline::ComputeSpline()
{
    const std::vector<Vector3d>& k = GetPoints();  

    int N = (int)k.size()-1;
    p1_points_.resize(N);
    p2_points_.resize(N);
    if(N == 0)
        return false;

    if(N == 1)
    {  // Only 2 points...just create a straight line.
        // Constraint:  3*P1 = 2*P0 + P3
        p1_points_[0] = (2.0/3.0*k[0] + 1.0/3.0*k[1]);
        // Constraint:  P2 = 2*P1 - P0
        p2_points_[0] = 2.0*p1_points_[0] - k[0];
        curvature_profile_.push_back(0.0);
        return true;
    }

    /*rhs vector*/
    std::vector<Vector3d> a(N);
    std::vector<Vector3d> b(N);
    std::vector<Vector3d> c(N);
    std::vector<Vector3d> r(N);

    /*left most segment*/
    a[0].x() = 0;   b[0].x() = 2;   c[0].x() = 1;   r[0].x() = k[0].x()+2*k[1].x();// outside matrix
    a[0].y() = 0;   b[0].y() = 2;   c[0].y() = 1;   r[0].y() = k[0].y()+2*k[1].y();
    // a[0].z() = 0;   b[0].z() = 2;   c[0].z() = 1;   r[0].z() = k[0].z()+2*k[1].z();

    /*internal segments*/
    for (int i = 1; i < N - 1; i++)
    {
        a[i].x() = 1; b[i].x() = 4; c[i].x() = 1; r[i].x() = 4*k[i].x()+2*k[i+1].x();
        a[i].y() = 1; b[i].y() = 4; c[i].y() = 1; r[i].y() = 4*k[i].y()+2*k[i+1].y();
        // a[i].z() = 1; b[i].z() = 4; c[i].z() = 1; r[i].z() = 4*k[i].x()+2*k[i+1].x();   // TODO: Fix the r implementation to depend on xy
    }

    /*right segment*/
    a[N-1].x() = 2; b[N-1].x() = 7; c[N-1].x() = 0; r[N-1].x() = 8*k[N-1].x()+k[N].x();
    a[N-1].y() = 2; b[N-1].y() = 7; c[N-1].y() = 0; r[N-1].y() = 8*k[N-1].y()+k[N].y();
    // a[N-1].z() = 2; b[N-1].z() = 7; c[N-1].z() = 0; r[N-1].z() = 8*k[N-1].z()+k[N].z();


    /*solves Ax=b with the Thomas algorithm (from Wikipedia)*/
    for (int i = 1; i < N; i++)
    {
        double m;

        m = a[i].x()/b[i-1].x();
        b[i].x() = b[i].x()-m*c[i-1].x();
        r[i].x() = r[i].x()-m*r[i-1].x();

        m = a[i].y()/b[i-1].y();
        b[i].y() = b[i].y()-m*c[i-1].y();
        r[i].y() = r[i].y()-m*r[i-1].y();
    }

    p1_points_[N-1].x() = r[N-1].x()/b[N-1].x();
    p1_points_[N-1].y() = r[N-1].y()/b[N-1].y();
    for (int i = N - 2; i >= 0; i--)
    {
        p1_points_[i].x() = (r[i].x()-c[i].x()*p1_points_[i+1].x()) /b[i].x();
        p1_points_[i].y() = (r[i].y()-c[i].y()* p1_points_[i+1].y()) /b[i].y();
    }

    /*we have p1, now compute p2*/
    for (int i=0;i<N-1;i++)
    {
        p2_points_[i].x()=2*k[i+1].x()-p1_points_[i+1].x();
        p2_points_[i].y()=2*k[i+1].y()-p1_points_[i+1].y();
    }

    p2_points_[N-1].x() = 0.5 * (k[N].x()+p1_points_[N-1].x());
    p2_points_[N-1].y() = 0.5 * (k[N].y()+p1_points_[N-1].y());

    return true;
}

void BezierSpline::PrintDerivedData()
{
    std::cout << " Control Points " << std::endl;
    for(int idx = 0; idx < p1_points_.size(); idx++)
    {
        std::cout << "[" << idx << "]  ";
        std::cout << "P1: " << p1_points_[idx];
        std::cout << "   ";
        std::cout << "P2: " << p2_points_[idx];
        std::cout << std::endl;
    }
}

bool BezierSpline::BuildSpline(std::vector<Vector3d> setpoints, int divisions)
{
    assert(setpoints.size() >= 2);
    
    
    for(int idx = 0; idx<setpoints.size(); idx++)
    {
        AddPoint(setpoints[idx]);
    }
    pos_profile_.clear();
    // Smooth them.
    ComputeSpline();
    
    for(int idx = 0; idx < GetPoints().size()-1; idx++)
    {
        for(int division = 0; division <= divisions; division++)
        {
            double t = division*1.0/divisions;
            std::tuple<Vector3d, Vector3d, Vector3d, double> state_info = Evaluate(idx, t);
            pos_profile_.push_back(std::get<0>(state_info));    // this is backwards
            vel_profile_.push_back(std::get<1>(state_info));
            accel_profile_.push_back(std::get<2>(state_info));
            curvature_profile_.push_back(std::get<3>(state_info));
        }
    }
    return true;
}
