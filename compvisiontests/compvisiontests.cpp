// compvisionhw.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>

typedef std::vector<cv::DMatch> MatchNeighbours;
typedef std::vector<MatchNeighbours> Matches;
typedef std::vector<cv::KeyPoint> KeyPoints;

const int NUM_NEAREST_NEIGHBOURS = 2;
const double THRESHOLD_DISTANCE = 0.75;
const int MAX_MATCHES = 20;


void AddPatchedTextToImage(cv::Mat& image, std::string text)
{
	using namespace cv;

	int baseline = 0;
	Size text_size = getTextSize(text, cv::FONT_HERSHEY_PLAIN, 1, 1, &baseline);
	rectangle(image, Rect(image.cols - text_size.width - 20, 0, text_size.width + 20, text_size.height * 2), cv::Scalar(0, 0, 0, 0), FILLED);
	putText(image, text, Point(image.cols - text_size.width - 5, text_size.height + baseline), FONT_HERSHEY_PLAIN, 1, Scalar(255, 255, 255, 0));
}


void WriteKeyPointOutput(std::string output_name, 
						 cv::InputArray image, 
						 const KeyPoints& keyPoints, 
						 bool showImage = false)
{
	cv::Mat output;
	cv::drawKeypoints(image, keyPoints, output, cv::Scalar_<double>::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

	std::string num_features = "Features Found: " + std::to_string(keyPoints.size());
	AddPatchedTextToImage(output, num_features);
	

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
						const MatchNeighbours& filteredMatches, 
						unsigned int goodMatchesFound,
						unsigned int totalMatchesFound,
						std::vector<cv::Point2f> boxInScene, 
						cv::Mat MatchMask,
						bool showImage = false)
{
	MatchNeighbours displayedMatches;
	for (unsigned int matchIndex = 0; matchIndex < filteredMatches.size() && displayedMatches.size() < MAX_MATCHES; matchIndex++)
	{
		unsigned int element = (unsigned int)MatchMask.at<unsigned char>(matchIndex, 0);
		if (1 == element)
		{
			displayedMatches.push_back(filteredMatches[matchIndex]);
		}
	}

	cv::Mat output;
	cv::drawMatches(sourceImage, sourceKeypoints, destinationImage, destinationKeypoints, displayedMatches, output);
	std::string num_matches = "Good Matches Found: " + std::to_string(goodMatchesFound) + " / " + std::to_string(totalMatchesFound);
	AddPatchedTextToImage(output, num_matches);

	std::vector<cv::Point2i> boxInSceneOffset;
	for (unsigned int i = 0; i < boxInScene.size(); i++)
	{
		boxInSceneOffset.push_back(cv::Point2i((int)(boxInScene[i].x + (float)sourceImage.getMat().cols), (int)boxInScene[i].y));
	}
	cv::polylines(output, boxInSceneOffset, true, cv::Scalar(0, 0, 255, 0), 2);

	cv::imwrite("Output\\" + output_name + ".jpg", output);

	if (showImage)
	{
		cv::imshow(output_name, output);
	}
}


void Process(std::string object_name, std::string scene_name, bool show_result = false)
{
	using namespace cv;

	const Mat object = imread("Data\\src\\" + object_name + ".jpg");
	const Mat scene = imread("Data\\dst\\" + scene_name + ".jpg");


	// Fetch feature points
	Ptr<xfeatures2d::SIFT> detector = xfeatures2d::SIFT::create();

	KeyPoints object_keypoints;
	Mat object_descriptors;
	detector->detectAndCompute(object, noArray(), object_keypoints, object_descriptors);

	KeyPoints scene_keypoints;
	Mat scene_descriptors;
	detector->detectAndCompute(scene, noArray(), scene_keypoints, scene_descriptors);

	// Get output
	WriteKeyPointOutput(object_name, object, object_keypoints, show_result);
	WriteKeyPointOutput(scene_name, scene, scene_keypoints, show_result);


	// DO BRUTE FORCE MATCHING
	// Match points in destination
	Ptr<BFMatcher> matcher = BFMatcher::create();

	Matches matches;
	matcher->knnMatch(object_descriptors, scene_descriptors, matches, NUM_NEAREST_NEIGHBOURS);

	// Filter the matches
	MatchNeighbours good_matches;
	for (Matches::iterator matchItr = matches.begin(); matchItr != matches.end(); matchItr++)
	{
		MatchNeighbours neighbours = *matchItr;

		// 0 is the closest neigbour to match_matrix[match_id]'s query idx
		// Retain the match only if it is closer by 60% of the next nearest neighbour.
		if (neighbours.size() > 0 && neighbours.size() <= NUM_NEAREST_NEIGHBOURS)
		{
			if (neighbours[0].distance < THRESHOLD_DISTANCE * neighbours[1].distance)
			{
				good_matches.push_back(neighbours[0]);
			}
		}
	}
	
	// Sort the matches based on distance
	std::sort(good_matches.begin(), good_matches.end(), 
	[](DMatch match1, DMatch match2)
	{
		return match1.distance < match2.distance;
	});
	

	// PERFORM HOMOGRAPHY
	// Separate out the object and Scene points from good matches
	std::vector<Point2f> good_object_points, good_scene_points;
	for (MatchNeighbours::iterator neighbourItr = good_matches.begin(); neighbourItr != good_matches.end(); neighbourItr++)
	{
		DMatch hoodMatch = *neighbourItr;
		good_object_points.push_back(object_keypoints[hoodMatch.queryIdx].pt);
		good_scene_points.push_back(scene_keypoints[hoodMatch.trainIdx].pt);
	}

	// Calculate homography
	Mat homographyMask;
	Mat homography = findHomography(good_object_points, good_scene_points, homographyMask, RANSAC, 3.0f);
	std::cout << object_name << " vs " << scene_name << std::endl << homography << std::endl << std::endl;
	//std::cout << "Mask: " << std::endl << homographyMask.size() << " vs " << good_object_points.size() << " and " << good_scene_points.size() << std::endl << std::endl << std::endl;

	// Compute Inliers
	unsigned int numInliers = (int)sum(homographyMask)[0];
	unsigned int numOutliers = good_object_points.size() - numInliers;

	std::cout << "Homography Stats: \n\t Inliers: " << numInliers << "\n\t Outliers: " << numOutliers << std::endl << std::endl;

	// Compute the box using homography
	std::vector<Point2f> object_box_outline, box_outline_in_scene;
	object_box_outline.push_back(Point2f(0.0f, 0.0f));
	object_box_outline.push_back(Point2f(0.0f, (float)object.rows));
	object_box_outline.push_back(Point2f((float)object.cols, (float)object.rows));
	object_box_outline.push_back(Point2f((float)object.cols, 0.0f));
	
	perspectiveTransform(object_box_outline, box_outline_in_scene, homography);


	// Draw Output
	WriteMatchesOutput(object_name + "_" + scene_name, 
		object, object_keypoints, 
		scene, scene_keypoints, 
		good_matches, good_matches.size(), matches.size(), 
		box_outline_in_scene, homographyMask,
		show_result);


	if (show_result)
	{
		waitKey();
		destroyAllWindows();
	}	
}


int main(int argc, const char* argv[])
{
	for (auto& object_filename : std::filesystem::directory_iterator("Data\\src"))
	{
		for (auto& scene_filename : std::filesystem::directory_iterator("Data\\dst"))
		{
			Process(object_filename.path().filename().stem().string(), scene_filename.path().filename().stem().string(), true);
		}		
	}

	return 0;
}