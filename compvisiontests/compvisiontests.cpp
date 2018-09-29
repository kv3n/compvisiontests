// compvisionhw.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>

typedef std::vector<std::vector<cv::DMatch>> Matches;

int main(int argc, const char* argv[])
{
	const cv::Mat source = cv::imread("Data\\src_1.jpg"); // Query Image
	const cv::Mat destination = cv::imread("Data\\dst_1.jpg"); // Train Image
	

	// Fetch feature points
	cv::Ptr<cv::xfeatures2d::SIFT> detector = cv::xfeatures2d::SIFT::create();

	std::vector<cv::KeyPoint> source_keypoints;
	cv::Mat source_descriptors;
	detector->detectAndCompute(source, cv::noArray(), source_keypoints, source_descriptors);
	
	std::vector<cv::KeyPoint> destination_keypoints;
	cv::Mat destination_descriptors;
	detector->detectAndCompute(destination, cv::noArray(), destination_keypoints, destination_descriptors);

	
	// Match points in destination
	cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create();

	Matches matches;
	matcher->knnMatch(source_descriptors, destination_descriptors, matches, 3);

	
	// Get output
	cv::Mat output_src_keypoints_only;
	cv::drawKeypoints(source, source_keypoints, output_src_keypoints_only, cv::Scalar_<double>::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

	cv::Mat output;
	cv::drawMatches(source, source_keypoints, destination, destination_keypoints, matches, output);
	

	cv::imshow("Test", output);
	cv::waitKey();


	// Unload Resources
	delete detector;
	delete matcher;

	return 0;
}