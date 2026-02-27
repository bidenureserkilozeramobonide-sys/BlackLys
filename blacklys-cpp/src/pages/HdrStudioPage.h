#pragma once

#include "../engine/HdrEngine.h"
#include "../widgets/ColorWheelWidget.h"
#include "../widgets/HslWheelWidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStack>
#include <QTimer>
#include <QWidget>
#include <atomic>
#include <chrono>
#include <future>
#include <opencv2/core.hpp>

class QListWidget;
class QToolButton;

// ── Histogram Widget ──────────────────────────────
class HistogramWidget : public QWidget {
  Q_OBJECT
public:
  explicit HistogramWidget(QWidget *parent = nullptr);
  void updateHistogram(const QImage &image);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  std::vector<int> m_histR, m_histG, m_histB, m_histL;
  int m_maxVal = 1;
};

// ── HDR Studio Page ───────────────────────────────
class HdrStudioPage : public QWidget {
  Q_OBJECT

public:
  explicit HdrStudioPage(QWidget *parent = nullptr);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
  void onLoadImages();
  void onExport();
  void onSliderChanged();
  void onResetAll();
  void onTopazAI();
  void onBatchProcess();
  void onZoomIn();
  void onZoomOut();
  void onZoomFit();
  void onRemoveImage(int index);
  void onRotateCW();
  void onRotateCCW();
  void onToggleSplitView();
  void onFlipH();
  void onFlipV();
  void onAutoEnhance();
  void onToggleGrid();
  void onToggleClipping();
  void onWbPickerToggle();
  void onSavePreset();
  void onLoadPreset();

private:
  void setupUi();
  QWidget *createSliderRow(const QString &label, QSlider *&slider, int min,
                           int max, int defaultVal, double scale = 1.0);
  QWidget *createSectionHeader(const QString &title, const QString &color);
  void updatePreview();
  void updateInfoPanel();
  void loadFromPaths(const QStringList &paths);

  // State
  cv::Mat m_sourceImage;
  QStringList m_loadedPaths;
  HdrSettings m_settings;
  bool m_merged = false;
  double m_zoomFactor = 0.0; // 0 = fit
  QTimer *m_previewTimer = nullptr;
  std::atomic<int> m_renderVersion{0}; // for cancelling stale renders
  bool m_draggingSlider = false;       // true while any slider is pressed
  cv::Mat m_fastPreviewImage;          // 400px proxy for dragging

  // Bracket groups (auto-detected)
  QList<QStringList> m_bracketGroups;
  QList<cv::Mat> m_groupPreviews; // low-res merged cache per group
  int m_currentGroup = 0;
  void loadGroup(int index);

  // Progressive loading: low-res first, full-res in background
  bool m_fullResLoading = false;
  std::future<cv::Mat> m_fullResFuture;
  QTimer *m_fullResCheckTimer = nullptr;
  QLabel *m_fullResBanner = nullptr;
  QLabel *m_fullResBannerLabel = nullptr;
  QProgressBar *m_fullResProgress = nullptr;
  void onFullResReady();
  void showFullResBanner(const QString &text, bool showProgress = true);
  void hideFullResBanner();
  void startFullResPipeline();
  void loadNextFullRes();
  int m_fullResGroupIndex = 0;

  // Split-view (Before/After)
  bool m_splitViewActive = false;
  double m_splitPos = 0.5; // 0.0 = full original, 1.0 = full edited
  bool m_draggingSplit = false;
  QPushButton *m_splitViewBtn = nullptr;

  // Grid overlay
  bool m_gridOverlay = false;

  // Zoom: QGraphicsView handles zoom/pan natively
  void applyZoom();
  double m_targetZoom = 0;       // animated zoom target
  QTimer *m_zoomTimer = nullptr; // smooth zoom animation timer
  void animateZoom();

  // UI — Viewer (QGraphicsView-based)
  QGraphicsView *m_graphicsView = nullptr;
  QGraphicsScene *m_graphicsScene = nullptr;
  QGraphicsPixmapItem *m_pixmapItem = nullptr;
  QLabel *m_imageLabel = nullptr;      // kept for overlay compatibility
  QScrollArea *m_scrollArea = nullptr; // kept for overlay compatibility
  QWidget *m_header = nullptr;
  QWidget *m_emptyOverlay = nullptr;

  // UI - Loading Overlay
  QWidget *m_loadingOverlay = nullptr;
  QProgressBar *m_progressBar = nullptr;
  QLabel *m_loadingLabel = nullptr;

  // UI - Info Bar & Zoom
  QLabel *m_infoBar = nullptr;
  QLabel *m_zoomLabel = nullptr;

  // UI — Histogram
  HistogramWidget *m_histogram = nullptr;

  // Sliders
  QSlider *m_exposureSlider = nullptr;
  QSlider *m_contrastSlider = nullptr;
  QSlider *m_claritySlider = nullptr;
  QSlider *m_textureSlider = nullptr;
  QSlider *m_highlightsSlider = nullptr;
  QSlider *m_shadowsSlider = nullptr;
  QSlider *m_whitesSlider = nullptr;
  QSlider *m_blacksSlider = nullptr;
  QSlider *m_temperatureSlider = nullptr;
  QSlider *m_tintSlider = nullptr;
  QSlider *m_vibranceSlider = nullptr;
  QSlider *m_saturationSlider = nullptr;
  QSlider *m_dehazeSlider = nullptr;
  QSlider *m_vignetteSlider = nullptr;
  QSlider *m_sharpeningSlider = nullptr;
  QSlider *m_noiseSlider = nullptr;

  // HSL / Color Mixer — interactive wheel widget
  HslWheelWidget *m_hslWheel = nullptr;

  // Color Grading sliders (3-way: Shadow/Midtone/Highlight × Hue/Sat)
  ColorWheelWidget *m_cgShadowWheel = nullptr;
  ColorWheelWidget *m_cgMidtoneWheel = nullptr;
  ColorWheelWidget *m_cgHighlightWheel = nullptr;

  // Clipping indicator
  bool m_showClipping = false;
  QPushButton *m_clippingBtn = nullptr;

  // White Balance Picker
  bool m_wbPickerActive = false;
  QPushButton *m_wbPickerBtn = nullptr;

  QComboBox *m_methodCombo = nullptr;
  QComboBox *m_formatCombo = nullptr;
  QProcess *m_topazProcess = nullptr;
  QString m_topazOutputPath;
  void onTopazFinished(int exitCode, QProcess::ExitStatus status);
  void onTopazOutput();

  // Topaz AI sliders
  QSlider *m_topazDenoiseSlider = nullptr;
  QSlider *m_topazSharpenSlider = nullptr;
  QComboBox *m_topazUpscaleCombo = nullptr;
  QWidget *m_topazSettingsPanel = nullptr;

  // Bracket grouping
  QSpinBox *m_bracketCountSpin = nullptr;

  // UI - Group Navigation Bar
  QWidget *m_groupBar = nullptr;
  QPushButton *m_prevGroupBtn = nullptr;
  QPushButton *m_nextGroupBtn = nullptr;
  QLabel *m_groupLabel = nullptr;
  void updateGroupBar();

  // Optimized Preview
  cv::Mat m_previewImage;
  bool m_hdPreview = false; // false = 800px proxy, true = full-res
  QPushButton *m_hdToggleBtn = nullptr;
  void updatePreviewSource();

  // Undo/Redo
  QStack<HdrSettings> m_undoStack;
  QStack<HdrSettings> m_redoStack;
  void pushUndo();
  void undo();
  void redo();

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
};
