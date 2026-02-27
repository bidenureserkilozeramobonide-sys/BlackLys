#include "HdrEngine.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <opencv2/video.hpp>

#include <algorithm>
#include <cmath>

#include <libraw/libraw.h>

// ═══════════════════════════════════════════════════════
//  Image I/O
// ═══════════════════════════════════════════════════════

// Helper: detect RAW by extension
static bool isRawFile(const QString &path) {
  static const QStringList rawExts = {"cr2", "cr3", "nef", "arw", "dng",
                                      "orf", "raf", "rw2", "pef", "srw",
                                      "x3f", "3fr", "iiq", "raw"};
  return rawExts.contains(QFileInfo(path).suffix().toLower());
}

cv::Mat HdrEngine::loadImage(const QString &path) {
  // Try RAW first
  if (isRawFile(path)) {
    qDebug() << "[HdrEngine] Loading RAW file:" << path;
    LibRaw raw;
    int ret = raw.open_file(path.toStdString().c_str());
    if (ret != LIBRAW_SUCCESS) {
      qWarning() << "[HdrEngine] LibRaw open failed:" << libraw_strerror(ret);
      return {};
    }

    ret = raw.unpack();
    if (ret != LIBRAW_SUCCESS) {
      qWarning() << "[HdrEngine] LibRaw unpack failed:" << libraw_strerror(ret);
      return {};
    }

    // Auto white balance + gamma
    raw.imgdata.params.use_auto_wb = 1;
    raw.imgdata.params.use_camera_wb = 1;
    raw.imgdata.params.output_bps = 16;     // 16-bit output
    raw.imgdata.params.gamm[0] = 1.0 / 2.4; // sRGB gamma
    raw.imgdata.params.gamm[1] = 12.92;
    raw.imgdata.params.output_color = 1; // sRGB
    raw.imgdata.params.half_size =
        0; // Full sensor resolution for maximum quality
    raw.imgdata.params.fbdd_noiserd = 1;   // fast denoise during decode
    raw.imgdata.params.no_auto_bright = 1; // don't auto-brighten

    ret = raw.dcraw_process();
    if (ret != LIBRAW_SUCCESS) {
      qWarning() << "[HdrEngine] LibRaw process failed:"
                 << libraw_strerror(ret);
      return {};
    }

    libraw_processed_image_t *img = raw.dcraw_make_mem_image(&ret);
    if (!img || ret != LIBRAW_SUCCESS) {
      qWarning() << "[HdrEngine] LibRaw mem_image failed";
      return {};
    }

    // Convert to cv::Mat
    cv::Mat rgb;
    if (img->bits == 16) {
      cv::Mat raw16(img->height, img->width, CV_16UC3, img->data);
      raw16.convertTo(rgb, CV_32FC3, 1.0 / 65535.0);
    } else {
      cv::Mat raw8(img->height, img->width, CV_8UC3, img->data);
      raw8.convertTo(rgb, CV_32FC3, 1.0 / 255.0);
    }

    LibRaw::dcraw_clear_mem(img);
    raw.recycle();

    qDebug() << "[HdrEngine] RAW loaded:" << rgb.cols << "x" << rgb.rows;
    return rgb;
  }

  // Fallback: standard image (JPEG, PNG, TIFF, BMP)
  // Use IMREAD_ANYCOLOR | IMREAD_ANYDEPTH to handle 16-bit natively
  cv::Mat bgr =
      cv::imread(path.toStdString(), cv::IMREAD_ANYCOLOR | cv::IMREAD_ANYDEPTH);
  if (bgr.empty()) {
    qWarning() << "[HdrEngine] Could not load:" << path;
    return {};
  }

  cv::Mat rgb;
  if (bgr.channels() == 1) {
    cv::cvtColor(bgr, rgb, cv::COLOR_GRAY2RGB);
  } else {
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
  }

  // Normalize to float32 [0..1]
  cv::Mat flt;
  if (rgb.depth() == CV_16U) {
    rgb.convertTo(flt, CV_32FC3, 1.0 / 65535.0);
  } else {
    rgb.convertTo(flt, CV_32FC3, 1.0 / 255.0);
  }

  return flt;
}

cv::Mat HdrEngine::loadImage(const QString &path, int maxSize) {
  // Fast low-res loading: use native downscaling in decoders
  if (isRawFile(path)) {
    qDebug() << "[HdrEngine] Loading RAW (low-res):" << path;
    LibRaw raw;
    int ret = raw.open_file(path.toStdString().c_str());
    if (ret != LIBRAW_SUCCESS)
      return {};
    ret = raw.unpack();
    if (ret != LIBRAW_SUCCESS)
      return {};

    raw.imgdata.params.use_auto_wb = 1;
    raw.imgdata.params.use_camera_wb = 1;
    raw.imgdata.params.output_bps = 16;
    raw.imgdata.params.gamm[0] = 1.0 / 2.4;
    raw.imgdata.params.gamm[1] = 12.92;
    raw.imgdata.params.output_color = 1;
    raw.imgdata.params.half_size = 1; // 4× faster: half sensor resolution
    raw.imgdata.params.fbdd_noiserd = 1;
    raw.imgdata.params.no_auto_bright = 1;

    ret = raw.dcraw_process();
    if (ret != LIBRAW_SUCCESS)
      return {};

    libraw_processed_image_t *img = raw.dcraw_make_mem_image(&ret);
    if (!img || ret != LIBRAW_SUCCESS)
      return {};

    cv::Mat rgb;
    if (img->bits == 16) {
      cv::Mat raw16(img->height, img->width, CV_16UC3, img->data);
      raw16.convertTo(rgb, CV_32FC3, 1.0 / 65535.0);
    } else {
      cv::Mat raw8(img->height, img->width, CV_8UC3, img->data);
      raw8.convertTo(rgb, CV_32FC3, 1.0 / 255.0);
    }
    LibRaw::dcraw_clear_mem(img);
    raw.recycle();

    // Further resize if still larger than maxSize
    if (maxSize > 0 && (rgb.cols > maxSize || rgb.rows > maxSize)) {
      double scale =
          std::min((double)maxSize / rgb.cols, (double)maxSize / rgb.rows);
      cv::Mat resized;
      cv::resize(rgb, resized, cv::Size(), scale, scale, cv::INTER_AREA);
      return resized;
    }
    return rgb;
  }

  // Standard images: use IMREAD_REDUCED for fast decoding
  cv::Mat bgr =
      cv::imread(path.toStdString(), cv::IMREAD_ANYCOLOR | cv::IMREAD_ANYDEPTH |
                                         cv::IMREAD_REDUCED_COLOR_2);
  if (bgr.empty()) {
    // Fallback to normal load if reduced fails
    bgr = cv::imread(path.toStdString(),
                     cv::IMREAD_ANYCOLOR | cv::IMREAD_ANYDEPTH);
  }
  if (bgr.empty())
    return {};

  cv::Mat rgb;
  if (bgr.channels() == 1)
    cv::cvtColor(bgr, rgb, cv::COLOR_GRAY2RGB);
  else
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

  cv::Mat flt;
  if (rgb.depth() == CV_16U)
    rgb.convertTo(flt, CV_32FC3, 1.0 / 65535.0);
  else
    rgb.convertTo(flt, CV_32FC3, 1.0 / 255.0);

  // Further resize if needed
  if (maxSize > 0 && (flt.cols > maxSize || flt.rows > maxSize)) {
    double scale =
        std::min((double)maxSize / flt.cols, (double)maxSize / flt.rows);
    cv::Mat resized;
    cv::resize(flt, resized, cv::Size(), scale, scale, cv::INTER_AREA);
    return resized;
  }
  return flt;
}

bool HdrEngine::saveImage(const cv::Mat &img, const QString &path,
                          const HdrSettings &settings) {
  cv::Mat bgr;
  cv::cvtColor(img, bgr, cv::COLOR_RGB2BGR);

  QString suffix = QFileInfo(path).suffix().toLower();

  if (suffix == "tiff" || suffix == "tif") {
    cv::Mat out;
    bgr.convertTo(out, CV_16UC3, 65535.0);
    return cv::imwrite(path.toStdString(), out);
  }

  // 8-bit output
  cv::Mat out;
  bgr.convertTo(out, CV_8UC3, 255.0);
  cv::Mat clamped;
  cv::max(out, 0, clamped);
  cv::min(clamped, 255, clamped);

  if (suffix == "png") {
    std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 3};
    return cv::imwrite(path.toStdString(), clamped, params);
  }

  // Default: JPEG
  std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, settings.jpegQuality};
  return cv::imwrite(path.toStdString(), clamped, params);
}

QImage HdrEngine::matToQImage(const cv::Mat &mat) {
  // Clamp to [0,1]
  cv::Mat clamped;
  cv::max(mat, 0.0f, clamped);
  cv::min(clamped, 1.0f, clamped);

  // Convert to 16-bit per channel for smooth gradients in highlights
  cv::Mat rgb16;
  clamped.convertTo(rgb16, CV_16UC3, 65535.0);

  // Add alpha channel (RGBX64 = RGBA 16-bit with opaque alpha)
  cv::Mat rgba16;
  cv::cvtColor(rgb16, rgba16, cv::COLOR_RGB2RGBA);

  return QImage(rgba16.data, rgba16.cols, rgba16.rows, rgba16.step,
                QImage::Format_RGBX64)
      .copy();
}

// ═══════════════════════════════════════════════════════
//  Alignment (ECC — same as Python)
// ═══════════════════════════════════════════════════════

std::vector<cv::Mat>
HdrEngine::alignBrackets(const std::vector<cv::Mat> &images) {
  if (images.size() <= 1)
    return images;

  std::vector<cv::Mat> aligned;
  aligned.push_back(images[0].clone());

  int origW = images[0].cols;
  int origH = images[0].rows;

  // ── Multi-scale pyramid: coarse (200px) → fine (600px) ──
  // Coarse pass catches large shifts, fine pass refines sub-pixel
  struct PyramidLevel {
    int maxSize;
    int iterations;
    double epsilon;
  };
  std::vector<PyramidLevel> levels = {
      {200, 100, 1e-3}, // coarse: fast, catches large motion
      {600, 150, 1e-5}, // fine: precise sub-pixel refinement
  };

  // Prepare reference grayscale at each level
  cv::Mat ref8;
  images[0].convertTo(ref8, CV_8UC3, 255.0);
  cv::Mat refGrayFull;
  cv::cvtColor(ref8, refGrayFull, cv::COLOR_RGB2GRAY);

  for (size_t i = 1; i < images.size(); ++i) {
    cv::Mat img8;
    images[i].convertTo(img8, CV_8UC3, 255.0);
    cv::Mat imgGrayFull;
    cv::cvtColor(img8, imgGrayFull, cv::COLOR_RGB2GRAY);

    // Start with identity warp (homography 3x3)
    cv::Mat warpMatrix = cv::Mat::eye(3, 3, CV_32F);
    bool useHomography = true;
    bool success = false;

    // Multi-scale pyramid alignment
    for (const auto &level : levels) {
      double scale = 1.0;
      if (origW > level.maxSize || origH > level.maxSize) {
        scale = std::min((double)level.maxSize / origW,
                         (double)level.maxSize / origH);
      }

      cv::Mat refScaled = refGrayFull, imgScaled = imgGrayFull;
      if (scale < 1.0) {
        cv::resize(refGrayFull, refScaled, cv::Size(), scale, scale,
                   cv::INTER_AREA);
        cv::resize(imgGrayFull, imgScaled, cv::Size(), scale, scale,
                   cv::INTER_AREA);
      }

      // Scale the current warp matrix for this pyramid level
      cv::Mat scaledWarp = warpMatrix.clone();
      if (useHomography) {
        // Scale translation components of homography
        scaledWarp.at<float>(0, 2) *= (float)scale;
        scaledWarp.at<float>(1, 2) *= (float)scale;
        scaledWarp.at<float>(2, 0) /= (float)scale;
        scaledWarp.at<float>(2, 1) /= (float)scale;
      }

      cv::TermCriteria criteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS,
                                level.iterations, level.epsilon);

      try {
        if (useHomography) {
          cv::findTransformECC(refScaled, imgScaled, scaledWarp,
                               cv::MOTION_HOMOGRAPHY, criteria);
        } else {
          // Fallback: 2x3 affine
          cv::Mat affineWarp = scaledWarp(cv::Rect(0, 0, 3, 2)).clone();
          cv::findTransformECC(refScaled, imgScaled, affineWarp,
                               cv::MOTION_AFFINE, criteria);
          scaledWarp = cv::Mat::eye(3, 3, CV_32F);
          affineWarp.copyTo(scaledWarp(cv::Rect(0, 0, 3, 2)));
        }

        // Scale warp back to full resolution
        warpMatrix = scaledWarp.clone();
        if (scale < 1.0) {
          warpMatrix.at<float>(0, 2) /= (float)scale;
          warpMatrix.at<float>(1, 2) /= (float)scale;
          warpMatrix.at<float>(2, 0) *= (float)scale;
          warpMatrix.at<float>(2, 1) *= (float)scale;
        }
        success = true;
      } catch (const cv::Exception &) {
        if (useHomography) {
          // Fallback: try affine instead of homography
          useHomography = false;
          warpMatrix = cv::Mat::eye(3, 3, CV_32F);
        }
      }
    }

    // Apply the final warp
    if (success) {
      cv::Mat warped;
      if (useHomography) {
        cv::warpPerspective(images[i], warped, warpMatrix, images[i].size(),
                            cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                            cv::BORDER_REFLECT);
      } else {
        cv::Mat affineWarp = warpMatrix(cv::Rect(0, 0, 3, 2));
        cv::warpAffine(images[i], warped, affineWarp, images[i].size(),
                       cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                       cv::BORDER_REFLECT);
      }
      aligned.push_back(warped);
    } else {
      qWarning() << "[HdrEngine] Alignment failed for image" << i
                 << ", using original";
      aligned.push_back(images[i].clone());
    }
  }

  return aligned;
}

// ═══════════════════════════════════════════════════════
//  HDR Fusion
// ═══════════════════════════════════════════════════════

cv::Mat HdrEngine::mergeMertens(const std::vector<cv::Mat> &images) {
  // Convert to 8-bit for Mertens
  std::vector<cv::Mat> images8;
  for (const auto &img : images) {
    cv::Mat u8;
    img.convertTo(u8, CV_8UC3, 255.0);
    images8.push_back(u8);
  }

  // ── Anti-ghosting: detect and mask moving objects ──
  if (images8.size() >= 3) {
    // Use middle image as reference
    int refIdx = (int)images8.size() / 2;

    // Convert all to grayscale
    std::vector<cv::Mat> grays(images8.size());
    for (size_t i = 0; i < images8.size(); ++i)
      cv::cvtColor(images8[i], grays[i], cv::COLOR_RGB2GRAY);

    // Compute median image (approximate via pixel-wise median)
    cv::Mat median = grays[refIdx].clone();
    if (images8.size() == 3) {
      // Exact median of 3 images: sort per-pixel using intermediates
      cv::Mat a = grays[0], b = grays[1], c = grays[2];
      cv::Mat minAB, maxAB, minMaxAB_C;
      cv::min(a, b, minAB);
      cv::max(a, b, maxAB);
      cv::min(maxAB, c, minMaxAB_C);
      cv::max(minAB, minMaxAB_C, median);
    }

    // For each image, detect ghosted areas (deviation from median)
    float ghostThreshold = 25.0f; // intensity difference threshold
    for (size_t i = 0; i < images8.size(); ++i) {
      if ((int)i == refIdx)
        continue;

      cv::Mat diff;
      cv::absdiff(grays[i], median, diff);

      // Threshold: pixels that moved between brackets
      cv::Mat ghostMask;
      cv::threshold(diff, ghostMask, ghostThreshold, 255, cv::THRESH_BINARY);

      // Dilate to cover edges of moving objects
      cv::Mat kernel =
          cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
      cv::dilate(ghostMask, ghostMask, kernel);

      // Blur for smooth blending at ghost boundaries
      cv::GaussianBlur(ghostMask, ghostMask, cv::Size(15, 15), 0);

      // In ghosted areas, replace with reference image
      cv::Mat ghostMask3;
      cv::cvtColor(ghostMask, ghostMask3, cv::COLOR_GRAY2RGB);
      cv::Mat blendMask;
      ghostMask3.convertTo(blendMask, CV_32F, 1.0 / 255.0);
      cv::Mat img_f, ref_f;
      images8[i].convertTo(img_f, CV_32F);
      images8[refIdx].convertTo(ref_f, CV_32F);

      // Blend: ghosted areas use reference, rest keeps original
      cv::Mat blended;
      cv::multiply(ref_f, blendMask, blended);
      cv::Mat invMask;
      cv::subtract(cv::Scalar(1.0f, 1.0f, 1.0f), blendMask, invMask);
      cv::multiply(img_f, invMask, img_f);
      cv::add(img_f, blended, img_f);
      img_f.convertTo(images8[i], CV_8UC3);
    }
  }

  auto merger = cv::createMergeMertens(1.2f, 1.3f, 0.5f);
  cv::Mat result;
  merger->process(images8, result);
  return result;
}

cv::Mat HdrEngine::mergeDebevec(const std::vector<cv::Mat> &images) {
  // Convert to 8-bit for Debevec
  std::vector<cv::Mat> images8;
  for (const auto &img : images) {
    cv::Mat u8;
    img.convertTo(u8, CV_8UC3, 255.0);
    images8.push_back(u8);
  }

  // Synthetic exposure times
  std::vector<float> times;
  for (size_t i = 0; i < images.size(); ++i) {
    times.push_back(1.0f + 0.1f * i);
  }

  auto calibrate = cv::createCalibrateRobertson();
  cv::Mat response;
  calibrate->process(images8, response, times);

  auto merge = cv::createMergeDebevec();
  cv::Mat hdr;
  merge->process(images8, hdr, times, response);

  auto tonemap = cv::createTonemapReinhard(1.2f, 0.0f, 0.8f, 0.0f);
  cv::Mat ldr;
  tonemap->process(hdr, ldr);

  return ldr;
}

// ═══════════════════════════════════════════════════════
//  Adjustment Pipeline (matches Python exactly)
// ═══════════════════════════════════════════════════════

cv::Mat HdrEngine::applyAdjustments(const cv::Mat &img,
                                    const HdrSettings &settings, bool isPreview,
                                    bool fastPreview) {
  cv::Mat out;
  cv::max(img, 0.0f, out);
  cv::min(out, 1.0f, out);
  out.convertTo(out, CV_32F);

  // ─── LIGHT PANEL ───

  // 1. Base stretch (normalize dynamic range)
  {
    cv::Mat flat = out.reshape(1, out.total() * out.channels());
    double lo, hi;
    cv::minMaxLoc(flat, &lo, &hi);
    if (hi > lo + 1e-5) {
      double range = hi - lo;
      double pLo = lo + range * 0.005;
      double pHi = hi - range * 0.005;
      if (pHi > pLo + 1e-5) {
        out = (out - pLo) / (pHi - pLo);
      }
    }
    cv::max(out, 0.0f, out);
    cv::min(out, 1.0f, out);
  }

  // 2. Exposure
  out *= settings.exposure;

  // 3. Contrast
  out = 0.5f + (out - 0.5f) * (float)settings.contrast;

  // 3b. Clarity — local mid-tone contrast via high-pass filter
  if (!fastPreview && std::abs(settings.clarity) > 1e-4) {
    float cl = (float)settings.clarity;
    cv::Mat gray;
    cv::cvtColor(out, gray, cv::COLOR_RGB2GRAY);

    // Low-frequency: large blur to extract structure
    int kSize = std::max(31, (out.cols / 20) | 1); // ~5% of width, must be odd
    cv::Mat lowFreq;
    cv::GaussianBlur(gray, lowFreq, cv::Size(kSize, kSize), 0);

    // High-pass = gray - lowFreq (mid-tone structure)
    cv::Mat highPass = gray - lowFreq;

    // Mid-tone mask: only affect pixels in 0.15..0.85 range (avoid clipping)
    cv::Mat mask = cv::Mat::ones(gray.size(), CV_32F);
    cv::Mat darkMask = cv::max(gray - 0.15f, 0.0f) * (1.0f / 0.15f);
    cv::min(darkMask, 1.0f, darkMask);
    cv::Mat brightMask = cv::max(0.85f - gray, 0.0f) * (1.0f / 0.15f);
    cv::min(brightMask, 1.0f, brightMask);
    cv::multiply(darkMask, brightMask, mask);

    // Apply: out += clarity * highPass * mask (per channel)
    cv::Mat adjustment;
    cv::multiply(highPass, mask, adjustment);
    adjustment *= cl * 1.5f;

    std::vector<cv::Mat> channels(3);
    cv::split(out, channels);
    for (int c = 0; c < 3; ++c)
      channels[c] += adjustment;
    cv::merge(channels, out);
  }

  // 3c. Texture — micro-contrast on fine details (high-frequency only)
  if (!fastPreview && std::abs(settings.texture) > 1e-4) {
    float tex = (float)settings.texture;
    cv::Mat gray;
    cv::cvtColor(out, gray, cv::COLOR_RGB2GRAY);

    // Small blur to isolate fine detail
    cv::Mat smallBlur;
    cv::GaussianBlur(gray, smallBlur, cv::Size(0, 0), 2.0);

    // Detail = gray - smallBlur (high-frequency content)
    cv::Mat detail = gray - smallBlur;

    // Apply to all channels uniformly (no mid-tone masking)
    detail *= tex * 2.0f;
    std::vector<cv::Mat> channels(3);
    cv::split(out, channels);
    for (int c = 0; c < 3; ++c)
      channels[c] += detail;
    cv::merge(channels, out);
  }

  // 4-7. Highlights, Shadows, Whites, Blacks — vectorized mask operations
  {
    bool doHL = std::abs(settings.highlights) > 1e-4;
    bool doSH = std::abs(settings.shadows) > 1e-4;
    bool doWH = std::abs(settings.whites) > 1e-4;
    bool doBL = std::abs(settings.blacks) > 1e-4;

    if (doHL || doSH || doWH || doBL) {
      cv::Mat gray;
      cv::cvtColor(out, gray, cv::COLOR_RGB2GRAY);

      // Split channels for per-channel adjustment
      std::vector<cv::Mat> channels(3);
      cv::split(out, channels);

      if (doHL) {
        // Highlights: per-channel soft-knee compression (Reinhard-style)
        // Negative = recover/compress highlights, Positive = boost
        // Preserves color ratios naturally — no harsh artifacts
        float hl = -(float)settings.highlights; // negative = compress
        float knee = 0.8f; // only affect truly bright pixels (was 0.65)
        float strength = std::abs(hl) * 8.0f; // stronger compression (was 4.0)

        for (int c = 0; c < 3; ++c) {
          cv::Mat v = channels[c].clone();
          cv::Mat excess = cv::max(v - knee, 0.0f);

          if (hl > 0) {
            // Compress: v_new = knee + excess / (1 + strength * excess)
            cv::Mat denom = 1.0f + strength * excess;
            cv::Mat compressed;
            cv::divide(excess, denom, compressed);
            channels[c] = cv::min(v, knee) + compressed;
          } else {
            // Boost: expand above knee
            channels[c] = v + excess * std::abs(hl) * 0.5f;
          }
        }
      }

      if (doSH) {
        // Shadows: additive lift proportional to darkness
        // Positive = lift shadows, negative = crush
        float sh = (float)settings.shadows;
        cv::Mat t = cv::max(0.5f - gray, 0.0f) * 2.0f;
        cv::min(t, 1.0f, t);
        cv::Mat mask;
        cv::multiply(t, t, mask);

        // Lift amount: proportional to (1 - channel) to avoid blowing out
        // bright parts
        for (int c = 0; c < 3; ++c) {
          cv::Mat headroom = 1.0f - channels[c];
          cv::Mat lift;
          cv::multiply(mask, headroom, lift);
          channels[c] += lift * sh * 0.6f;
        }
      }

      if (doWH) {
        // Whites: multiplicative for very bright areas
        float wh = (float)settings.whites;
        cv::Mat t = cv::max(gray - 0.75f, 0.0f) * 4.0f;
        cv::min(t, 1.0f, t);
        cv::Mat mask;
        cv::multiply(t, t, mask);
        cv::Mat factor = 1.0f + wh * mask * 0.5f;
        cv::max(factor, 0.1f, factor);
        for (int c = 0; c < 3; ++c)
          cv::multiply(channels[c], factor, channels[c]);
      }

      if (doBL) {
        // Blacks: multiplicative crush for very dark areas
        float bl = (float)settings.blacks;
        cv::Mat t = cv::max(0.25f - gray, 0.0f) * 4.0f;
        cv::min(t, 1.0f, t);
        cv::Mat mask;
        cv::multiply(t, t, mask);
        cv::Mat factor = 1.0f - bl * mask * 0.5f;
        cv::max(factor, 0.1f, factor);
        for (int c = 0; c < 3; ++c)
          cv::multiply(channels[c], factor, channels[c]);
      }

      cv::merge(channels, out);
    }
  }

  cv::max(out, 0.0f, out);
  cv::min(out, 1.0f, out);

  // ─── COLOR PANEL ───

  // 8. Temperature
  if (std::abs(settings.temperature) > 0.5) {
    float temp = (float)(settings.temperature / 100.0);
    std::vector<cv::Mat> channels(3);
    cv::split(out, channels);
    channels[0] += temp * 0.2f;
    channels[2] -= temp * 0.2f;
    cv::merge(channels, out);
  }

  // 9. Tint
  if (std::abs(settings.tint) > 0.5) {
    float t = (float)(settings.tint / 100.0);
    std::vector<cv::Mat> channels(3);
    cv::split(out, channels);
    channels[1] -= t * 0.12f;
    channels[0] += t * 0.04f;
    cv::merge(channels, out);
  }

  // 10-11. Vibrance and Saturation — vectorized
  {
    bool doVib = std::abs(settings.vibrance) > 1e-4;
    bool doSat = std::abs(settings.saturation - 1.0) > 1e-4;

    if (doVib || doSat) {
      cv::Mat gray;
      cv::cvtColor(out, gray, cv::COLOR_RGB2GRAY);

      if (doVib) {
        float vib = (float)settings.vibrance;
        std::vector<cv::Mat> channels(3);
        cv::split(out, channels);

        // Compute max deviation from gray per pixel (saturation proxy)
        cv::Mat diff0, diff1, diff2;
        cv::absdiff(channels[0], gray, diff0);
        cv::absdiff(channels[1], gray, diff1);
        cv::absdiff(channels[2], gray, diff2);
        cv::Mat maxDiff = cv::max(cv::max(diff0, diff1), diff2);

        // Weight = 1 - maxDiff (boost unsaturated pixels more)
        cv::Mat weight = 1.0f - maxDiff;
        cv::Mat factor = 1.0f + weight * vib;

        // Apply: out = gray + (out - gray) * factor
        for (int c = 0; c < 3; ++c) {
          cv::Mat delta;
          cv::subtract(channels[c], gray, delta);
          cv::multiply(delta, factor, channels[c]);
          channels[c] += gray;
        }
        cv::merge(channels, out);

        // Recompute gray if saturation also needed
        if (doSat)
          cv::cvtColor(out, gray, cv::COLOR_RGB2GRAY);
      }

      if (doSat) {
        float sat = (float)settings.saturation;
        std::vector<cv::Mat> channels(3);
        cv::split(out, channels);
        for (int c = 0; c < 3; ++c) {
          // out = gray + (out - gray) * sat
          cv::Mat delta;
          cv::subtract(channels[c], gray, delta);
          channels[c] = gray + delta * sat;
        }
        cv::merge(channels, out);
      }
    }
  }

  cv::max(out, 0.0f, out);
  cv::min(out, 1.0f, out);

  // ─── HSL / COLOR MIXER ───
  {
    bool doHSL = false;
    for (int i = 0; i < 8; ++i) {
      if (std::abs(settings.hslHue[i]) > 0.5 ||
          std::abs(settings.hslSat[i]) > 1e-4 ||
          std::abs(settings.hslLum[i]) > 1e-4) {
        doHSL = true;
        break;
      }
    }
    if (doHSL) {
      // Convert RGB → HSV
      cv::Mat hsv;
      cv::cvtColor(out, hsv, cv::COLOR_RGB2HSV);

      // HSV: H in [0,360), S in [0,1], V in [0,1]
      // 8 hue bins centered at: 0(Red), 30(Orange), 60(Yellow), 120(Green),
      //   180(Aqua), 240(Blue), 300(Purple), 330(Magenta)
      const float hueCenters[8] = {0.0f,   30.0f,  60.0f,  120.0f,
                                   180.0f, 240.0f, 300.0f, 330.0f};

      int rows = hsv.rows, cols = hsv.cols;
      for (int y = 0; y < rows; ++y) {
        float *ptr = hsv.ptr<float>(y);
        for (int x = 0; x < cols; ++x) {
          float h = ptr[x * 3 + 0];
          float s = ptr[x * 3 + 1];
          float v = ptr[x * 3 + 2];

          // Skip near-gray pixels (low saturation)
          if (s < 0.05f)
            continue;

          // Accumulate weighted adjustments from all 8 hue bins
          float hueShift = 0.0f, satScale = 0.0f, lumShift = 0.0f;
          float totalWeight = 0.0f;

          for (int i = 0; i < 8; ++i) {
            float center = hueCenters[i];
            float diff = std::abs(h - center);
            if (diff > 180.0f)
              diff = 360.0f - diff;
            // Soft transition: 30° full, falls off to 0 at 60°
            float w = std::max(0.0f, 1.0f - diff / 45.0f);
            if (w > 0.0f) {
              hueShift += w * (float)settings.hslHue[i];
              satScale += w * (float)settings.hslSat[i];
              lumShift += w * (float)settings.hslLum[i];
              totalWeight += w;
            }
          }

          if (totalWeight > 0.0f) {
            hueShift /= totalWeight;
            satScale /= totalWeight;
            lumShift /= totalWeight;

            // Apply hue shift
            h += hueShift;
            if (h < 0.0f)
              h += 360.0f;
            if (h >= 360.0f)
              h -= 360.0f;

            // Apply saturation
            s = std::clamp(s * (1.0f + satScale), 0.0f, 1.0f);

            // Apply luminance
            v = std::clamp(v + lumShift * 0.5f, 0.0f, 1.0f);

            ptr[x * 3 + 0] = h;
            ptr[x * 3 + 1] = s;
            ptr[x * 3 + 2] = v;
          }
        }
      }

      cv::cvtColor(hsv, out, cv::COLOR_HSV2RGB);
    }
  }

  // ─── COLOR GRADING (3-way split toning) ───
  {
    bool doCG = (settings.cgShadowSat > 1e-4 || settings.cgMidtoneSat > 1e-4 ||
                 settings.cgHighlightSat > 1e-4);
    if (doCG) {
      cv::Mat gray;
      cv::cvtColor(out, gray, cv::COLOR_RGB2GRAY);

      // Precompute toning colors for each zone
      auto hueToRGB = [](float hue, float sat) -> cv::Vec3f {
        // Convert hue (0-360) + sat (0-1) to RGB offset
        float h = hue / 60.0f;
        float c = sat;
        float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
        float r = 0, g = 0, b = 0;
        if (h < 1) {
          r = c;
          g = x;
        } else if (h < 2) {
          r = x;
          g = c;
        } else if (h < 3) {
          g = c;
          b = x;
        } else if (h < 4) {
          g = x;
          b = c;
        } else if (h < 5) {
          r = x;
          b = c;
        } else {
          r = c;
          b = x;
        }
        return cv::Vec3f(r, g, b);
      };

      cv::Vec3f shadowTone =
          hueToRGB((float)settings.cgShadowHue, (float)settings.cgShadowSat);
      cv::Vec3f midTone =
          hueToRGB((float)settings.cgMidtoneHue, (float)settings.cgMidtoneSat);
      cv::Vec3f highTone = hueToRGB((float)settings.cgHighlightHue,
                                    (float)settings.cgHighlightSat);

      int rows = out.rows, cols = out.cols;
      for (int y = 0; y < rows; ++y) {
        float *outPtr = out.ptr<float>(y);
        const float *grayPtr = gray.ptr<float>(y);
        for (int x = 0; x < cols; ++x) {
          float lum = grayPtr[x];

          // Shadow weight: 1 at lum=0, 0 at lum=0.5
          float sw = std::max(0.0f, 1.0f - lum * 2.0f);
          sw *= sw; // smooth falloff

          // Highlight weight: 1 at lum=1, 0 at lum=0.5
          float hw = std::max(0.0f, (lum - 0.5f) * 2.0f);
          hw *= hw;

          // Midtone weight: 1 at lum=0.5, 0 at edges
          float mw = 1.0f - sw - hw;
          mw = std::max(0.0f, mw);

          for (int c = 0; c < 3; ++c) {
            float tint =
                sw * shadowTone[c] + mw * midTone[c] + hw * highTone[c];
            outPtr[x * 3 + c] =
                std::clamp(outPtr[x * 3 + c] + tint * 0.15f, 0.0f, 1.0f);
          }
        }
      }
    }
  }

  cv::max(out, 0.0f, out);
  cv::min(out, 1.0f, out);

  // ─── EFFECTS PANEL ───

  // 12. Dehaze — vectorized dark channel prior
  if (!fastPreview && std::abs(settings.dehaze) > 1e-4) {
    float strength = std::clamp((float)settings.dehaze, -0.8f, 0.8f);
    std::vector<cv::Mat> chans(3);
    cv::split(out, chans);

    // Dark channel = per-pixel minimum across RGB
    cv::Mat dark = cv::min(cv::min(chans[0], chans[1]), chans[2]);

    // Erode for local minimum estimation
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
    cv::erode(dark, dark, kernel);

    double atmospheric;
    cv::minMaxLoc(dark, nullptr, &atmospheric);
    atmospheric = std::clamp(atmospheric, 0.1, 0.95);

    // Transmission map
    cv::Mat trans = 1.0f - strength * (dark / ((float)atmospheric + 1e-6f));
    cv::max(trans, 0.3f, trans);
    cv::min(trans, 1.0f, trans);

    // Apply dehaze per channel: v = (v - atm*(1-trans)) / trans
    float atm = (float)atmospheric;
    cv::Mat atmCorrection = atm * (1.0f - trans);
    for (int c = 0; c < 3; ++c) {
      chans[c] = (chans[c] - atmCorrection);
      cv::divide(chans[c], trans, chans[c]);
    }
    cv::merge(chans, out);
  }

  // 13. Vignette — precomputed radial mask
  if (!fastPreview && std::abs(settings.vignette) > 1e-4) {
    int h = out.rows, w = out.cols;
    float cy = h / 2.0f, cx = w / 2.0f;
    float maxDist = std::sqrt(cx * cx + cy * cy);

    // Build 1D coordinate arrays
    cv::Mat rowCoords(h, 1, CV_32F);
    cv::Mat colCoords(1, w, CV_32F);
    for (int y = 0; y < h; ++y)
      rowCoords.at<float>(y) = (y - cy) * (y - cy);
    for (int x = 0; x < w; ++x)
      colCoords.at<float>(x) = (x - cx) * (x - cx);

    // dist² = rowCoords + colCoords (broadcast via repeat)
    cv::Mat rowBroadcast, colBroadcast;
    cv::repeat(rowCoords, 1, w, rowBroadcast);
    cv::repeat(colCoords, h, 1, colBroadcast);
    cv::Mat dist2 = rowBroadcast + colBroadcast;

    // Normalized distance² and mask
    cv::Mat nd2 = dist2 / (maxDist * maxDist);
    cv::Mat mask = 1.0f - (float)settings.vignette * nd2;
    cv::max(mask, 0.0f, mask);
    cv::min(mask, 1.5f, mask);

    // Apply to all channels
    std::vector<cv::Mat> channels(3);
    cv::split(out, channels);
    for (int c = 0; c < 3; ++c)
      cv::multiply(channels[c], mask, channels[c]);
    cv::merge(channels, out);
  }

  cv::max(out, 0.0f, out);
  cv::min(out, 1.0f, out);

  // ─── DETAIL PANEL ───

  // 14. Sharpening (unsharp mask) — already vectorized
  if (settings.sharpening > 0.5) {
    float amount = (float)(settings.sharpening / 100.0);
    cv::Mat blurred;
    cv::GaussianBlur(out, blurred, cv::Size(0, 0), 1.5);
    cv::addWeighted(out, 1.0 + amount, blurred, -amount, 0, out);
  }

  // 15. Noise reduction (fast bilateral) — skip during interactive preview
  if (!isPreview && settings.noiseReduction > 0.5) {
    int d = std::clamp((int)(settings.noiseReduction / 20) + 1, 1, 5);
    double sigmaColor = settings.noiseReduction * 0.5 + 10;
    double sigmaSpace = settings.noiseReduction * 0.3 + 10;
    cv::Mat u8;
    out.convertTo(u8, CV_8UC3, 255.0);
    cv::Mat filtered;
    cv::bilateralFilter(u8, filtered, d, sigmaColor, sigmaSpace);
    filtered.convertTo(out, CV_32FC3, 1.0 / 255.0);
  }

  cv::max(out, 0.0f, out);
  cv::min(out, 1.0f, out);

  // ─── FINAL TONE MAPPING ── vectorized S-curve
  // s = v² * (3 - 2v), out = 0.7v + 0.3s
  {
    cv::Mat v2;
    cv::multiply(out, out, v2); // v²
    cv::Mat v3;
    cv::multiply(v2, out, v3);         // v³
    cv::Mat s = v2 * 3.0f - v3 * 2.0f; // smoothstep
    out = out * 0.7f + s * 0.3f;
  }

  cv::max(out, 0.0f, out);
  cv::min(out, 1.0f, out);

  return out;
}

// ═══════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════

HdrJobResult HdrEngine::mergeBracketGroup(const QStringList &inputPaths,
                                          const HdrSettings &settings,
                                          const QString &outputDir,
                                          const QString &outputName) {
  HdrJobResult result;

  if (inputPaths.isEmpty()) {
    result.error = "No input paths provided";
    return result;
  }

  // Load all images
  std::vector<cv::Mat> images;
  for (const auto &path : inputPaths) {
    cv::Mat img = loadImage(path);
    if (!img.empty()) {
      images.push_back(img);
    }
  }

  if (images.empty()) {
    result.error = "No images could be loaded";
    return result;
  }

  qDebug() << "[HdrEngine] Loaded" << images.size() << "images, aligning...";

  // Align
  auto aligned = alignBrackets(images);

  qDebug() << "[HdrEngine] Aligned, merging with" << settings.mergeMethod;

  // Merge
  cv::Mat merged;
  if (settings.mergeMethod.toLower() == "debevec") {
    merged = mergeDebevec(aligned);
    result.usedMethod = "debevec";
  } else {
    merged = mergeMertens(aligned);
    result.usedMethod = "mertens";
  }

  // Apply adjustments
  merged = applyAdjustments(merged, settings);

  // Save
  QDir dir;
  dir.mkpath(outputDir);

  QString suffix = settings.outputFormat.toLower();
  if (suffix != "jpg" && suffix != "jpeg" && suffix != "png" &&
      suffix != "tiff")
    suffix = "jpg";

  QString outPath = outputDir + "/" + outputName + "." + suffix;

  if (saveImage(merged, outPath, settings)) {
    result.outputPath = outPath;
    result.mergedCount = (int)images.size();
    result.success = true;
    qDebug() << "[HdrEngine] Saved:" << outPath;
  } else {
    result.error = "Failed to save output image";
  }

  return result;
}

HdrJobResult HdrEngine::adjustImage(const QString &inputPath,
                                    const HdrSettings &settings,
                                    const QString &outputDir,
                                    const QString &outputName) {
  HdrJobResult result;

  cv::Mat img = loadImage(inputPath);
  if (img.empty()) {
    result.error = "Could not load image: " + inputPath;
    return result;
  }

  img = applyAdjustments(img, settings);

  QDir dir;
  dir.mkpath(outputDir);

  QString suffix = settings.outputFormat.toLower();
  if (suffix != "jpg" && suffix != "jpeg" && suffix != "png" &&
      suffix != "tiff")
    suffix = "jpg";

  QString outPath = outputDir + "/" + outputName + "." + suffix;

  if (saveImage(img, outPath, settings)) {
    result.outputPath = outPath;
    result.mergedCount = 1;
    result.usedMethod = "adjust";
    result.success = true;
  } else {
    result.error = "Failed to save output image";
  }

  return result;
}

QImage HdrEngine::preview(const cv::Mat &source, const HdrSettings &settings,
                          bool fastPreview) {
  if (source.empty())
    return {};

  cv::Mat adjusted = applyAdjustments(source, settings, true, fastPreview);
  return matToQImage(adjusted);
}

cv::Mat HdrEngine::generatePreview(const cv::Mat &source, int maxSize) {
  if (source.empty())
    return {};

  int w = source.cols;
  int h = source.rows;

  if (w <= maxSize && h <= maxSize)
    return source.clone();

  double scale = std::min((double)maxSize / w, (double)maxSize / h);
  cv::Mat resized;
  cv::resize(source, resized, cv::Size(), scale, scale, cv::INTER_AREA);

  return resized;
}

cv::Mat HdrEngine::getClippingMask(const cv::Mat &img) {
  if (img.empty())
    return {};

  cv::Mat mask = cv::Mat::zeros(img.size(), CV_32FC3);
  const float hiThresh = 0.995f;
  const float loThresh = 0.005f;

  int rows = img.rows, cols = img.cols;
  for (int y = 0; y < rows; ++y) {
    const float *srcPtr = img.ptr<float>(y);
    float *maskPtr = mask.ptr<float>(y);
    for (int x = 0; x < cols; ++x) {
      float r = srcPtr[x * 3 + 0];
      float g = srcPtr[x * 3 + 1];
      float b = srcPtr[x * 3 + 2];

      // Overexposed: any channel clipped high → red overlay
      if (r >= hiThresh && g >= hiThresh && b >= hiThresh) {
        maskPtr[x * 3 + 0] = 1.0f; // R
        maskPtr[x * 3 + 1] = 0.0f;
        maskPtr[x * 3 + 2] = 0.0f;
      }
      // Underexposed: all channels near black → blue overlay
      else if (r <= loThresh && g <= loThresh && b <= loThresh) {
        maskPtr[x * 3 + 0] = 0.0f;
        maskPtr[x * 3 + 1] = 0.3f;
        maskPtr[x * 3 + 2] = 1.0f; // B
      }
    }
  }

  return mask;
}

// ═══════════════════════════════════════════════════════
//  Bracket Group Detection
// ═══════════════════════════════════════════════════════

qint64 HdrEngine::getImageTimestamp(const QString &path) {
  // For RAW files: use LibRaw to get EXIF timestamp
  if (isRawFile(path)) {
    LibRaw raw;
    if (raw.open_file(path.toStdString().c_str()) == LIBRAW_SUCCESS) {
      qint64 ts = (qint64)raw.imgdata.other.timestamp;
      raw.recycle();
      if (ts > 0)
        return ts;
    }
  }

  // Fallback: file modification time (works for JPEG/PNG)
  QFileInfo fi(path);
  return fi.lastModified().toSecsSinceEpoch();
}

QList<QStringList> HdrEngine::groupBrackets(const QStringList &paths,
                                            int gapSeconds) {
  if (paths.size() <= 1)
    return {paths};

  // Build (timestamp, path) pairs
  QList<QPair<qint64, QString>> items;
  for (const auto &p : paths)
    items.append({getImageTimestamp(p), p});

  // Sort by timestamp
  std::sort(items.begin(), items.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  // Group by gap
  QList<QStringList> groups;
  QStringList current;
  current.append(items[0].second);

  for (int i = 1; i < items.size(); ++i) {
    qint64 gap = items[i].first - items[i - 1].first;
    if (gap > gapSeconds) {
      groups.append(current);
      current.clear();
    }
    current.append(items[i].second);
  }
  if (!current.isEmpty())
    groups.append(current);

  qDebug() << "[HdrEngine] Detected" << groups.size() << "bracket group(s) from"
           << paths.size() << "images";
  for (int i = 0; i < groups.size(); ++i)
    qDebug() << "  Group" << (i + 1) << ":" << groups[i].size() << "images";

  return groups;
}
