#include "cubic_spline.h"

CubicSpline::CubicSpline() 
    : SplineCurve()
{
}
CubicSpline::CubicSpline(std::vector<Vector3d> points) 
    : SplineCurve(points)
{ 
}
CubicSpline::CubicSpline(std::vector<Vector3d> points, int segments_per_pt) 
    : SplineCurve(points, segments_per_pt)
{
}

std::tuple<Vector3d,Vector3d,Vector3d,double>  CubicSpline::EvaluateSplineAtTime(double t)
{
    const std::vector<Vector3d>& points = GetPoints();

    if (t > GetPoints().size())
        t = GetPoints().size();

    if (t == GetPoints().size())
        t = GetPoints().size() - 0.0000f;

    int segment = (int)t;
    t = t - segment;

    segment = segment % (GetPoints().size()-1);

    Vector3d spline_positions = x_col_[segment][0] + x_col_[segment][1]*t + x_col_[segment][2]*t*t + x_col_[segment][3]*t*t*t;
    Vector3d spline_velocity = x_col_[segment][1] + x_col_[segment][2]*2*t + x_col_[segment][3]*3*t*t;
    Vector3d spline_acceleration = x_col_[segment][2]*2 + x_col_[segment][3]*6*t;
   
    double   spline_curvature = (spline_velocity.x()*spline_acceleration.y())-(spline_velocity.y()*spline_acceleration.x())/
                                pow((spline_velocity.x()*spline_velocity.x()+spline_velocity.y()*spline_velocity.y()),(3/2));
    
    return std::make_tuple( spline_positions, 
                            spline_velocity,
                            spline_acceleration,
                            spline_curvature );
}

void CubicSpline::ResetDerived()
{
    for(int i=0;i<NOM_SIZE;i++)
    {
        for(int j=0;j<4;j++)
        {
            x_col_[i][j] = Vector3d(0.0,0.0,0.0);
        }
        spline_lengths_[i] = 0.0;
    }
}

bool CubicSpline::ComputeSpline()
{
    const std::vector<Vector3d> p = GetPoints();

    int n = p.size()-1;

    Vector3d a[p.size()];
    for (int i = 1; i <= n-1; i++)
        a[i] = 3*((p[i+1] - 2*p[i] + p[i-1]));

    float l[p.size()];
    float mu[p.size()];
    Vector3d z[p.size()];

    l[0] = l[n] = 1;
    mu[0] = 0;
    z[0] = z[n] = Vector3d(0.0, 0.0, 0.0);
    x_col_[n][2] = Vector3d(0.0, 0.0, 0.0);

    for (int i = 1; i <= n-1; i++)
    {
        l[i] = 4 - mu[i-1];
        mu[i] = 1 / l[i];
        z[i] = (a[i] - z[i-1]) / l[i];
    }

    for (int i = 0; i < p.size(); i++)
        x_col_[i][0] = p[i];    // set all the "a" variables to the initial position
     

    for (int j = n-1; j >= 0; j--)
    {
        x_col_[j][2] = z[j] - mu[j] * x_col_[j+1][2];
        x_col_[j][3] = (1.0f / 3.0f)*(x_col_[j+1][2] - x_col_[j][2]);
        x_col_[j][1] = p[j + 1] - p[j] - x_col_[j][2] - x_col_[j][3];
    }
    
    // Solved for the x column coefficients (a,b,c,d in polynomial)
    return true;
}

double CubicSpline::SplineArcLengthIntegrand(int spline, double t)
{
    Vector3d dv = x_col_[spline][1] + 2*x_col_[spline][2]*t + 3*x_col_[spline][3]*t*t;
    return sqrt(dv.x()*dv.x() + dv.y()*dv.y() + dv.z()*dv.z());
}

double CubicSpline::ArcLengthIntegrand(double t)
{
    if (t < 0)
        return SplineArcLengthIntegrand(0, t);

    if (t >= GetPoints().size()-1)
        return SplineArcLengthIntegrand(GetPoints().size()-1, fmod(t, 1));

    return SplineArcLengthIntegrand((int)t, t - (int)t);
}

// Composite Simpson's Rule, Burden & Faires - Numerical Analysis 9th, algorithm 4.1
// Integrate spline section between t=[0,1]
double CubicSpline::IntegrateSpline(int spline, double t)
{

    assert(t >= -1);
    assert(t <= 2);
    int n = GetPoints().size();
    double h = t / n;
    double XI0 = SplineArcLengthIntegrand(spline, t);
    double XI1 = 0;
    double XI2 = 0;

    for (int i = 0; i < n; i++)
    {
        double X = i*h;
        if (i % 2 == 0)
            XI2 += SplineArcLengthIntegrand(spline, X);
        else
            XI1 += SplineArcLengthIntegrand(spline, X);
    }
    double XI = h*(XI0 + 2*XI2 + 4*XI1) * (1.0f/3);
    return XI;
}

// Using Simpsons rule
double CubicSpline::IntegrateCurve(float t0, float t1)
{
    float multiplier = 1;
    if (t0 > t1)
    {
        std::swap(t0, t1);
        multiplier = -1;
    }

    int first_spline = (int)t0;
    if (first_spline < 0)
        first_spline = 0;

    int last_spline = (int)t1;
    if (last_spline >= GetPoints().size()-1)
        last_spline = GetPoints().size()-2;

    if (first_spline == last_spline)
        return (IntegrateSpline(last_spline, t1 - last_spline) - IntegrateSpline(first_spline, t0 - first_spline)) * multiplier;

    float sum = spline_lengths_[first_spline] - IntegrateSpline(first_spline, t0);

    for (int k = first_spline+1; k < last_spline; k++)
        sum += spline_lengths_[k];

    sum += IntegrateSpline(last_spline, t1 - last_spline);

    return sum * multiplier;
}

// Using a concatinated Simpsons Rule
double CubicSpline::EvaluateCurveLength()
{
    float sum = 0;

    for (int i = 0; i < GetPoints().size(); i++)
        sum += spline_lengths_[i];

    return sum;
}

Vector3d CubicSpline::ConstVelocitySplineAtTime(double t, double speed)
{
    float total_length = EvaluateCurveLength();
    float desired_distance = fmod(t * speed, total_length);

    float t_last = NOM_SIZE * desired_distance / total_length;
    float t_next = t_last;

    auto g = [this, desired_distance](float t) -> float {
        return IntegrateCurve(0, t) - desired_distance;
    };

    auto L = [this](float t) -> float {
        return ArcLengthIntegrand(t);
    };

    float t_max = NOM_SIZE;
    float t_min = -0.1f;

    while (t_max - t_min > 0.5f)
    {
        float t_mid = (t_max + t_min)/2;
        if (g(t_min) * g(t_mid) < 0)
            t_max = t_mid;
        else
            t_min = t_mid;
    }

    t_next = (t_max + t_min)/2;

    do {
        t_last = t_next;
        t_next = t_last - g(t_last) / L(t_last);
    } while(fabs(t_last - t_next) > 0.001f);

    // Because of root finding it may be slightly negative sometimes.
    // VAssert(t_next >= -0.1f && t_next <= 999999);

    // return EvaluateSplineAtTime(t_next);
    return Vector3d(0.0,0.0,0.0);
}

void CubicSpline::PrintDerivedData()
{
    std::cout << " Control Points " << std::endl;
}

bool CubicSpline::BuildSpline()
{
    assert(GetPoints().size() > 2);
    // Smooth splines
    std::cout << "Computing Spline" << std::endl;
    ComputeSpline();
    // Get the number of segments in the spline
    double spline_segments = (GetPoints().size()-1)*segments_per_pt_;
    // Loop through all segments and create the overall spline

    std::cout << "Generating Spline Properties" << std::endl;
    for(int t = 0; t < spline_segments; t++)
    {
        double t0 = (double)t/segments_per_pt_;
        // double t1 = (double)(t+1)/segments_per_pt_; next point
        std::tuple<Vector3d, Vector3d, Vector3d, double> state_info = EvaluateSplineAtTime(t0);
        pos_profile_.push_back(std::get<0>(state_info));
        vel_profile_.push_back(std::get<1>(state_info));
        accel_profile_.push_back(std::get<2>(state_info));
        curvature_profile_.push_back(std::get<3>(state_info));
    }
    std::tuple<Vector3d, Vector3d, Vector3d, double> state_info = std::make_tuple( 
                                                                    GetPoints()[GetPoints().size()-1], 
                                                                    Vector3d(0.0,0.0,0.0),
                                                                    Vector3d(0.0,0.0,0.0),
                                                                    0.0
                                                                );
    pos_profile_.push_back(std::get<0>(state_info));
    vel_profile_.push_back(std::get<1>(state_info));
    accel_profile_.push_back(std::get<2>(state_info));
    curvature_profile_.push_back(std::get<3>(state_info));

    for(int i=0;i<GetPoints().size()-1;i++)
    {
        spline_lengths_.push_back(IntegrateSpline(i, 1));
    }

    return true;
}

bool CubicSpline::BuildSpline(std::vector<Vector3d> points)
{
    assert(points.size() > 2);

    Reset();
    for(int i = 0; i<points.size(); i++)
    {
        AddPoint(points[i]);
    }
    
    BuildSpline();

    return true;
}


void PrintDerivedData()
{

}
