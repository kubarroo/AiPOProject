#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <iostream>
#include "seam_carver.hpp"

enum class Axis {
    HORIZONTAL,
    VERTICAL
};

class FastCarver {
 public:
  cv::Mat baseImage;
  cv::Mat indexMap;
  Axis currentAxis = Axis::HORIZONTAL;

  void precompute(const cv::Mat& img, SeamCarver& carver, Axis axis) {
    currentAxis = axis;
    
    cv::Mat workImg = img.clone();
    if (axis == Axis::VERTICAL) {
        cv::transpose(workImg, workImg);
    }
    baseImage = workImg.clone();
    
    int H = baseImage.rows;
    int W = baseImage.cols;

    std::vector<std::vector<int>> seams = carver.getRemovedSeams(workImg, W - 1);

    indexMap = cv::Mat(H, W, CV_32S, cv::Scalar(W + 1));

    for (int s = 0; s < (int)seams.size(); ++s) {
        int step = s + 1;
        for (int y = 0; y < H; ++y) {
            int orig_x = seams[s][y];
            indexMap.at<int>(y, orig_x) = step;
        }
    }
  }

  cv::Mat getRealTimeImage(int target_size) {
    if (baseImage.empty() || indexMap.empty()) return cv::Mat();
    int H = baseImage.rows;
    int W = baseImage.cols;

    target_size = std::max(1, target_size);
    cv::Mat result(H, target_size, CV_8UC3);

    if (target_size <= W) {
        int pixels_to_remove = W - target_size;
        #pragma omp parallel for
        for (int y = 0; y < H; ++y) {
          int dest_x = 0;
          for (int x = 0; x < W; ++x) {
            int death_index = indexMap.at<int>(y, x);
            if (death_index > pixels_to_remove) {
              if (dest_x < target_size) {
                if (death_index == pixels_to_remove + 1) {
                  result.at<cv::Vec3b>(y, dest_x) = cv::Vec3b(0, 0, 255);
                } else {
                  result.at<cv::Vec3b>(y, dest_x) = baseImage.at<cv::Vec3b>(y, x);
                }
                dest_x++;
              }
            }
          }
        }
    } else {
        int pixels_to_add = target_size - W;
        std::vector<int> red_seam_x(H, -1);
        
        #pragma omp parallel for
        for (int y = 0; y < H; ++y) {
          int dest_x = 0;
          for (int x = 0; x < W; ++x) {
            if (dest_x >= target_size) break;
            
            int death_index = indexMap.at<int>(y, x);
            if (death_index == pixels_to_add + 1) {
                red_seam_x[y] = dest_x;
            }
            
            result.at<cv::Vec3b>(y, dest_x) = baseImage.at<cv::Vec3b>(y, x);
            dest_x++;
            
            if (death_index <= pixels_to_add && dest_x < target_size) {
                cv::Vec3b p1 = baseImage.at<cv::Vec3b>(y, x);
                cv::Vec3b p2 = (x + 1 < W) ? baseImage.at<cv::Vec3b>(y, x + 1) : p1;
                cv::Vec3b avg;
                avg[0] = (p1[0] + p2[0]) / 2;
                avg[1] = (p1[1] + p2[1]) / 2;
                avg[2] = (p1[2] + p2[2]) / 2;
                result.at<cv::Vec3b>(y, dest_x) = avg;
                dest_x++;
            }
          }
          while (dest_x < target_size) {
              result.at<cv::Vec3b>(y, dest_x) = baseImage.at<cv::Vec3b>(y, W - 1);
              dest_x++;
          }
        }
        
        for (int y = 0; y < H - 1; ++y) {
            int x1 = red_seam_x[y];
            int x2 = red_seam_x[y+1];
            if (x1 != -1 && x2 != -1) {
                cv::line(result, cv::Point(x1, y), cv::Point(x2, y+1), cv::Scalar(0, 0, 255), 1);
            } else if (x1 != -1) {
                result.at<cv::Vec3b>(y, x1) = cv::Vec3b(0, 0, 255);
            }
        }
        if (H > 0 && red_seam_x[H-1] != -1) {
            result.at<cv::Vec3b>(H-1, red_seam_x[H-1]) = cv::Vec3b(0, 0, 255);
        }
    }
    
    if (currentAxis == Axis::VERTICAL) {
        cv::Mat transposed;
        cv::transpose(result, transposed);
        return transposed;
    }
    return result;
  }
};
