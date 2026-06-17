#pragma once

#include <omp.h>
#include <cmath>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <vector>

struct LastCachedResult {
  int width;
  int height;
  cv::Mat result;
};

template <typename T>
int sgn(T val) {
  // Source - https://stackoverflow.com/a/4609795
  // Retrieved 2026-06-17, License - CC BY-SA 4.0
  return (T(0) < val) - (val < T(0));
}

class SeamCarver {
 public:
  SeamCarver() = default;
  ~SeamCarver() = default;

  void setImage(const cv::Mat& mat) {
    baseImage = mat.clone();
    lastCachedResult = {mat.cols, mat.rows, mat.clone()};
  }

  cv::Mat getImage(int targetWidth, int targetHeight) {
    if (baseImage.empty()) {
      return cv::Mat();
    }
    if (isCacheMissInAxis(baseImage.cols, targetWidth,
                          lastCachedResult.width) ||
        isCacheMissInAxis(baseImage.rows, targetHeight,
                          lastCachedResult.height)) {
      std::cerr << "[cache miss]";
      return changeResolutionAndCache(baseImage, targetWidth, targetHeight);
    }
    return changeResolutionAndCache(lastCachedResult.result, targetWidth,
                                    targetHeight);
  }

 private:
  cv::Mat baseImage;
  LastCachedResult lastCachedResult = {0, 0, cv::Mat()};

  bool isCacheMissInAxis(int base, int target, int cached) {
    if (target == cached)
      return false;  // nothing needs to change
    int cached_offset = cached - base;
    int target_offset = target - base;
    if (cached_offset == 0)
      return false;  // don't care, base is same as cached
    if (sgn(cached_offset) != sgn(target_offset))
      return true;  // cached and target are on opposite sides of base, cache
                    // won't help
    if (std::abs(cached_offset) < std::abs(target_offset))
      return false;  // cached is closer to base than target, we can use it
    return true;     // cached is further from base than target, we need to
                     // recreate
  }

  cv::Mat changeResolutionAndCache(cv::Mat img,
                                   int targetWidth,
                                   int targetHeight) {
    if (targetWidth < img.cols)
      img = shrinkWidth(img, targetWidth);
    if (targetHeight < img.rows)
      img = shrinkHeight(img, targetHeight);
    if (targetWidth > img.cols)
      img = expandWidthIncremental(img, targetWidth);
    if (targetHeight > img.rows)
      img = expandHeightIncremental(img, targetHeight);

    lastCachedResult.result = img.clone();
    lastCachedResult.width = img.cols;
    lastCachedResult.height = img.rows;

    return img;
  }

  cv::Mat shrinkHeight(cv::Mat img, int targetHeight) {
    return shrinkWidth(img.t(), targetHeight).t();
  }

  cv::Mat expandHeightIncremental(cv::Mat img, int targetHeight) {
    return expandWidthIncremental(img.t(), targetHeight).t();
  }

  cv::Mat shrinkWidth(cv::Mat img, int targetWidth) {
    int H = img.rows;
    int W = img.cols;
    if (targetWidth >= W)
      return img;

    cv::Mat gray(H, W, CV_8UC1);
    cv::Mat grad_x(H, W, CV_32F);
    cv::Mat grad_y(H, W, CV_32F);
    cv::Mat energy(H, W, CV_32F);
    cv::Mat M(H, W, CV_32F);
    cv::Mat backtrack(H, W, CV_32S);
    std::vector<int> seam(H);

    while (img.cols > targetWidth) {
      int curW = img.cols;

      cv::Mat gray_roi = gray(cv::Rect(0, 0, curW, H));
      cv::Mat grad_x_roi = grad_x(cv::Rect(0, 0, curW, H));
      cv::Mat grad_y_roi = grad_y(cv::Rect(0, 0, curW, H));
      cv::Mat energy_roi = energy(cv::Rect(0, 0, curW, H));
      cv::Mat M_roi = M(cv::Rect(0, 0, curW, H));
      cv::Mat bt_roi = backtrack(cv::Rect(0, 0, curW, H));

      computeEnergy(img, gray_roi, grad_x_roi, grad_y_roi, energy_roi);
      findSingleSeam(energy_roi, M_roi, bt_roi, seam);
      removeSingleSeamInPlace(img, seam);
    }

    return img.clone();
  }

  cv::Mat expandWidthIncremental(cv::Mat img, int targetWidth) {
    int delta = targetWidth - img.cols;
    if (delta <= 0)
      return img;

    int max_expand_per_step = std::max(1, img.cols / 2);
    int step = std::min(delta, max_expand_per_step);

    img = expandImageWidth(img, step);

    if (img.cols < targetWidth) {
      return expandWidthIncremental(img, targetWidth);
    }
    return img;
  }

  void computeEnergy(const cv::Mat& img,
                     cv::Mat& gray,
                     cv::Mat& grad_x,
                     cv::Mat& grad_y,
                     cv::Mat& energy) {
    int H = img.rows;
    int W = img.cols;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
    cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);

#pragma omp parallel for schedule(static)
    for (int i = 0; i < H; ++i) {
      const float* gx = grad_x.ptr<float>(i);
      const float* gy = grad_y.ptr<float>(i);
      float* en = energy.ptr<float>(i);
      for (int j = 0; j < W; ++j) {
        en[j] = std::abs(gx[j]) + std::abs(gy[j]);
      }
    }
  }

  void findSingleSeam(const cv::Mat& energy,
                      cv::Mat& M,
                      cv::Mat& backtrack,
                      std::vector<int>& seam) {
    int H = energy.rows;
    int W = energy.cols;
    energy.row(0).copyTo(M.row(0));

    for (int i = 1; i < H; ++i) {
      const float* prev_M = M.ptr<float>(i - 1);
      const float* curr_E = energy.ptr<float>(i);
      float* curr_M = M.ptr<float>(i);
      int* bt_row = backtrack.ptr<int>(i);

      for (int j = 0; j < W; ++j) {
        float min_val = prev_M[j];
        int idx = j;
        if (j > 0 && prev_M[j - 1] < min_val) {
          min_val = prev_M[j - 1];
          idx = j - 1;
        }
        if (j < W - 1 && prev_M[j + 1] < min_val) {
          min_val = prev_M[j + 1];
          idx = j + 1;
        }
        curr_M[j] = curr_E[j] + min_val;
        bt_row[j] = idx;
      }
    }

    const float* last_row = M.ptr<float>(H - 1);
    int min_idx = 0;
    float min_val = last_row[0];
    for (int j = 1; j < W; ++j) {
      if (last_row[j] < min_val) {
        min_val = last_row[j];
        min_idx = j;
      }
    }

    seam[H - 1] = min_idx;
    for (int i = H - 2; i >= 0; --i) {
      seam[i] = backtrack.ptr<int>(i + 1)[seam[i + 1]];
    }
  }

  void removeSingleSeamInPlace(cv::Mat& img, const std::vector<int>& seam) {
    int H = img.rows;
    int W = img.cols;
    size_t esz = img.elemSize();

#pragma omp parallel for schedule(static)
    for (int i = 0; i < H; ++i) {
      uchar* row_ptr = img.ptr<uchar>(i);
      int col = seam[i];
      if (col < W - 1) {
        std::memmove(row_ptr + col * esz, row_ptr + (col + 1) * esz,
                     (W - 1 - col) * esz);
      }
    }
    img = img(cv::Rect(0, 0, W - 1, H));
  }

  cv::Mat expandImageWidth(const cv::Mat& img, int num_add) {
    int H = img.rows;
    int W = img.cols;
    cv::Mat temp_im = img.clone();
    cv::Mat idxMap(H, W, CV_32S);
    for (int i = 0; i < H; ++i) {
      int* row_ptr = idxMap.ptr<int>(i);
      for (int j = 0; j < W; ++j)
        row_ptr[j] = j;
    }

    std::vector<std::vector<int>> original_seams(num_add, std::vector<int>(H));
    cv::Mat gray(H, W, CV_8UC1);
    cv::Mat grad_x(H, W, CV_32F);
    cv::Mat grad_y(H, W, CV_32F);
    cv::Mat energy(H, W, CV_32F);
    cv::Mat M(H, W, CV_32F);
    cv::Mat backtrack(H, W, CV_32S);
    std::vector<int> seam(H);

    for (int s = 0; s < num_add; ++s) {
      int curW = temp_im.cols;
      cv::Mat gray_roi = gray(cv::Rect(0, 0, curW, H));
      cv::Mat grad_x_roi = grad_x(cv::Rect(0, 0, curW, H));
      cv::Mat grad_y_roi = grad_y(cv::Rect(0, 0, curW, H));
      cv::Mat energy_roi = energy(cv::Rect(0, 0, curW, H));
      cv::Mat M_roi = M(cv::Rect(0, 0, curW, H));
      cv::Mat bt_roi = backtrack(cv::Rect(0, 0, curW, H));

      computeEnergy(temp_im, gray_roi, grad_x_roi, grad_y_roi, energy_roi);
      findSingleSeam(energy_roi, M_roi, bt_roi, seam);

      for (int i = 0; i < H; ++i) {
        original_seams[s][i] = idxMap.ptr<int>(i)[seam[i]];
      }
      removeSingleSeamInPlace(temp_im, seam);
      removeSingleSeamInPlace(idxMap, seam);
    }

    cv::Mat insertions = cv::Mat::zeros(H, W, CV_32S);
    for (const auto& s_vec : original_seams) {
      for (int i = 0; i < H; ++i)
        insertions.ptr<int>(i)[s_vec[i]]++;
    }

    size_t esz = img.elemSize();
    cv::Mat output(H, W + num_add, img.type());

#pragma omp parallel for schedule(static)
    for (int i = 0; i < H; ++i) {
      const uchar* src = img.ptr<uchar>(i);
      uchar* dst = output.ptr<uchar>(i);
      const int* ins = insertions.ptr<int>(i);
      int dst_col = 0;
      for (int j = 0; j < W; ++j) {
        std::memcpy(dst + (dst_col * esz), src + (j * esz), esz);
        dst_col++;
        if (ins[j] > 0) {
          int next_j = std::min(j + 1, W - 1);
          const uchar* p1 = src + (j * esz);
          const uchar* p2 = src + (next_j * esz);
          for (int k = 0; k < ins[j]; ++k) {
            uchar* avg = dst + (dst_col * esz);
            for (size_t c = 0; c < esz; ++c) {
              avg[c] = static_cast<uchar>((p1[c] + p2[c]) / 2);
            }
            dst_col++;
          }
        }
      }
    }
    return output;
  }
};
