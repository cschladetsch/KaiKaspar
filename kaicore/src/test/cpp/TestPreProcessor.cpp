#include <gtest/gtest.h>
#include "../../main/cpp/PreProcessor.h"
#include <opencv2/opencv.hpp>

using namespace kaspar;

TEST(PreProcessorTest, SharpnessMetric) {
    PreProcessor processor;
    
    // Create a blurry image (constant color)
    cv::Mat blurry = cv::Mat::zeros(100, 100, CV_8UC1);
    double blurry_sharpness = processor.calculate_sharpness(blurry);
    
    // Create a sharp image (checkerboard)
    cv::Mat sharp = cv::Mat::zeros(100, 100, CV_8UC1);
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            if ((i / 10 + j / 10) % 2 == 0) sharp.at<uchar>(i, j) = 255;
        }
    }
    double sharp_sharpness = processor.calculate_sharpness(sharp);
    
    EXPECT_LT(blurry_sharpness, sharp_sharpness);
}

TEST(PreProcessorTest, ProcessGating) {
    PreProcessor::Config config;
    config.sharpness_threshold = 50.0;
    PreProcessor processor(config);
    
    cv::Mat blurry = cv::Mat::zeros(100, 100, CV_8UC3);
    cv::Mat output;
    
    // Should fail due to sharpness
    EXPECT_FALSE(processor.process(blurry, output));
    
    // Create a sharp image
    cv::Mat sharp = cv::Mat::zeros(100, 100, CV_8UC3);
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            if ((i / 10 + j / 10) % 2 == 0) sharp.at<cv::Vec3b>(i, j) = cv::Vec3b(255, 255, 255);
        }
    }
    
    EXPECT_TRUE(processor.process(sharp, output));
}
