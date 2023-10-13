#include "spline.h"

/* Clear out data */
        void SplineCurve::Reset()
        {
            points_.clear();
            pos_profile_.clear();
            vel_profile_.clear();
            accel_profile_.clear();
            curvature_profile_.clear();
            spline_lengths_.clear();
            // ResetDerived();
        }

        void  SplineCurve::AddPoint(const Vector3d& pt)
        {
            // if new point is colinear with previous pts remove the last pt
            // and replace it with this one 
            if(elim_colinear_pts_ && points_.size()>2)
            {
                int n = points_.size()-1;
                Vector3d p0 = points_[n-1]-points_[n-2];
                Vector3d p1 = points_[n]-points_[n-1];
                Vector3d p2 = pt-points_[n];
                // test for colinearity by comparing slopes
                // of two lines. If slopes are equivalent assume colinearity
                double delta = (p2.y()-p1.y())*(p1.x()-p0.x())-(p1.y()-p0.y())*(p2.x()-p1.x());
                if(std::abs(delta) < 0.001)
                {
                    std::cout<< "Point " << points_[n] << "was removed with delta:" << delta << std::endl;
                    points_.pop_back();
                }            
            }
            points_.push_back(pt);

        }

        void  SplineCurve::PrintData(int segments=5)
        {
            assert(segments>1);
            std::cout << "Original Points (" << points_.size() << ")" << std::endl;
            std::cout << "-----------------------------" << std::endl;
            for(int idx = 0; idx < points_.size(); ++idx)
            {
                std::cout << "[" << idx << "]" << "  " << points_[idx] << std::endl;
            }

            std::cout << "-----------------------------" << std::endl;
            PrintDerivedData();

            std::cout << "-----------------------------" << std::endl;
            std::cout << "Evaluating Spline at " << segments << " points." << std::endl;
            for(int idx = 0; idx < points_.size(); idx++)
            {
                std::cout << "---------- " << "From " <<  points_[idx] << " to " << points_[idx+1] << "." << std::endl;
                for(int tIdx = 0; tIdx < segments+1; ++tIdx)
                {
                    double t = tIdx*1.0/segments;
                    std::tuple<Vector3d, Vector3d, Vector3d, double> state_info = EvaluateSplineAtTime(t);
                    std::cout << "[" << tIdx << "]" << "   ";
                    std::cout << "[" << t*100 << "%]" << "   ";
                    std::cout << " --> " << std::get<0>(state_info);
                    std::cout << std::endl;
                }
            }
        }

        
