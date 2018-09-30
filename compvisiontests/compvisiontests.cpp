// compvisionhw.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <string>
#include <filesystem>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>

typedef std::vector<cv::DMatch> MatchNeighbours;
typedef std::vector<MatchNeighbours> Matches;
typedef std::vector<cv::KeyPoint> KeyPoints;

const int NUM_NEAREST_NEIGHBOURS = 5;
const double THRESHOLD_DISTANCE = 0.6;


void WriteKeyPointOutput(std::string output_name, 
						 cv::InputArray image, 
						 const KeyPoints& keyPoints, 
						 bool showImage = false)
{
	cv::Mat output;
	cv::drawKeypoints(image, keyPoints, output, cv::Scalar_<double>::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

	std::string num_features = "Features Found: " + std::to_string(keyPoints.size());
	cv::rectangle(output, cv::Rect(output.cols - 220, 0, 220, 30), cv::Scalar(0, 0, 0, 0), cv::FILLED);
	cv::putText(output, num_features, cv::Point(output.cols - 200, 20), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(255, 255, 255, 0));

	cv::imwrite("Output\\" + output_name + ".jpg", output);	
	

	if (showImage)
	{
		cv::imshow(output_name, output);
	}
}


void WriteMatchesOutput(std::string output_name, 
						cv::InputArray sourceImage, 
						const KeyPoints& sourceKeypoints, 
						cv::InputArray destinationImage, 
						const KeyPoints& destinationKeypoints, 
						const MatchNeighbours& matches, 
						bool showImage = false)
{
	cv::Mat output;
	cv::drawMatches(sourceImage, sourceKeypoints, destinationImage, destinationKeypoints, matches, output);
	cv::imwrite("Output\\" + output_name + ".jpg", output);

	if (showImage)
	{
		cv::imshow(output_name, output);
	}
}


void Process(std::string object_name, std::string scene_name, bool show_result = false)
{
	const cv::Mat object = cv::imread("Data\\src\\" + object_name + ".jpg");
	const cv::Mat scene = cv::imread("Data\\dst\\" + scene_name + ".jpg");


	// Fetch feature points
	cv::Ptr<cv::xfeatures2d::SIFT> detector = cv::xfeatures2d::SIFT::create();

	std::vector<cv::KeyPoint> object_keypoints;
	cv::Mat object_descriptors;
	detector->detectAndCompute(object, cv::noArray(), object_keypoints, object_descriptors);

	std::vector<cv::KeyPoint> scene_keypoints;
	cv::Mat scene_descriptors;
	detector->detectAndCompute(scene, cv::noArray(), scene_keypoints, scene_descriptors);


	// Match points in destination
	cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create();

	Matches matches;
	matcher->knnMatch(object_descriptors, scene_descriptors, matches, NUM_NEAREST_NEIGHBOURS);


	// Filter the matches
	MatchNeighbours good_matches;
	for (Matches::iterator neighbourItr = matches.begin(); neighbourItr != matches.end(); neighbourItr++)
	{
		MatchNeighbours neighbour = *neighbourItr;

		// 0 is the closest neigbour to match_matrix[match_id]'s query idx
		// Retain the match only if it is closer by 60% of the next nearest neighbour.
		if (neighbour.size() > 0 && neighbour.size() <= NUM_NEAREST_NEIGHBOURS)
		{
			if (neighbour[0].distance < THRESHOLD_DISTANCE * neighbour[1].distance)
			{
				good_matches.push_back(neighbour[0]);
			}
		}
	}

	// Get output
	WriteKeyPointOutput(object_name, object, object_keypoints, show_result);
	WriteKeyPointOutput(scene_name, scene, scene_keypoints, show_result);
	WriteMatchesOutput(object_name + "_" + scene_name, object, object_keypoints, scene, scene_keypoints, good_matches, show_result);

	if (show_result)
	{
		cv::waitKey();
	}	
}


int main(int argc, const char* argv[])
{
	for (auto& object_filename : std::filesystem::directory_iterator("Data\\src"))
		for (auto& scene_filename : std::filesystem::directory_iterator("Data\\dst"))
			Process(object_filename.path().filename().stem().string(), scene_filename.path().filename().stem().string());

	return 0;
}