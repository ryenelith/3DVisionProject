#ifndef TOOLS_H
#define TOOLS_H

#include <vector>
#include <iostream>
#include <Eigen/Dense>
#include <fstream>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/nonfree/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "CImg.h"
#include "camera.h"
#include "inputManager.h"
//#include "latticeStruct.h"

using namespace std;
using namespace cimg_library;

class snake
{
private:
    vector< Eigen::Vector2f > points;
    int size;
    float gamma;
    float alpha;
    float beta;
    bool end;
    CImgList<float> extGrad;
    CImg<unsigned char> image;
    void preProcess();
public:
    snake()
    {
        size = 0;
        alpha =0; gamma=0; beta=0;
        end = false;
    }
    void assign(vector<vector<int> > ps, CImg<unsigned char> &img);
    void setParams(float _gamma, float _alpha, float _beta);
    void update();
    void display(CImg<unsigned char> &img, CImgDisplay &disp, const unsigned char *const colorPts, const unsigned char *const colorLines);
    bool stop();
};


template<typename T1> T1 median(vector<T1> &v)
{
    size_t n = v.size() / 2;
    nth_element(v.begin(), v.begin()+n, v.end());
    return v[n];
}

/*!
	 * Calculates the SIFT descriptor of a 2d point
	 *
	 * @param[in] imagename name of the image. Must be the path after the "data/" directory
	 * @param[in] pos 2d position of the point
	 * @param[out] outSingleFeatureVector the array to store the computed SIFT descriptor
	 * @return	a dummy integer value
	 */
inline int computeSIFT(string imagename, Eigen::Vector2d pos, Eigen::VectorXd &outSingleFeatureVector)
{
    cv::Mat B;
    float x = pos(0);
    float y = pos(1);

    const cv::Mat input = cv::imread("data/"+imagename, 0); //Load as grayscale

    if(! input.data )                              // Check for invalid input
        {
            cout <<  "Could not open or find the image" << std::endl ;
            return -1;
        }
    assert(x>0 && y>0 && x< input.cols && y < input.rows);

    // create mask to search for nearby keypoints to use their size for our keypoint
    cv::Mat mask;
    input.copyTo(mask);
    mask = cv::Scalar(0);
 
    int KPsize = 9;

    // compute descriptor of desired keypoint location using calculated size
    cv::SIFT siftDetector;
    cv::KeyPoint myKeypoint(x, y, KPsize);
    vector<cv::KeyPoint> myKeyPoints;
    myKeyPoints.push_back(myKeypoint);
    cv::Mat descriptor;

    siftDetector(input,mask,myKeyPoints,descriptor, true);

    // convert descriptor to Eigen::VectorXf (column vector)
    Eigen::VectorXd outDescriptor(descriptor.cols,1);
    for(int i = 0; i<descriptor.cols;i++)
    {
        outDescriptor(i) = descriptor.at<float>(0,i);
    }

    // pass result to specified Eigen::VectorXf
    outSingleFeatureVector = outDescriptor;

    return 0;
}


/*!
	 * Checks whether the reference and the pointToTest have similar SIFT descriptors for their most frontoparallel view. Called from latticeDetector.
	 *
	 * @param[in] referencePoint the first point to check (in 3D)
	 * @param[in] pointToTest the second point to check (in 3D)
	 * @param[in] plane the array to store the computed SIFT descriptor
	 * @param[in] K intrinsic camera matrix (to calculate projections)
	 * @param[in] camPoses all the poses of the cameras in the dataset
	 * @param[in] viewIds the corresponding index to the image directory
	 * @param[in] imageNames an array containing the image names
	 * @return	true if the points have similar SIFT
	 */
inline bool compareSiftFronto(Eigen::Vector3d const &referencePoint, Eigen::Vector3d const &pointToTest,
		Eigen::Vector4d plane,
		Eigen::Matrix3d K, vector<Eigen::Matrix<double,3,4>> camPoses, vector<int> viewIds,  vector<string> imageNames ){

	//TODO: We use fixed image size (1696x1132). Must read img to check real...
	float const w = 1696;
	float const h = 1132;

	CameraMatrix cam;
	cam.setIntrinsic(K);

	double cosangle = 0;
	double tmpcosangle=0;
	int bestview=-1;
	Vector2d pbest(0,0);
	Vector2d p;

	//TODO: pose selection only for the three images that the lattices were extracted.
	//Proposed paper method is weak.
	for (size_t i=0; i<camPoses.size(); i++){

	//get view
		int view = viewIds[i];

		if ((view < 45) || (view > 47))
		{
			continue;
		}

	//check angle between camera-point line and plane normal
		Vector3d line = referencePoint - camPoses[i].block<3,1>(0,3);
	//abs because we dont know the plane orientation
		tmpcosangle = abs(line.dot(plane.head(3)))/sqrt(line.squaredNorm() * plane.head(3).squaredNorm());

	//project point into image
		cam.setOrientation(camPoses[i]);
		p = cam.projectPoint(referencePoint);

		double d = cam.transformPointIntoCameraSpace(referencePoint)[2];

		if ((d > 0) && (tmpcosangle > cosangle) && (p[0]>=0)&&(p[1]>=0)&& (p[0]<w)&&(p[1]<h)){
			cosangle = tmpcosangle;
			bestview = view;
			pbest = p;			
		}
	}


	Eigen::VectorXd s1;
	computeSIFT(imageNames[bestview],pbest,s1);

	//==========================

	cosangle = 0;
	pbest[0]=-1;
	pbest[1]=-1;
	bestview = -1;

	for (size_t i=0; i<camPoses.size(); i++){

		int view = viewIds[i];

		if ((view < 45) || (view > 47))
				{
					continue;
				}

	//check angle between camera-point line and plane normal
		Vector3d line = pointToTest - camPoses[i].block<3,1>(0,3);
	//abs because we dont know the plane orientation
		tmpcosangle = abs(line.dot(plane.head(3)))/(line.norm()*plane.head(3).norm());

	//project point into image
		cam.setOrientation(camPoses[i]);
		p = cam.projectPoint(pointToTest);
		double d = cam.transformPointIntoCameraSpace(pointToTest)[2];

		if ((d > 0)&&(tmpcosangle > cosangle) && (p[0]>=0)&&(p[1]>=0)&& (p[0]<w)&&(p[1]<h)){
			cosangle = tmpcosangle;
			bestview = view;
			pbest = p;			
		}
	}

	if (bestview == -1){
		return false;
	}

	Eigen::VectorXd s2;
	computeSIFT(imageNames[bestview],pbest,s2);

	double dotProduct = s1.dot(s2);

	 //angle (in radians)
	 double theta = acos(dotProduct/(s1.norm()*s2.norm()));


	 if ((theta) < 2*0.25) // 2*tol_angle from detectRepPoints. This threshold is defined by the paper.
	 	 return true;
	 else
		 return false;

}
#endif // TOOLs_H
