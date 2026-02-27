#pragma once

#include <QImage>
#include <QString>
#include <opencv2/core.hpp>

// ── HDR Processing Settings ─────────────────────────
struct HdrSettings {
  // Light panel
  double exposure = 1.0;   // 0.1 .. 5.0
  double contrast = 1.0;   // 0.5 .. 2.0
  double clarity = 0.0;    // -1.0 .. 1.0  (local mid-tone contrast)
  double texture = 0.0;    // -1.0 .. 1.0  (micro-contrast / fine detail)
  double highlights = 0.0; // -1.0 .. 1.0
  double shadows = 0.0;    // -1.0 .. 1.0
  double whites = 0.0;     // -1.0 .. 1.0
  double blacks = 0.0;     // -1.0 .. 1.0

  // Color panel
  double temperature = 0.0; // -100 .. 100
  double tint = 0.0;        // -100 .. 100
  double vibrance = 0.0;    // -1.0 .. 1.0
  double saturation = 1.0;  // 0.0 .. 2.0

  // HSL / Color Mixer — 8 hue channels:
  //   0=Red, 1=Orange, 2=Yellow, 3=Green, 4=Aqua, 5=Blue, 6=Purple, 7=Magenta
  double hslHue[8] = {}; // -30 .. +30 degrees shift
  double hslSat[8] = {}; // -1.0 .. 1.0
  double hslLum[8] = {}; // -1.0 .. 1.0

  // Color Grading (3-way split toning)
  double cgShadowHue = 0.0;    // 0..360
  double cgShadowSat = 0.0;    // 0..1
  double cgMidtoneHue = 0.0;   // 0..360
  double cgMidtoneSat = 0.0;   // 0..1
  double cgHighlightHue = 0.0; // 0..360
  double cgHighlightSat = 0.0; // 0..1

  // Effects panel
  double dehaze = 0.0;   // -1.0 .. 1.0
  double vignette = 0.0; // -1.0 .. 1.0

  // Detail panel
  double sharpening = 0.0;     // 0 .. 100
  double noiseReduction = 0.0; // 0 .. 100

  // Fusion
  QString mergeMethod = "mertens"; // "mertens" or "debevec"
  QString outputFormat = "jpg";    // "jpg", "png", "tiff"
  int jpegQuality = 92;
};

// ── HDR Job Result ──────────────────────────────────
struct HdrJobResult {
  QString outputPath;
  int mergedCount = 0;
  QString usedMethod;
  bool success = false;
  QString error;
};

// ── HDR Engine ──────────────────────────────────────
class HdrEngine {
public:
  // Core pipeline: load → align → merge → adjust → save
  static HdrJobResult mergeBracketGroup(const QStringList &inputPaths,
                                        const HdrSettings &settings,
                                        const QString &outputDir,
                                        const QString &outputName);

  // Adjust an existing image (no fusion)
  static HdrJobResult adjustImage(const QString &inputPath,
                                  const HdrSettings &settings,
                                  const QString &outputDir,
                                  const QString &outputName);

  // Live preview (returns QImage, no file save)
  static QImage preview(const cv::Mat &source, const HdrSettings &settings,
                        bool fastPreview = false);

  // Generate downscaled preview for UI performance
  static cv::Mat generatePreview(const cv::Mat &source, int maxSize);

  // Sub-steps (all public for HdrStudioPage access)
  static cv::Mat loadImage(const QString &path);
  static cv::Mat loadImage(const QString &path, int maxSize);
  static std::vector<cv::Mat> alignBrackets(const std::vector<cv::Mat> &images);
  static cv::Mat mergeMertens(const std::vector<cv::Mat> &images);
  static cv::Mat mergeDebevec(const std::vector<cv::Mat> &images);
  static cv::Mat applyAdjustments(const cv::Mat &img,
                                  const HdrSettings &settings,
                                  bool isPreview = false,
                                  bool fastPreview = false);
  static bool saveImage(const cv::Mat &img, const QString &path,
                        const HdrSettings &settings);
  static QImage matToQImage(const cv::Mat &mat);

  // Clipping indicator: returns overlay (red=overexposed, blue=underexposed)
  static cv::Mat getClippingMask(const cv::Mat &img);

  // Bracket group detection
  static qint64 getImageTimestamp(const QString &path);
  static QList<QStringList> groupBrackets(const QStringList &paths,
                                          int gapSeconds = 5);
};
