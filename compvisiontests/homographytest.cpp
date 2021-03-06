// compvisionhw.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "common.h"
#include "homographytest.h"


void ProcessForHomography(std::string object_name, std::string scene_name, bool show_result)
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
	WriteKeyPointOutput("homography", object_name, object, object_keypoints, show_result);
	WriteKeyPointOutput("homography", scene_name, scene, scene_keypoints, show_result);


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
	Mat homography = findHomography(good_object_points, good_scene_points, homographyMask, RANSAC, 0.0f);
	std::cout << object_name << " vs " << scene_name << std::endl << homography << std::endl << std::endl;

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
	WriteMatchesOutput("homography", object_name + "_" + scene_name, 
		object, object_keypoints, 
		scene, scene_keypoints, 
		good_matches, good_matches.size(), matches.size(), 
		box_outline_in_scene, homographyMask,
		show_result);
}