#include "HdrStudioPage.h"
#include "../app/MainWindow.h"
#include "../database/Database.h"
#include "../widgets/PageHeader.h"
#include "../widgets/ToastWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

#include <future>
#include <thread>

// ════════════════════════════════════════════════
//  HistogramWidget
// ════════════════════════════════════════════════

HistogramWidget::HistogramWidget(QWidget *parent) : QWidget(parent) {
  setFixedHeight(40);
  setStyleSheet("background: rgba(255,255,255,0.02); border-radius: 8px;");
  m_histR.resize(256, 0);
  m_histG.resize(256, 0);
  m_histB.resize(256, 0);
  m_histL.resize(256, 0);
}

void HistogramWidget::updateHistogram(const QImage &image) {
  std::fill(m_histR.begin(), m_histR.end(), 0);
  std::fill(m_histG.begin(), m_histG.end(), 0);
  std::fill(m_histB.begin(), m_histB.end(), 0);
  std::fill(m_histL.begin(), m_histL.end(), 0);

  for (int y = 0; y < image.height(); y += 4) {
    for (int x = 0; x < image.width(); x += 4) {
      QRgb px = image.pixel(x, y);
      int r = qRed(px), g = qGreen(px), b = qBlue(px);
      m_histR[r]++;
      m_histG[g]++;
      m_histB[b]++;
      int lum = (r * 299 + g * 587 + b * 114) / 1000;
      m_histL[qBound(0, lum, 255)]++;
    }
  }

  m_maxVal = 1;
  for (int i = 0; i < 256; ++i) {
    m_maxVal = qMax(m_maxVal, m_histR[i]);
    m_maxVal = qMax(m_maxVal, m_histG[i]);
    m_maxVal = qMax(m_maxVal, m_histB[i]);
  }
  update();
}

void HistogramWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  int w = width() - 8;
  int h = height() - 8;
  int x0 = 4, y0 = 4;

  p.fillRect(rect(), QColor("#0f0f11"));
  p.setPen(Qt::NoPen);

  auto drawChannel = [&](const std::vector<int> &hist, QColor color) {
    color.setAlpha(50);
    QPainterPath path;
    path.moveTo(x0, y0 + h);
    for (int i = 0; i < 256; ++i) {
      double fx = x0 + (double)i / 255.0 * w;
      double fy = y0 + h - (double)hist[i] / m_maxVal * h;
      path.lineTo(fx, fy);
    }
    path.lineTo(x0 + w, y0 + h);
    path.closeSubpath();
    p.setBrush(color);
    p.drawPath(path);

    color.setAlpha(120);
    p.setPen(QPen(color, 1));
    for (int i = 0; i < 255; ++i) {
      double x1 = x0 + (double)i / 255.0 * w;
      double y1 = y0 + h - (double)hist[i] / m_maxVal * h;
      double x2 = x0 + (double)(i + 1) / 255.0 * w;
      double y2 = y0 + h - (double)hist[i + 1] / m_maxVal * h;
      p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }
    p.setPen(Qt::NoPen);
  };

  drawChannel(m_histR, QColor("#ef4444"));
  drawChannel(m_histG, QColor("#22c55e"));
  drawChannel(m_histB, QColor("#3b82f6"));

  QColor lumColor("#a1a1aa");
  lumColor.setAlpha(30);
  QPainterPath lumPath;
  lumPath.moveTo(x0, y0 + h);
  for (int i = 0; i < 256; ++i) {
    double fx = x0 + (double)i / 255.0 * w;
    double fy = y0 + h - (double)m_histL[i] / m_maxVal * h;
    lumPath.lineTo(fx, fy);
  }
  lumPath.lineTo(x0 + w, y0 + h);
  lumPath.closeSubpath();
  p.setBrush(lumColor);
  p.drawPath(lumPath);
}

// ════════════════════════════════════════════════
//  HdrStudioPage
// ════════════════════════════════════════════════

HdrStudioPage::HdrStudioPage(QWidget *parent) : QWidget(parent) {
  setObjectName("page");
  setAcceptDrops(true);

  // Debounce timer: wait 40ms after last slider change before re-rendering
  m_previewTimer = new QTimer(this);
  m_previewTimer->setSingleShot(true);
  m_previewTimer->setInterval(200);
  connect(m_previewTimer, &QTimer::timeout, this, [this]() {
    // Deferred histogram update — only when user pauses slider drag
    const cv::Mat &source =
        m_previewImage.empty() ? m_sourceImage : m_previewImage;
    if (!source.empty()) {
      QImage img = HdrEngine::preview(source, m_settings);
      if (!img.isNull())
        m_histogram->updateHistogram(img);
    }
  });

  setupUi();
}

// ── Event filter: double-click slider → reset, viewport resize → overlay,
//    Ctrl+Scroll on viewport → zoom ──
bool HdrStudioPage::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonDblClick) {
    auto *slider = qobject_cast<QSlider *>(obj);
    if (slider) {
      pushUndo();
      slider->setValue(0);
      return true;
    }
  }
  // Block scroll wheel on all interactive widgets so the sidebar scrolls freely
  if (event->type() == QEvent::Wheel) {
    if (qobject_cast<QSlider *>(obj) || qobject_cast<QComboBox *>(obj) ||
        qobject_cast<QSpinBox *>(obj)) {
      return true; // eat the event
    }
  }
  // Keep overlay centered when viewport resizes
  if (event->type() == QEvent::Resize && m_graphicsView &&
      obj == m_graphicsView->viewport()) {
    if (m_emptyOverlay)
      m_emptyOverlay->setGeometry(m_graphicsView->viewport()->rect());
    if (m_loadingOverlay)
      m_loadingOverlay->setGeometry(m_graphicsView->viewport()->rect());
  }

  // Intercept Ctrl+Scroll on viewport for zoom (before QGraphicsView eats it)
  if (event->type() == QEvent::Wheel && m_graphicsView &&
      obj == m_graphicsView->viewport()) {
    auto *we = static_cast<QWheelEvent *>(event);
    if (we->modifiers() & Qt::ControlModifier) {
      if (m_pixmapItem && !m_pixmapItem->pixmap().isNull()) {
        double delta = we->angleDelta().y();
        double factor = qPow(1.001, delta); // ~12% per standard scroll tick

        if (m_zoomFactor <= 0) {
          QSizeF imgSz = m_pixmapItem->pixmap().size();
          QSizeF viewSz = m_graphicsView->viewport()->size();
          m_zoomFactor = qMin(viewSz.width() / imgSz.width(),
                              viewSz.height() / imgSz.height());
        }
        // Set animated target — timer will smoothly interpolate
        double nextTarget = (m_targetZoom > 0 ? m_targetZoom : m_zoomFactor);
        m_targetZoom = qBound(0.05, nextTarget * factor, 20.0);
        if (!m_zoomTimer) {
          m_zoomTimer = new QTimer(this);
          m_zoomTimer->setInterval(16); // ~60fps
          connect(m_zoomTimer, &QTimer::timeout, this,
                  &HdrStudioPage::animateZoom);
        }
        if (!m_zoomTimer->isActive())
          m_zoomTimer->start();
      }
      return true; // consumed
    }
  }
  // Intercept arrow keys for bracket group switching (before QGraphicsView eats
  // them)
  if (event->type() == QEvent::KeyPress) {
    auto *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right) {
      // Debug: always show toast
      ToastWidget::show(this,
                        QString("Arrow %1 | Groups: %2 | Current: %3")
                            .arg(ke->key() == Qt::Key_Left ? "LEFT" : "RIGHT")
                            .arg(m_bracketGroups.size())
                            .arg(m_currentGroup + 1),
                        ToastWidget::Info);

      if (m_bracketGroups.size() > 1) {
        if (ke->key() == Qt::Key_Left && m_currentGroup > 0) {
          loadGroup(m_currentGroup - 1);
          return true;
        }
        if (ke->key() == Qt::Key_Right &&
            m_currentGroup < m_bracketGroups.size() - 1) {
          loadGroup(m_currentGroup + 1);
          return true;
        }
      }
      return true; // consume arrow keys regardless
    }
  }

  // ── WB Picker: click on image to auto-adjust temperature/tint ──
  if (m_wbPickerActive && event->type() == QEvent::MouseButtonPress &&
      m_graphicsView && obj == m_graphicsView->viewport()) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton && !m_previewImage.empty()) {
      // Map viewport coords → scene coords → image pixel
      QPointF scenePos = m_graphicsView->mapToScene(me->pos());
      const cv::Mat &src =
          m_previewImage.empty() ? m_sourceImage : m_previewImage;
      int px = (int)scenePos.x();
      int py = (int)scenePos.y();
      if (px >= 0 && py >= 0 && px < src.cols && py < src.rows) {
        // Apply current adjustments to get the actual pixel color
        cv::Mat adjusted = HdrEngine::applyAdjustments(src, m_settings, true);
        const float *ptr = adjusted.ptr<float>(py);
        float r = ptr[px * 3 + 0];
        float g = ptr[px * 3 + 1];
        float b = ptr[px * 3 + 2];

        // Calculate temp/tint correction to make this pixel neutral
        // Neutral = R == G == B, so we compute offsets
        float avgRGB = (r + g + b) / 3.0f;
        if (avgRGB > 0.02f) {
          // Temperature: shift R vs B to equalize
          float tempCorr = (b - r) / avgRGB * 50.0f; // -100..100 range
          // Tint: shift G relative to R/B average
          float tintCorr = ((r + b) / 2.0f - g) / avgRGB * 50.0f;

          pushUndo();
          m_temperatureSlider->setValue(
              qBound(-100, m_temperatureSlider->value() + (int)tempCorr, 100));
          m_tintSlider->setValue(
              qBound(-100, m_tintSlider->value() + (int)tintCorr, 100));
        }
      }

      // Deactivate picker after use
      m_wbPickerActive = false;
      m_wbPickerBtn->setChecked(false);
      m_graphicsView->viewport()->setCursor(Qt::ArrowCursor);
      ToastWidget::show(this, "White balance set from clicked point",
                        ToastWidget::Success);
      return true;
    }
  }

  return QWidget::eventFilter(obj, event);
}

// ── Slider Row Factory ──
// All sliders: range -100..100, default 0, display -1.00..1.00
QWidget *HdrStudioPage::createSliderRow(const QString &label, QSlider *&slider,
                                        int min, int max, int defaultVal,
                                        double scale) {
  (void)min;
  (void)max;
  (void)defaultVal;
  (void)scale;

  auto *row = new QWidget(this);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 3, 0, 3);
  layout->setSpacing(8);

  auto *lbl = new QLabel(label, row);
  lbl->setFixedWidth(105);
  lbl->setStyleSheet(
      "color: #a1a1aa; font-size: 12px; background: transparent;");
  layout->addWidget(lbl);

  slider = new QSlider(Qt::Horizontal, row);
  slider->setRange(-100, 100);
  slider->setValue(0);
  slider->setFixedHeight(20);
  slider->installEventFilter(this);
  slider->setFocusPolicy(Qt::NoFocus);
  slider->setStyleSheet(
      "QSlider::groove:horizontal {"
      "  background: rgba(255,255,255,0.06); height: 4px; border-radius: 2px; }"
      "QSlider::handle:horizontal {"
      "  background: #f59e0b; width: 12px; height: 12px; margin: -4px 0;"
      "  border-radius: 6px; border: 2px solid #0f0f11; }"
      "QSlider::handle:horizontal:hover {"
      "  background: #fbbf24; width: 14px; height: 14px; margin: -5px 0;"
      "  border-radius: 7px; }"
      "QSlider::sub-page:horizontal {"
      "  background: rgba(245,158,11,0.3); border-radius: 2px; }");
  connect(slider, &QSlider::sliderPressed, this,
          [this]() { m_draggingSlider = true; });
  connect(slider, &QSlider::sliderReleased, this, [this]() {
    m_draggingSlider = false;
    updatePreview(); // full-quality re-render on release
    m_previewTimer->start();
  });
  layout->addWidget(slider, 1);

  auto *valLbl = new QLabel("0.00", row);
  valLbl->setFixedWidth(58);
  valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  valLbl->setStyleSheet(
      "color: #71717a; font-size: 11px; font-family: 'Consolas'; "
      "background: transparent;");
  layout->addWidget(valLbl);

  connect(slider, &QSlider::valueChanged, [valLbl](int v) {
    valLbl->setText(QString::number(v * 0.01, 'f', 2));
  });
  connect(slider, &QSlider::valueChanged, this,
          &HdrStudioPage::onSliderChanged);
  connect(slider, &QSlider::sliderPressed, this, &HdrStudioPage::pushUndo);

  return row;
}

QWidget *HdrStudioPage::createSectionHeader(const QString &title,
                                            const QString &color) {
  // Create a wrapper that contains the header button + collapsible content
  auto *wrapper = new QWidget(this);
  auto *wrapperLayout = new QVBoxLayout(wrapper);
  wrapperLayout->setContentsMargins(0, 0, 0, 0);
  wrapperLayout->setSpacing(0);

  auto *btn =
      new QPushButton(QString::fromUtf8("\u25BE") + "  " + title, wrapper);
  btn->setFlat(true);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setFixedHeight(28);
  btn->setStyleSheet(
      QString("QPushButton { color: %1; font-size: 11px; font-weight: 700; "
              "letter-spacing: 2px; padding: 6px 0 4px 0; text-align: left; "
              "background: transparent; border: none; "
              "border-bottom: 1px solid rgba(255,255,255,0.04); }"
              "QPushButton:hover { color: %1; "
              "background: rgba(255,255,255,0.02); }")
          .arg(color));

  auto *content = new QWidget(wrapper);
  auto *contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(2);

  wrapperLayout->addWidget(btn);
  wrapperLayout->addWidget(content);

  // Toggle collapse on click
  connect(btn, &QPushButton::clicked, [btn, content, title]() {
    bool visible = !content->isVisible();
    content->setVisible(visible);
    btn->setText(
        (visible ? QString::fromUtf8("\u25BE") : QString::fromUtf8("\u25B8")) +
        "  " + title);
  });

  // Store content widget in the button's property so setupUi can retrieve it
  btn->setProperty("contentWidget",
                   QVariant::fromValue(static_cast<QObject *>(content)));

  return wrapper;
}

// ════════════════════════════════════════════════
//  Drag & Drop
// ════════════════════════════════════════════════

void HdrStudioPage::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    bool hasImage = false;
    for (const auto &url : event->mimeData()->urls()) {
      QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
      if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "tiff" ||
          ext == "tif" || ext == "bmp" || ext == "cr2" || ext == "cr3" ||
          ext == "nef" || ext == "arw" || ext == "dng" || ext == "orf" ||
          ext == "raf" || ext == "rw2" || ext == "raw") {
        hasImage = true;
        break;
      }
    }
    if (hasImage)
      event->acceptProposedAction();
  }
}

void HdrStudioPage::dropEvent(QDropEvent *event) {
  QStringList paths;
  for (const auto &url : event->mimeData()->urls()) {
    QString path = url.toLocalFile();
    if (!path.isEmpty())
      paths << path;
  }
  if (!paths.isEmpty())
    loadFromPaths(paths);
}

// ════════════════════════════════════════════════
//  Setup UI
// ════════════════════════════════════════════════

void HdrStudioPage::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setObjectName("studioSplitter");
  splitter->setHandleWidth(1);
  splitter->setStyleSheet(
      "QSplitter::handle { background: rgba(255,255,255,0.06); }");

  // ══════════════════════════════════════════════════
  //  LEFT: Image Viewer
  // ══════════════════════════════════════════════════
  auto *viewerWidget = new QWidget(splitter);
  viewerWidget->setStyleSheet("background: #09090b;");
  auto *viewerLayout = new QVBoxLayout(viewerWidget);
  viewerLayout->setContentsMargins(0, 0, 0, 0);
  viewerLayout->setSpacing(0);

  // ── Main Image Viewer (QGraphicsView — optimized for smooth pan) ──
  m_graphicsScene = new QGraphicsScene(this);
  m_pixmapItem = m_graphicsScene->addPixmap(QPixmap());
  m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);

  m_graphicsView = new QGraphicsView(m_graphicsScene, viewerWidget);
  m_graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
  m_graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
  m_graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  m_graphicsView->setResizeAnchor(QGraphicsView::AnchorViewCenter);
  m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_graphicsView->setStyleSheet(
      "QGraphicsView { border: none; background: #09090b; }");
  m_graphicsView->setMinimumSize(400, 300);
  m_graphicsView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  viewerLayout->addWidget(m_graphicsView, 1);

  // Compatibility: keep m_scrollArea as a hidden container for overlays
  m_scrollArea = new QScrollArea(viewerWidget);
  m_scrollArea->setVisible(false);

  // m_imageLabel kept for info overlay reference only
  m_imageLabel = new QLabel();

  // ── Empty State Overlay ──
  m_emptyOverlay = new QWidget(m_graphicsView->viewport());
  m_emptyOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  auto *overlayLayout = new QVBoxLayout(m_emptyOverlay);
  overlayLayout->setAlignment(Qt::AlignCenter);
  overlayLayout->setSpacing(16);

  // Camera icon
  auto *iconLabel = new QLabel(QString(QChar(0xE722)), m_emptyOverlay);
  QFont iconFont("Segoe MDL2 Assets", 48);
  iconLabel->setFont(iconFont);
  iconLabel->setAlignment(Qt::AlignCenter);
  iconLabel->setStyleSheet("color: #27272a; background: transparent;");
  overlayLayout->addWidget(iconLabel, 0, Qt::AlignCenter);

  auto *titleLabel = new QLabel("HDR Studio", m_emptyOverlay);
  titleLabel->setAlignment(Qt::AlignCenter);
  titleLabel->setStyleSheet(
      "color: #52525b; font-size: 18px; font-weight: 700; "
      "letter-spacing: 1px; background: transparent;");
  overlayLayout->addWidget(titleLabel, 0, Qt::AlignCenter);

  auto *importBtn = new QPushButton(
      QString("  %1  Import images").arg(QChar(0xE8B5)), m_emptyOverlay);
  importBtn->setFixedSize(260, 48);
  importBtn->setCursor(Qt::PointingHandCursor);
  importBtn->setStyleSheet(
      "QPushButton { background: rgba(245,158,11,0.1); color: #fbbf24; "
      "border: 2px dashed rgba(245,158,11,0.25); border-radius: 12px; "
      "font-size: 13px; font-weight: 600; }"
      "QPushButton:hover { background: rgba(245,158,11,0.18); color: "
      "#fde68a; border-color: rgba(245,158,11,0.4); }");
  connect(importBtn, &QPushButton::clicked, this, &HdrStudioPage::onLoadImages);
  overlayLayout->addWidget(importBtn, 0, Qt::AlignCenter);

  auto *hintLabel = new QLabel("Drag and drop your files here", m_emptyOverlay);
  hintLabel->setAlignment(Qt::AlignCenter);
  hintLabel->setStyleSheet(
      "color: #3f3f46; font-size: 11px; background: transparent;");
  overlayLayout->addWidget(hintLabel, 0, Qt::AlignCenter);

  auto *formatsLabel = new QLabel(
      "JPEG  ·  PNG  ·  TIFF  ·  CR2  ·  CR3  ·  NEF  ·  ARW  ·  DNG  ·  RAW",
      m_emptyOverlay);
  formatsLabel->setAlignment(Qt::AlignCenter);
  formatsLabel->setStyleSheet(
      "color: #27272a; font-size: 10px; letter-spacing: 1px; "
      "background: transparent;");
  overlayLayout->addWidget(formatsLabel, 0, Qt::AlignCenter);

  m_emptyOverlay->setStyleSheet("background: transparent;");
  m_graphicsView->viewport()->installEventFilter(this);
  m_graphicsView->installEventFilter(this);

  // Arrow shortcuts for bracket group switching (work regardless of focus)
  auto *prevGroupShortcut = new QShortcut(Qt::Key_Left, this);
  connect(prevGroupShortcut, &QShortcut::activated, this, [this]() {
    if (m_bracketGroups.size() > 1 && m_currentGroup > 0)
      loadGroup(m_currentGroup - 1);
  });
  auto *nextGroupShortcut = new QShortcut(Qt::Key_Right, this);
  connect(nextGroupShortcut, &QShortcut::activated, this, [this]() {
    if (m_bracketGroups.size() > 1 &&
        m_currentGroup < m_bracketGroups.size() - 1)
      loadGroup(m_currentGroup + 1);
  });
  m_emptyOverlay->setGeometry(m_graphicsView->viewport()->rect());
  m_emptyOverlay->show();

  // ── Loading Overlay ──
  m_loadingOverlay = new QWidget(m_graphicsView->viewport());
  m_loadingOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  auto *loadingLayout = new QVBoxLayout(m_loadingOverlay);
  loadingLayout->setAlignment(Qt::AlignCenter);
  loadingLayout->setSpacing(12);

  m_loadingLabel = new QLabel("Loading...", m_loadingOverlay);
  m_loadingLabel->setAlignment(Qt::AlignCenter);
  m_loadingLabel->setStyleSheet(
      "color: #a1a1aa; font-size: 14px; background: transparent;");
  loadingLayout->addWidget(m_loadingLabel, 0, Qt::AlignCenter);

  m_progressBar = new QProgressBar(m_loadingOverlay);
  m_progressBar->setFixedSize(300, 6);
  m_progressBar->setTextVisible(false);
  m_progressBar->setStyleSheet(
      "QProgressBar { background: rgba(255,255,255,0.05); border: none; "
      "border-radius: 3px; }"
      "QProgressBar::chunk { background: #f59e0b; border-radius: 3px; }");
  loadingLayout->addWidget(m_progressBar, 0, Qt::AlignCenter);

  m_loadingOverlay->setStyleSheet("background: rgba(9, 9, 11, 0.85);");
  m_loadingOverlay->setGeometry(m_graphicsView->viewport()->rect());
  m_loadingOverlay->hide();

  // ── Group Navigation Bar ──
  m_groupBar = new QWidget(viewerWidget);
  m_groupBar->setFixedHeight(32);
  m_groupBar->setStyleSheet("background: rgba(15,15,17,0.92);"
                            "border-top: 1px solid rgba(255,255,255,0.04);");
  auto *groupBarLayout = new QHBoxLayout(m_groupBar);
  groupBarLayout->setContentsMargins(12, 0, 12, 0);
  groupBarLayout->setSpacing(8);

  QString groupBtnStyle =
      "QPushButton { background: rgba(255,255,255,0.05); color: #a1a1aa; "
      "border: 1px solid rgba(255,255,255,0.08); border-radius: 4px; "
      "padding: 2px 10px; font-size: 12px; font-weight: 600; }"
      "QPushButton:hover { background: rgba(245,158,11,0.15); color: #f59e0b; "
      "border-color: rgba(245,158,11,0.3); }"
      "QPushButton:disabled { color: #3f3f46; background: transparent; "
      "border-color: rgba(255,255,255,0.03); }";

  m_prevGroupBtn = new QPushButton(QString(QChar(0x25C0)), m_groupBar);
  m_prevGroupBtn->setFixedSize(28, 24);
  m_prevGroupBtn->setCursor(Qt::PointingHandCursor);
  m_prevGroupBtn->setStyleSheet(groupBtnStyle);
  m_prevGroupBtn->setToolTip("Previous group (←)");
  connect(m_prevGroupBtn, &QPushButton::clicked, this, [this]() {
    if (m_currentGroup > 0)
      loadGroup(m_currentGroup - 1);
  });

  m_groupLabel = new QLabel("Group 1 / 1", m_groupBar);
  m_groupLabel->setAlignment(Qt::AlignCenter);
  m_groupLabel->setStyleSheet(
      "QLabel { color: #f59e0b; font-size: 11px; font-weight: 600; "
      "letter-spacing: 0.5px; background: transparent; border: none; }");

  m_nextGroupBtn = new QPushButton(QString(QChar(0x25B6)), m_groupBar);
  m_nextGroupBtn->setFixedSize(28, 24);
  m_nextGroupBtn->setCursor(Qt::PointingHandCursor);
  m_nextGroupBtn->setStyleSheet(groupBtnStyle);
  m_nextGroupBtn->setToolTip("Next group (→)");
  connect(m_nextGroupBtn, &QPushButton::clicked, this, [this]() {
    if (m_currentGroup < m_bracketGroups.size() - 1)
      loadGroup(m_currentGroup + 1);
  });

  groupBarLayout->addStretch();
  groupBarLayout->addWidget(m_prevGroupBtn);
  groupBarLayout->addWidget(m_groupLabel);
  groupBarLayout->addWidget(m_nextGroupBtn);
  groupBarLayout->addStretch();

  m_groupBar->hide(); // hidden until multiple groups exist
  viewerLayout->addWidget(m_groupBar);

  // ── Info Bar + HD Toggle ──
  QWidget *infoBarContainer = new QWidget(viewerWidget);
  infoBarContainer->setFixedHeight(24);
  infoBarContainer->setStyleSheet(
      "QWidget { background: rgba(15,15,17,0.9); "
      "border-top: 1px solid rgba(255,255,255,0.04); }");
  QHBoxLayout *infoLayout = new QHBoxLayout(infoBarContainer);
  infoLayout->setContentsMargins(0, 0, 0, 0);
  infoLayout->setSpacing(0);

  m_infoBar = new QLabel(infoBarContainer);
  m_infoBar->setAlignment(Qt::AlignCenter);
  m_infoBar->setStyleSheet(
      "QLabel { background: transparent; color: #52525b; "
      "font-size: 10px; font-family: 'Consolas', monospace; "
      "padding: 0 12px; }");
  m_infoBar->setText("No image loaded");

  m_hdToggleBtn = new QPushButton("SD", infoBarContainer);
  m_hdToggleBtn->setFixedSize(36, 20);
  m_hdToggleBtn->setToolTip("Toggle HD preview (full resolution)");
  m_hdToggleBtn->setStyleSheet(
      "QPushButton { background: rgba(40,40,45,0.9); color: #6b7280; "
      "border: 1px solid rgba(255,255,255,0.08); border-radius: 3px; "
      "font-size: 9px; font-weight: bold; font-family: 'Consolas'; "
      "padding: 0; }"
      "QPushButton:hover { background: rgba(60,60,65,0.9); color: #9ca3af; }"
      "QPushButton:checked { background: rgba(245,158,11,0.2); "
      "color: #f59e0b; border-color: rgba(245,158,11,0.4); }");
  m_hdToggleBtn->setCheckable(true);
  connect(m_hdToggleBtn, &QPushButton::toggled, this, [this](bool checked) {
    m_hdPreview = checked;
    m_hdToggleBtn->setText(checked ? "HD" : "SD");
    updatePreviewSource();
    updatePreview();
  });

  infoLayout->addWidget(m_infoBar, 1);

  // ── Clipping Toggle ──
  m_clippingBtn = new QPushButton("J", infoBarContainer);
  m_clippingBtn->setFixedSize(24, 20);
  m_clippingBtn->setToolTip("Toggle clipping indicators (J)");
  m_clippingBtn->setCheckable(true);
  m_clippingBtn->setStyleSheet(
      "QPushButton { background: rgba(40,40,45,0.9); color: #6b7280; "
      "border: 1px solid rgba(255,255,255,0.08); border-radius: 3px; "
      "font-size: 9px; font-weight: bold; font-family: 'Consolas'; "
      "padding: 0; }"
      "QPushButton:hover { background: rgba(60,60,65,0.9); color: #9ca3af; }"
      "QPushButton:checked { background: rgba(239,68,68,0.2); "
      "color: #ef4444; border-color: rgba(239,68,68,0.4); }");
  connect(m_clippingBtn, &QPushButton::toggled, this, [this](bool checked) {
    m_showClipping = checked;
    updatePreview();
  });
  infoLayout->addWidget(m_clippingBtn);
  infoLayout->addSpacing(2);

  // ── WB Picker Toggle ──
  m_wbPickerBtn = new QPushButton(QString(QChar(0x1F4A7)), infoBarContainer);
  m_wbPickerBtn->setFixedSize(24, 20);
  m_wbPickerBtn->setToolTip("White Balance Picker (click neutral gray)");
  m_wbPickerBtn->setCheckable(true);
  m_wbPickerBtn->setStyleSheet(
      "QPushButton { background: rgba(40,40,45,0.9); color: #6b7280; "
      "border: 1px solid rgba(255,255,255,0.08); border-radius: 3px; "
      "font-size: 10px; padding: 0; }"
      "QPushButton:hover { background: rgba(60,60,65,0.9); color: #9ca3af; }"
      "QPushButton:checked { background: rgba(59,130,246,0.2); "
      "color: #3b82f6; border-color: rgba(59,130,246,0.4); }");
  connect(m_wbPickerBtn, &QPushButton::toggled, this, [this](bool checked) {
    m_wbPickerActive = checked;
    m_graphicsView->viewport()->setCursor(checked ? Qt::CrossCursor
                                                  : Qt::ArrowCursor);
  });
  infoLayout->addWidget(m_wbPickerBtn);
  infoLayout->addSpacing(2);

  infoLayout->addWidget(m_hdToggleBtn);
  infoLayout->addSpacing(6);
  viewerLayout->addWidget(infoBarContainer);

  splitter->addWidget(viewerWidget);

  // ══════════════════════════════════════════════════
  //  RIGHT: Controls Panel
  // ══════════════════════════════════════════════════
  auto *controlsOuter = new QWidget(splitter);
  controlsOuter->setFixedWidth(280);
  controlsOuter->setStyleSheet("background: #0f0f11;");
  auto *controlsLayout = new QVBoxLayout(controlsOuter);
  controlsLayout->setContentsMargins(8, 6, 8, 6);
  controlsLayout->setSpacing(3);

  // Common button style — neutral, uniform for all
  QString btnStyle =
      "QPushButton { background: rgba(255,255,255,0.04); color: #a1a1aa; "
      "border: 1px solid rgba(255,255,255,0.06); border-radius: 6px; "
      "padding: 0 10px; font-size: 11px; font-weight: 500; }"
      "QPushButton:hover { background: rgba(255,255,255,0.08); color: "
      "#e4e4e7; }"
      "QPushButton:checked { background: rgba(255,255,255,0.10); color: "
      "#f4f4f5; }";

  // ── Fullscreen (top) ──
  QString topBtnStyle =
      "QPushButton { background: rgba(255,255,255,0.03); color: #71717a; "
      "border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; "
      "padding: 0 14px; font-size: 11px; font-weight: 500; }"
      "QPushButton:hover { background: rgba(255,255,255,0.07); color: #d4d4d8; "
      "border-color: rgba(245,158,11,0.3); }";

  auto *fullscreenBtn =
      new QPushButton(QString(QChar(0x26F6)) + "  Fullscreen", controlsOuter);
  fullscreenBtn->setFixedHeight(30);
  fullscreenBtn->setCursor(Qt::PointingHandCursor);
  fullscreenBtn->setStyleSheet(topBtnStyle);
  connect(fullscreenBtn, &QPushButton::clicked, this, [this, fullscreenBtn]() {
    QWidget *w = this->window();
    auto *mw = qobject_cast<MainWindow *>(w);
    if (mw) {
      mw->toggleFullscreen();
      bool fs = mw->isEditingFullscreen();
      fullscreenBtn->setText(fs ? QString(QChar(0x2716)) + "  Exit Fullscreen"
                                : QString(QChar(0x26F6)) + "  Fullscreen");
    }
  });
  controlsLayout->addWidget(fullscreenBtn);

  // ── Presets ──
  auto *presetRow = new QWidget(controlsOuter);
  auto *presetLayout = new QHBoxLayout(presetRow);
  presetLayout->setContentsMargins(0, 2, 0, 2);
  presetLayout->setSpacing(4);
  auto *savePresetBtn = new QPushButton("Save Preset", presetRow);
  savePresetBtn->setFixedHeight(26);
  savePresetBtn->setCursor(Qt::PointingHandCursor);
  savePresetBtn->setStyleSheet(topBtnStyle);
  connect(savePresetBtn, &QPushButton::clicked, this,
          &HdrStudioPage::onSavePreset);
  auto *loadPresetBtn = new QPushButton("Load Preset", presetRow);
  loadPresetBtn->setFixedHeight(26);
  loadPresetBtn->setCursor(Qt::PointingHandCursor);
  loadPresetBtn->setStyleSheet(topBtnStyle);
  connect(loadPresetBtn, &QPushButton::clicked, this,
          &HdrStudioPage::onLoadPreset);
  presetLayout->addWidget(savePresetBtn);
  presetLayout->addWidget(loadPresetBtn);
  controlsLayout->addWidget(presetRow);

  // ── Brackets per group ──
  auto *bracketRow = new QWidget(controlsOuter);
  auto *bracketRowLayout = new QHBoxLayout(bracketRow);
  bracketRowLayout->setContentsMargins(4, 2, 4, 2);
  bracketRowLayout->setSpacing(6);
  auto *bracketLbl = new QLabel("Brackets/group", bracketRow);
  bracketLbl->setStyleSheet("color: #a1a1aa; font-size: 11px;");
  m_bracketCountSpin = new QSpinBox(bracketRow);
  m_bracketCountSpin->setRange(1, 20);
  m_bracketCountSpin->setValue(5);
  m_bracketCountSpin->installEventFilter(this);
  m_bracketCountSpin->setToolTip("Number of images per bracket group");
  m_bracketCountSpin->setStyleSheet(
      "QSpinBox { background: #1a1a2e; color: #f59e0b; border: 1px solid "
      "#3f3f46;"
      "  border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
      "QSpinBox::up-button, QSpinBox::down-button { width: 14px; }");
  bracketRowLayout->addWidget(bracketLbl);
  bracketRowLayout->addWidget(m_bracketCountSpin);
  controlsLayout->addWidget(bracketRow);

  // ── Topaz AI ──
  auto *topazHeader =
      new QLabel(QString(QChar(0x2728)) + "  Topaz AI", controlsOuter);
  topazHeader->setFixedHeight(30);
  topazHeader->setAlignment(Qt::AlignCenter);
  topazHeader->setStyleSheet(
      "QLabel { color: #f59e0b; font-size: 12px; font-weight: 600;"
      "  background: rgba(245, 158, 11, 0.08); border-radius: 6px;"
      "  border: 1px solid rgba(245, 158, 11, 0.15); }");
  controlsLayout->addWidget(topazHeader);

  // ── Topaz AI Settings Panel ──
  m_topazSettingsPanel = new QWidget(controlsOuter);
  m_topazSettingsPanel->setObjectName("topazPanel");
  auto *topazPanelLayout = new QVBoxLayout(m_topazSettingsPanel);
  topazPanelLayout->setContentsMargins(4, 6, 4, 6);
  topazPanelLayout->setSpacing(4);
  m_topazSettingsPanel->setStyleSheet(
      "#topazPanel { background: rgba(255,255,255,0.02); border-radius: 6px; "
      "}");

  auto makeTopazSlider = [&](const QString &label, QSlider *&slider,
                             int defaultVal) -> QWidget * {
    auto *row = new QWidget(m_topazSettingsPanel);
    auto *rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);
    auto *lbl = new QLabel(label, row);
    lbl->setFixedWidth(70);
    lbl->setStyleSheet("QLabel { color: #999; font-size: 11px;"
                       "  background: transparent; border: none; }");
    slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, 100);
    slider->setValue(defaultVal);
    slider->installEventFilter(this);
    slider->setFocusPolicy(Qt::NoFocus);
    slider->setStyleSheet(
        "QSlider::groove:horizontal { background: rgba(255,255,255,0.06);"
        "  height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #f59e0b;"
        "  width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }");
    auto *valLbl = new QLabel(QString::number(defaultVal), row);
    valLbl->setFixedWidth(24);
    valLbl->setAlignment(Qt::AlignRight);
    valLbl->setStyleSheet("QLabel { color: #f59e0b; font-size: 11px;"
                          "  font-weight: 600; background: transparent;"
                          "  border: none; }");
    connect(slider, &QSlider::valueChanged, valLbl,
            [valLbl](int v) { valLbl->setText(QString::number(v)); });
    rl->addWidget(lbl);
    rl->addWidget(slider, 1);
    rl->addWidget(valLbl);
    return row;
  };

  topazPanelLayout->addWidget(
      makeTopazSlider("Denoise", m_topazDenoiseSlider, 50));
  topazPanelLayout->addWidget(
      makeTopazSlider("Sharpen", m_topazSharpenSlider, 50));

  // Upscale combo
  auto *upscaleRow = new QWidget(m_topazSettingsPanel);
  auto *upscaleLayout = new QHBoxLayout(upscaleRow);
  upscaleLayout->setContentsMargins(0, 0, 0, 0);
  upscaleLayout->setSpacing(6);
  auto *upscaleLbl = new QLabel("Upscale", upscaleRow);
  upscaleLbl->setFixedWidth(70);
  upscaleLbl->setStyleSheet("QLabel { color: #999; font-size: 11px;"
                            "  background: transparent; border: none; }");
  m_topazUpscaleCombo = new QComboBox(upscaleRow);
  m_topazUpscaleCombo->addItems({"Off", "2x", "4x"});
  m_topazUpscaleCombo->installEventFilter(this);
  m_topazUpscaleCombo->setStyleSheet(
      "QComboBox { background: rgba(255,255,255,0.05); color: #e5e5e5;"
      "  border: 1px solid rgba(255,255,255,0.08); border-radius: 4px;"
      "  padding: 2px 8px; font-size: 11px; }"
      "QComboBox::drop-down { border: none; }"
      "QComboBox QAbstractItemView { background: #1a1a2e;"
      "  color: #e5e5e5; selection-background-color: #f59e0b; }");
  upscaleLayout->addWidget(upscaleLbl);
  upscaleLayout->addWidget(m_topazUpscaleCombo, 1);
  topazPanelLayout->addWidget(upscaleRow);

  // Apply button at the bottom of the panel
  auto *topazApplyBtn = new QPushButton("Apply Topaz AI", m_topazSettingsPanel);
  topazApplyBtn->setObjectName("topazApplyBtn");
  topazApplyBtn->setFixedHeight(34);
  topazApplyBtn->setCursor(Qt::PointingHandCursor);
  topazApplyBtn->setStyleSheet("QPushButton#topazApplyBtn {"
                               "  background: rgba(245, 158, 11, 0.12);"
                               "  color: #f59e0b;"
                               "  border: 1px solid rgba(245, 158, 11, 0.4);"
                               "  border-radius: 6px;"
                               "  font-size: 12px;"
                               "  font-weight: 600;"
                               "  padding: 4px 16px;"
                               "}"
                               "QPushButton#topazApplyBtn:hover {"
                               "  background: rgba(245, 158, 11, 0.22);"
                               "  border-color: #f59e0b;"
                               "}"
                               "QPushButton#topazApplyBtn:pressed {"
                               "  background: #f59e0b;"
                               "  color: #1a1a2e;"
                               "}");
  connect(topazApplyBtn, &QPushButton::clicked, this,
          &HdrStudioPage::onTopazAI);
  topazPanelLayout->addWidget(topazApplyBtn);

  controlsLayout->addWidget(m_topazSettingsPanel);

  // Combos kept hidden for code compatibility
  m_methodCombo = new QComboBox(controlsOuter);
  m_methodCombo->addItems({"Mertens", "Debevec"});
  m_methodCombo->hide();
  m_formatCombo = new QComboBox(controlsOuter);
  m_formatCombo->addItems({"JPEG 95%", "PNG", "TIFF"});
  m_formatCombo->hide();

  // ── Histogram ──
  m_histogram = new HistogramWidget(controlsOuter);
  m_histogram->setFixedHeight(80);
  m_histogram->setStyleSheet(
      "background: rgba(255,255,255,0.02); border-radius: 6px;");
  controlsLayout->addWidget(m_histogram);

  // ── Sliders (all -1.00 .. 1.00, default 0.00) ──
  auto *sliderScroll = new QScrollArea(controlsOuter);
  sliderScroll->setWidgetResizable(true);
  sliderScroll->setStyleSheet(
      "QScrollArea { border: none; background: transparent; }");
  sliderScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *slidersWidget = new QWidget(sliderScroll);
  auto *sl = new QVBoxLayout(slidersWidget);
  sl->setContentsMargins(0, 0, 0, 0);
  sl->setSpacing(2);

  // Helper: extract content layout from a section header wrapper
  auto getContentLayout = [](QWidget *sectionWrapper) -> QVBoxLayout * {
    // The wrapper has a QVBoxLayout with [btn, content]
    auto *wl = qobject_cast<QVBoxLayout *>(sectionWrapper->layout());
    if (!wl || wl->count() < 2)
      return nullptr;
    auto *content = qobject_cast<QWidget *>(wl->itemAt(1)->widget());
    if (!content)
      return nullptr;
    return qobject_cast<QVBoxLayout *>(content->layout());
  };

  // Simulates a click to collapse a section (starts collapsed)
  auto collapseSection = [](QWidget *sectionWrapper) {
    auto *wl = qobject_cast<QVBoxLayout *>(sectionWrapper->layout());
    if (wl && wl->count() >= 2) {
      auto *btn = qobject_cast<QPushButton *>(wl->itemAt(0)->widget());
      if (btn)
        btn->click(); // toggle to collapsed
    }
  };

  // ── LIGHT ──
  auto *lightSection = createSectionHeader("LIGHT", "#f59e0b");
  sl->addWidget(lightSection);
  {
    auto *cl = getContentLayout(lightSection);
    cl->addWidget(createSliderRow("Exposure", m_exposureSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Contrast", m_contrastSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Clarity", m_claritySlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Texture", m_textureSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Highlights", m_highlightsSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Shadows", m_shadowsSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Whites", m_whitesSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Blacks", m_blacksSlider, 0, 0, 0));
  }

  // ── COLOR ──
  auto *colorSection = createSectionHeader("COLOR", "#f59e0b");
  sl->addWidget(colorSection);
  {
    auto *cl = getContentLayout(colorSection);
    cl->addWidget(createSliderRow("Temperature", m_temperatureSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Tint", m_tintSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Vibrance", m_vibranceSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Saturation", m_saturationSlider, 0, 0, 0));
  }

  // ── HSL / COLOR MIXER ── (starts collapsed)
  auto *hslSection = createSectionHeader("HSL / COLOR MIXER", "#a78bfa");
  sl->addWidget(hslSection);
  {
    auto *cl = getContentLayout(hslSection);
    m_hslWheel = new HslWheelWidget(slidersWidget);
    cl->addWidget(m_hslWheel);

    connect(m_hslWheel, &HslWheelWidget::valueChanged, this, [this]() {
      if (m_sourceImage.empty())
        return;
      for (int i = 0; i < 8; ++i) {
        m_settings.hslHue[i] = m_hslWheel->hslHue(i);
        m_settings.hslSat[i] = m_hslWheel->hslSat(i);
        m_settings.hslLum[i] = m_hslWheel->hslLum(i);
      }
      updatePreview();
      m_previewTimer->start();
    });

    auto *hslResetBtn = new QPushButton("Reset HSL", slidersWidget);
    hslResetBtn->setCursor(Qt::PointingHandCursor);
    hslResetBtn->setFixedHeight(24);
    hslResetBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.04); color: #71717a;"
        " border: 1px solid rgba(255,255,255,0.06); border-radius: 4px;"
        " font-size: 10px; padding: 2px 12px; }"
        "QPushButton:hover { color: #e5e5e5; background: "
        "rgba(255,255,255,0.08);"
        " border-color: rgba(255,255,255,0.15); }");
    connect(hslResetBtn, &QPushButton::clicked, this, [this]() {
      m_hslWheel->resetAll();
      for (int i = 0; i < 8; ++i) {
        m_settings.hslHue[i] = 0;
        m_settings.hslSat[i] = 0;
        m_settings.hslLum[i] = 0;
      }
      updatePreview();
      m_previewTimer->start();
    });
    cl->addWidget(hslResetBtn);
  }
  collapseSection(hslSection); // start collapsed

  // ── COLOR GRADING ── (starts collapsed)
  auto *cgSection = createSectionHeader("COLOR GRADING", "#fb923c");
  sl->addWidget(cgSection);
  {
    auto *cl = getContentLayout(cgSection);

    // Create a horizontal layout for the 3 color wheels
    auto *wheelsWidget = new QWidget(slidersWidget);
    auto *wheelsLayout = new QHBoxLayout(wheelsWidget);
    wheelsLayout->setContentsMargins(0, 4, 0, 4);
    wheelsLayout->setSpacing(8);

    m_cgShadowWheel = new ColorWheelWidget("Shadows", wheelsWidget);
    m_cgMidtoneWheel = new ColorWheelWidget("Midtones", wheelsWidget);
    m_cgHighlightWheel = new ColorWheelWidget("Highlights", wheelsWidget);

    wheelsLayout->addWidget(m_cgShadowWheel);
    wheelsLayout->addWidget(m_cgMidtoneWheel);
    wheelsLayout->addWidget(m_cgHighlightWheel);
    cl->addWidget(wheelsWidget);

    // Connect wheels to direct update
    auto onWheelChanged = [this]() {
      m_settings.cgShadowHue = m_cgShadowWheel->hue();
      m_settings.cgShadowSat = m_cgShadowWheel->sat();
      m_settings.cgMidtoneHue = m_cgMidtoneWheel->hue();
      m_settings.cgMidtoneSat = m_cgMidtoneWheel->sat();
      m_settings.cgHighlightHue = m_cgHighlightWheel->hue();
      m_settings.cgHighlightSat = m_cgHighlightWheel->sat();
      updatePreview();
      m_previewTimer->start();
    };

    connect(m_cgShadowWheel, &ColorWheelWidget::colorChanged, this,
            onWheelChanged);
    connect(m_cgMidtoneWheel, &ColorWheelWidget::colorChanged, this,
            onWheelChanged);
    connect(m_cgHighlightWheel, &ColorWheelWidget::colorChanged, this,
            onWheelChanged);

    auto *cgResetBtn = new QPushButton("Reset Grading", wheelsWidget);
    cgResetBtn->setCursor(Qt::PointingHandCursor);
    cgResetBtn->setFixedHeight(24);
    cgResetBtn->setStyleSheet(
        "QPushButton { background: rgba(255,255,255,0.04); color: #71717a;"
        " border: 1px solid rgba(255,255,255,0.06); border-radius: 4px;"
        " font-size: 10px; padding: 2px 12px; }"
        "QPushButton:hover { color: #e5e5e5; background: "
        "rgba(255,255,255,0.08);"
        " border-color: rgba(255,255,255,0.15); }");
    connect(cgResetBtn, &QPushButton::clicked, this, [this]() {
      m_cgShadowWheel->reset();
      m_cgMidtoneWheel->reset();
      m_cgHighlightWheel->reset();
      m_settings.cgShadowHue = 0;
      m_settings.cgShadowSat = 0;
      m_settings.cgMidtoneHue = 0;
      m_settings.cgMidtoneSat = 0;
      m_settings.cgHighlightHue = 0;
      m_settings.cgHighlightSat = 0;
      updatePreview();
      m_previewTimer->start();
    });
    cl->addWidget(cgResetBtn);
  }
  collapseSection(cgSection); // start collapsed

  // ── EFFECTS ──
  auto *effectsSection = createSectionHeader("EFFECTS", "#f59e0b");
  sl->addWidget(effectsSection);
  {
    auto *cl = getContentLayout(effectsSection);
    cl->addWidget(createSliderRow("Dehaze", m_dehazeSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Vignette", m_vignetteSlider, 0, 0, 0));
  }

  // ── DETAIL ──
  auto *detailSection = createSectionHeader("DETAIL", "#f59e0b");
  sl->addWidget(detailSection);
  {
    auto *cl = getContentLayout(detailSection);
    cl->addWidget(createSliderRow("Sharpness", m_sharpeningSlider, 0, 0, 0));
    cl->addWidget(createSliderRow("Noise Reduction", m_noiseSlider, 0, 0, 0));
  }

  // HSL hue sliders: range is -100..100, mapping converts to -30..+30 degrees

  sl->addStretch();
  sliderScroll->setWidget(slidersWidget);
  controlsLayout->addWidget(sliderScroll, 1);

  // ── Export (bottom) ──
  auto *exportBtn = new QPushButton("Export (Ctrl+E)", controlsOuter);
  exportBtn->setFixedHeight(34);
  exportBtn->setCursor(Qt::PointingHandCursor);
  exportBtn->setStyleSheet("QPushButton { background: #f59e0b; color: #18181b; "
                           "border: none; border-radius: 6px; "
                           "font-size: 12px; font-weight: 600; }"
                           "QPushButton:hover { background: #fbbf24; }");
  connect(exportBtn, &QPushButton::clicked, this, &HdrStudioPage::onExport);
  controlsLayout->addWidget(exportBtn);

  splitter->addWidget(controlsOuter);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  splitter->setCollapsible(0, false);
  splitter->setCollapsible(1, false);
  splitter->handle(1)->setEnabled(false);
  splitter->handle(1)->setCursor(Qt::ArrowCursor);

  mainLayout->addWidget(splitter, 1);
}

// ════════════════════════════════════════════════
//  Load Images
// ════════════════════════════════════════════════

void HdrStudioPage::loadFromPaths(const QStringList &paths) {
  m_loadedPaths = paths;
  if (m_emptyOverlay)
    m_emptyOverlay->hide();

  // Cancel any pending full-res load
  m_fullResLoading = false;
  if (m_fullResCheckTimer)
    m_fullResCheckTimer->stop();

  // Group by bracket count from spinner
  int bracketCount = m_bracketCountSpin ? m_bracketCountSpin->value() : 1;
  m_bracketGroups.clear();
  m_currentGroup = 0;
  m_groupPreviews.clear();

  // Sort paths by filename for consistent grouping
  QStringList sortedPaths = paths;
  std::sort(sortedPaths.begin(), sortedPaths.end(),
            [](const QString &a, const QString &b) {
              return QFileInfo(a).fileName() < QFileInfo(b).fileName();
            });

  if (bracketCount > 1 && sortedPaths.size() >= bracketCount) {
    for (int i = 0; i < sortedPaths.size(); i += bracketCount) {
      QStringList group;
      for (int j = i; j < qMin(i + bracketCount, sortedPaths.size()); ++j)
        group.append(sortedPaths[j]);
      m_bracketGroups.append(group);
    }
  } else {
    m_bracketGroups.append(sortedPaths);
  }

  if (m_bracketGroups.size() > 1) {
    ToastWidget::show(this,
                      QString("%1 groups of %2 brackets")
                          .arg(m_bracketGroups.size())
                          .arg(bracketCount),
                      ToastWidget::Info);
  }
  updateGroupBar();

  // Show loading overlay
  if (m_loadingOverlay) {
    m_loadingOverlay->setGeometry(m_graphicsView->viewport()->rect());
    m_loadingOverlay->show();
    m_loadingOverlay->raise();
    m_loadingOverlay->repaint();
  }

  auto setProgress = [&](const QString &text, int val = -1) {
    if (m_loadingLabel)
      m_loadingLabel->setText(text);
    if (m_progressBar) {
      if (val < 0) {
        m_progressBar->setRange(0, 0);
      } else {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(val);
      }
    }
    if (m_loadingOverlay)
      m_loadingOverlay->repaint();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  };

  const int LOW_RES = 2400; // fast preview — full-res loaded async later
  QString method = m_methodCombo ? m_methodCombo->currentText() : "mertens";

  // === LOAD ALL GROUPS IN LOW-RES (synchronous, with progress) ===
  m_groupPreviews.resize(m_bracketGroups.size());
  int totalGroups = m_bracketGroups.size();

  for (int g = 0; g < totalGroups; ++g) {
    const QStringList &gPaths = m_bracketGroups[g];
    int baseProgress = (int)(100.0f * g / totalGroups);
    int nextProgress = (int)(100.0f * (g + 1) / totalGroups);

    if (gPaths.size() > 1) {
      setProgress(QString("Group %1/%2 — loading %3 images...")
                      .arg(g + 1)
                      .arg(totalGroups)
                      .arg(gPaths.size()),
                  baseProgress);

      // Parallel load images within this group
      std::vector<std::future<cv::Mat>> futures;
      for (const auto &p : gPaths)
        futures.push_back(std::async(std::launch::async, [p, LOW_RES]() {
          return HdrEngine::loadImage(p, LOW_RES);
        }));

      std::vector<cv::Mat> images;
      for (int i = 0; i < (int)futures.size(); ++i) {
        int imgProgress =
            baseProgress +
            (int)((float)(nextProgress - baseProgress) * i / gPaths.size());
        setProgress(QString("Group %1/%2 — image %3/%4")
                        .arg(g + 1)
                        .arg(totalGroups)
                        .arg(i + 1)
                        .arg(gPaths.size()),
                    imgProgress);
        cv::Mat img = futures[i].get();
        if (!img.empty())
          images.push_back(img);
      }

      if (!images.empty()) {
        int refW = images[0].cols, refH = images[0].rows;
        for (size_t i = 1; i < images.size(); ++i) {
          if (images[i].cols != refW || images[i].rows != refH)
            cv::resize(images[i], images[i], cv::Size(refW, refH), 0, 0,
                       cv::INTER_LINEAR);
        }
        setProgress(
            QString("Group %1/%2 — aligning...").arg(g + 1).arg(totalGroups),
            nextProgress - 4);
        auto aligned = HdrEngine::alignBrackets(images);
        setProgress(
            QString("Group %1/%2 — merging...").arg(g + 1).arg(totalGroups),
            nextProgress - 2);
        if (method.toLower() == "debevec")
          m_groupPreviews[g] = HdrEngine::mergeDebevec(aligned);
        else
          m_groupPreviews[g] = HdrEngine::mergeMertens(aligned);
      }
    } else if (gPaths.size() == 1) {
      setProgress(
          QString("Group %1/%2 — loading...").arg(g + 1).arg(totalGroups),
          baseProgress);
      m_groupPreviews[g] = HdrEngine::loadImage(gPaths.first(), LOW_RES);
    }

    // Display group 0 as soon as it's ready — don't wait for the rest
    if (g == 0) {
      if (m_loadingOverlay)
        m_loadingOverlay->hide();
      loadGroup(0);
      if (totalGroups > 1)
        showFullResBanner(QString("Loading groups 2-%1...").arg(totalGroups));
    }
  }

  // All groups loaded — hide any remaining overlays
  if (m_loadingOverlay)
    m_loadingOverlay->hide();
  hideFullResBanner();

  if (totalGroups > 1)
    ToastWidget::show(this, QString("All %1 groups ready").arg(totalGroups),
                      ToastWidget::Success);

  // Start full-res pipeline in background
  startFullResPipeline();
}

void HdrStudioPage::loadGroup(int index) {
  if (index < 0 || index >= m_bracketGroups.size())
    return;
  m_currentGroup = index;

  m_fullResLoading = false;
  if (m_fullResCheckTimer)
    m_fullResCheckTimer->stop();

  if (index < m_groupPreviews.size() && !m_groupPreviews[index].empty()) {
    // Cached — instant swap
    m_sourceImage = m_groupPreviews[index];
    m_merged = (m_bracketGroups[index].size() > 1);
  } else {
    // Not cached — load on demand
    const int LOW_RES = 800;
    QString method = m_methodCombo ? m_methodCombo->currentText() : "mertens";
    const QStringList &gPaths = m_bracketGroups[index];

    if (m_loadingOverlay) {
      m_loadingOverlay->setGeometry(m_graphicsView->viewport()->rect());
      m_loadingOverlay->show();
      m_loadingOverlay->raise();
    }
    if (m_loadingLabel)
      m_loadingLabel->setText(QString("Loading group %1...").arg(index + 1));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (gPaths.size() > 1) {
      std::vector<cv::Mat> images;
      for (const auto &p : gPaths) {
        cv::Mat img = HdrEngine::loadImage(p, LOW_RES);
        if (!img.empty())
          images.push_back(img);
      }
      if (!images.empty()) {
        int refW = images[0].cols, refH = images[0].rows;
        for (size_t i = 1; i < images.size(); ++i) {
          if (images[i].cols != refW || images[i].rows != refH)
            cv::resize(images[i], images[i], cv::Size(refW, refH), 0, 0,
                       cv::INTER_LINEAR);
        }
        // Skip alignment for preview — merge directly
        if (method.toLower() == "debevec")
          m_sourceImage = HdrEngine::mergeDebevec(images);
        else
          m_sourceImage = HdrEngine::mergeMertens(images);
      }
    } else {
      m_sourceImage = HdrEngine::loadImage(gPaths.first(), LOW_RES);
    }

    // Cache it
    if (index < m_groupPreviews.size())
      m_groupPreviews[index] = m_sourceImage;

    m_merged = (gPaths.size() > 1);
    if (m_loadingOverlay)
      m_loadingOverlay->hide();
  }

  updatePreviewSource();
  m_zoomFactor = 0;
  updateGroupBar();
  updateInfoPanel();
  updatePreview();
}

void HdrStudioPage::updateGroupBar() {
  if (!m_groupBar)
    return;
  if (m_bracketGroups.size() <= 1) {
    m_groupBar->hide();
    return;
  }
  m_groupBar->show();
  m_groupLabel->setText(QString("Group %1 / %2")
                            .arg(m_currentGroup + 1)
                            .arg(m_bracketGroups.size()));
  m_prevGroupBtn->setEnabled(m_currentGroup > 0);
  m_nextGroupBtn->setEnabled(m_currentGroup < m_bracketGroups.size() - 1);
}

void HdrStudioPage::startFullResPipeline() {
  m_fullResGroupIndex = 0;
  loadNextFullRes();
}

void HdrStudioPage::loadNextFullRes() {
  if (m_fullResGroupIndex >= m_bracketGroups.size()) {
    // All groups processed
    hideFullResBanner();
    return;
  }

  int g = m_fullResGroupIndex;
  QStringList paths = m_bracketGroups[g];
  QString method = m_methodCombo ? m_methodCombo->currentText() : "mertens";

  // Detach old future to avoid blocking destructor
  if (m_fullResFuture.valid()) {
    std::thread([f = std::move(m_fullResFuture)]() mutable {
      try {
        f.get();
      } catch (...) {
      }
    }).detach();
  }

  m_fullResLoading = true;
  m_fullResFuture =
      std::async(std::launch::async, [paths, method]() -> cv::Mat {
        if (paths.size() > 1) {
          // Parallel load all images at full resolution
          std::vector<std::future<cv::Mat>> futs;
          for (const auto &p : paths)
            futs.push_back(std::async(
                std::launch::async, [p]() { return HdrEngine::loadImage(p); }));

          std::vector<cv::Mat> imgs;
          for (auto &f : futs) {
            cv::Mat m = f.get();
            if (!m.empty())
              imgs.push_back(m);
          }
          if (imgs.empty())
            return cv::Mat();

          int rW = imgs[0].cols, rH = imgs[0].rows;
          for (size_t i = 1; i < imgs.size(); ++i) {
            if (imgs[i].cols != rW || imgs[i].rows != rH)
              cv::resize(imgs[i], imgs[i], cv::Size(rW, rH), 0, 0,
                         cv::INTER_LINEAR);
          }

          auto aligned = HdrEngine::alignBrackets(imgs);
          if (method.toLower() == "debevec")
            return HdrEngine::mergeDebevec(aligned);
          else
            return HdrEngine::mergeMertens(aligned);
        } else {
          return HdrEngine::loadImage(paths.first());
        }
      });

  if (!m_fullResCheckTimer) {
    m_fullResCheckTimer = new QTimer(this);
    m_fullResCheckTimer->setInterval(200);
    connect(m_fullResCheckTimer, &QTimer::timeout, this,
            &HdrStudioPage::onFullResReady);
  }
  m_fullResCheckTimer->start();
  showFullResBanner(QString("Full-res: group %1 / %2...")
                        .arg(g + 1)
                        .arg(m_bracketGroups.size()));
}

void HdrStudioPage::showFullResBanner(const QString &text, bool showProgress) {
  if (!m_fullResBanner) {
    m_fullResBanner = new QLabel(m_graphicsView->viewport());
    m_fullResBanner->setFixedWidth(300);
    m_fullResBanner->setStyleSheet(
        "QLabel#fullResBanner {"
        "  background: rgba(15, 15, 20, 0.88);"
        "  border: 1px solid rgba(245, 158, 11, 0.25);"
        "  border-radius: 10px;"
        "  padding: 0px;"
        "}");
    m_fullResBanner->setObjectName("fullResBanner");

    auto *layout = new QVBoxLayout(m_fullResBanner);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(6);

    m_fullResBannerLabel = new QLabel(m_fullResBanner);
    m_fullResBannerLabel->setAlignment(Qt::AlignCenter);
    m_fullResBannerLabel->setStyleSheet(
        "QLabel { background: transparent; border: none;"
        "  color: #e5e5e5; font-size: 12px; font-weight: 500;"
        "  letter-spacing: 0.3px; }");
    layout->addWidget(m_fullResBannerLabel);

    m_fullResProgress = new QProgressBar(m_fullResBanner);
    m_fullResProgress->setFixedHeight(4);
    m_fullResProgress->setTextVisible(false);
    m_fullResProgress->setRange(0, 0); // indeterminate
    m_fullResProgress->setStyleSheet(
        "QProgressBar {"
        "  background: rgba(255, 255, 255, 0.08);"
        "  border: none; border-radius: 2px;"
        "}"
        "QProgressBar::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #f59e0b, stop:1 #f97316);"
        "  border-radius: 2px;"
        "}");
    layout->addWidget(m_fullResProgress);
  }

  m_fullResBannerLabel->setText(text);
  m_fullResProgress->setVisible(showProgress);
  m_fullResBanner->adjustSize();

  // Position at bottom-center of viewport
  QRect vp = m_graphicsView->viewport()->rect();
  int x = (vp.width() - m_fullResBanner->width()) / 2;
  int y = vp.height() - m_fullResBanner->height() - 16;
  m_fullResBanner->move(x, y);
  m_fullResBanner->show();
  m_fullResBanner->raise();
}

void HdrStudioPage::hideFullResBanner() {
  if (m_fullResBanner)
    m_fullResBanner->hide();
}

void HdrStudioPage::onFullResReady() {
  if (!m_fullResLoading)
    return;

  // Check if future is ready without blocking
  if (m_fullResFuture.wait_for(std::chrono::seconds(0)) !=
      std::future_status::ready)
    return;

  // Future is ready — stop polling
  m_fullResCheckTimer->stop();
  m_fullResLoading = false;

  cv::Mat fullRes = m_fullResFuture.get();
  int g = m_fullResGroupIndex;

  if (!fullRes.empty() && g < m_groupPreviews.size()) {
    // Overwrite low-res cache with full-res
    m_groupPreviews[g] = fullRes;

    // If user is currently viewing this group, upgrade the display
    if (g == m_currentGroup) {
      m_sourceImage = fullRes;
      updatePreviewSource();
      updatePreview();
      showFullResBanner(
          QString::fromUtf8("\xe2\x9c\x93 Group %1 — full resolution")
              .arg(g + 1),
          false);
      QTimer::singleShot(1500, this, &HdrStudioPage::hideFullResBanner);
    }
  }

  // Advance to next group
  m_fullResGroupIndex++;
  loadNextFullRes();
}

void HdrStudioPage::updatePreviewSource() {
  if (m_sourceImage.empty()) {
    m_previewImage = cv::Mat();
    m_fastPreviewImage = cv::Mat();
    return;
  }

  if (m_hdPreview) {
    m_previewImage = m_sourceImage;
  } else {
    if (m_sourceImage.cols <= 800 && m_sourceImage.rows <= 800)
      m_previewImage = m_sourceImage;
    else
      m_previewImage = HdrEngine::generatePreview(m_sourceImage, 800);
  }

  // Build fast proxy (400px) for dragging
  if (m_sourceImage.cols <= 400 && m_sourceImage.rows <= 400)
    m_fastPreviewImage = m_sourceImage;
  else
    m_fastPreviewImage = HdrEngine::generatePreview(m_sourceImage, 400);
}

void HdrStudioPage::onLoadImages() {
  QString defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  QStringList files = QFileDialog::getOpenFileNames(
      this, "Load images", defaultDir,
      "Images (*.jpg *.jpeg *.png *.tiff *.tif *.bmp "
      "*.cr2 *.cr3 *.nef *.arw *.dng *.orf *.raf *.rw2 *.pef *.srw *.raw);;"
      "RAW (*.cr2 *.cr3 *.nef *.arw *.dng *.orf *.raf *.rw2 *.pef *.srw "
      "*.raw);;"
      "All files (*)");
  if (!files.isEmpty())
    loadFromPaths(files);
}

// ════════════════════════════════════════════════
//  Slider → Engine Mapping
// ════════════════════════════════════════════════
//  All sliders:  -100 .. 100  (displayed -1.00 .. 1.00)
//  Mapping to HdrSettings:
//    exposure   = 1.0 + val*0.01   (0 → 1.0,  100 → 2.0, -100 → 0.0)
//    contrast   = 1.0 + val*0.01
//    saturation = 1.0 + val*0.01
//    temperature = val             (0 → 0,  100 → 100, -100 → -100)
//    tint        = val
//    sharpening  = max(0, val)     (only +, clamped)
//    noise       = max(0, val)
//    everything else = val * 0.01

void HdrStudioPage::onSliderChanged() {
  if (m_sourceImage.empty())
    return;

  m_settings.exposure = 1.0 + m_exposureSlider->value() * 0.01;
  m_settings.contrast = 1.0 + m_contrastSlider->value() * 0.01;
  m_settings.clarity = m_claritySlider->value() * 0.01;
  m_settings.texture = m_textureSlider->value() * 0.01;
  m_settings.highlights = m_highlightsSlider->value() * 0.01;
  m_settings.shadows = m_shadowsSlider->value() * 0.01;
  m_settings.whites = m_whitesSlider->value() * 0.01;
  m_settings.blacks = m_blacksSlider->value() * 0.01;
  m_settings.temperature = m_temperatureSlider->value();
  m_settings.tint = m_tintSlider->value();
  m_settings.vibrance = m_vibranceSlider->value() * 0.01;
  m_settings.saturation = 1.0 + m_saturationSlider->value() * 0.01;
  m_settings.dehaze = m_dehazeSlider->value() * 0.01;
  m_settings.vignette = m_vignetteSlider->value() * 0.01;
  m_settings.sharpening = qMax(0, m_sharpeningSlider->value());
  m_settings.noiseReduction = qMax(0, m_noiseSlider->value());

  // HSL / Color Mixer — values come from HslWheelWidget directly
  // (connected via signal in setupUi)

  // Color Grading is now handled dynamically directly via ColorWheelWidget
  // signals, so we don't map from sliders here.

  // Direct render for immediate slider feedback
  updatePreview();

  // Deferred histogram (200ms after last slider move)
  m_previewTimer->start();
}

// ════════════════════════════════════════════════
//  Preview & Display
// ════════════════════════════════════════════════

void HdrStudioPage::updatePreview() {
  // Pick source: fast proxy during drag, normal proxy otherwise
  const cv::Mat &source =
      m_draggingSlider
          ? (m_fastPreviewImage.empty() ? m_previewImage : m_fastPreviewImage)
          : (m_previewImage.empty() ? m_sourceImage : m_previewImage);

  if (source.empty())
    return;

  int gen = ++m_renderVersion;

  // Apply adjustments (fast mode skips clarity/texture/dehaze/vignette)
  cv::Mat adjusted =
      HdrEngine::applyAdjustments(source, m_settings, true, m_draggingSlider);
  QImage edited = HdrEngine::matToQImage(adjusted);
  if (edited.isNull() || m_renderVersion.load() != gen)
    return;

  // Clipping overlay
  if (m_showClipping) {
    cv::Mat clipMask = HdrEngine::getClippingMask(adjusted);
    if (!clipMask.empty()) {
      QImage clipImg = HdrEngine::matToQImage(clipMask);
      if (!clipImg.isNull() && clipImg.size() == edited.size()) {
        QPainter p(&edited);
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.drawImage(0, 0, clipImg);
        p.end();
      }
    }
  }

  // Skip histogram during drag for extra speed
  if (!m_draggingSlider)
    m_histogram->updateHistogram(edited);

  QPixmap pix;

  if (m_splitViewActive) {
    HdrSettings neutral;
    QImage original = HdrEngine::preview(source, neutral);
    if (!original.isNull() && original.size() == edited.size()) {
      int splitX = (int)(edited.width() * m_splitPos);
      QImage composite = edited.copy();
      QPainter painter(&composite);
      painter.drawImage(QRect(0, 0, splitX, edited.height()), original,
                        QRect(0, 0, splitX, original.height()));
      painter.setPen(QPen(QColor("#f59e0b"), 2));
      painter.drawLine(splitX, 0, splitX, edited.height());
      painter.setPen(QColor("#ffffff"));
      QFont f = painter.font();
      f.setPixelSize(11);
      f.setBold(true);
      painter.setFont(f);
      painter.drawText(8, 18, "BEFORE");
      painter.drawText(edited.width() - 50, 18, "AFTER");
      painter.end();
      pix = QPixmap::fromImage(composite);
    } else {
      pix = QPixmap::fromImage(edited);
    }
  } else {
    pix = QPixmap::fromImage(edited);
  }

  m_pixmapItem->setPixmap(pix);
  m_graphicsScene->setSceneRect(pix.rect());
  applyZoom();
}

void HdrStudioPage::updateInfoPanel() {
  if (m_sourceImage.empty()) {
    if (m_infoBar)
      m_infoBar->setText("No image loaded");
    return;
  }
  QString info =
      QString("%1 x %2 px").arg(m_sourceImage.cols).arg(m_sourceImage.rows);

  // Show bracket group info
  if (m_bracketGroups.size() > 1) {
    info += QString("  |  Group %1/%2")
                .arg(m_currentGroup + 1)
                .arg(m_bracketGroups.size());
  }

  if (!m_loadedPaths.isEmpty()) {
    const auto &groupPaths = m_bracketGroups.size() > m_currentGroup
                                 ? m_bracketGroups[m_currentGroup]
                                 : m_loadedPaths;
    QFileInfo fi(groupPaths.first());
    double sizeMB = fi.size() / (1024.0 * 1024.0);
    if (groupPaths.size() > 1)
      info += QString("  |  %1 files  |  %2")
                  .arg(groupPaths.size())
                  .arg(fi.suffix().toUpper());
    else
      info += QString("  |  %1  |  %2 MB  |  %3")
                  .arg(fi.suffix().toUpper())
                  .arg(QString::number(sizeMB, 'f', 1))
                  .arg(fi.fileName());
  }
  if (m_zoomFactor > 0)
    info += QString("  |  %1%").arg(int(m_zoomFactor * 100));
  else
    info += "  |  Fit";

  // Add navigation hint if multiple groups
  if (m_bracketGroups.size() > 1)
    info += "  |  ←/→: switch group";

  if (m_infoBar)
    m_infoBar->setText(info);
}

// ════════════════════════════════════════════════
//  Zoom
// ════════════════════════════════════════════════

void HdrStudioPage::onZoomIn() {
  if (m_zoomFactor <= 0) {
    // From fit mode: start at the actual fit scale
    if (m_pixmapItem && !m_pixmapItem->pixmap().isNull()) {
      QSizeF imgSz = m_pixmapItem->pixmap().size();
      QSizeF viewSz = m_graphicsView->viewport()->size();
      m_zoomFactor = qMin(viewSz.width() / imgSz.width(),
                          viewSz.height() / imgSz.height());
    } else {
      m_zoomFactor = 1.0;
    }
  }
  m_zoomFactor = qMin(m_zoomFactor * 1.25, 20.0);
  if (m_zoomLabel)
    m_zoomLabel->setText(QString("%1%").arg(int(m_zoomFactor * 100)));
  updateInfoPanel();
  applyZoom();
}

void HdrStudioPage::onZoomOut() {
  if (m_zoomFactor <= 0) {
    // Already in fit mode, nothing to zoom out to
    return;
  }
  m_zoomFactor = m_zoomFactor * 0.8;

  // If we've zoomed out past the fit level, snap to fit mode
  if (m_pixmapItem && !m_pixmapItem->pixmap().isNull()) {
    QSizeF imgSz = m_pixmapItem->pixmap().size();
    QSizeF viewSz = m_graphicsView->viewport()->size();
    double fitScale =
        qMin(viewSz.width() / imgSz.width(), viewSz.height() / imgSz.height());
    if (m_zoomFactor <= fitScale) {
      onZoomFit();
      return;
    }
  }

  if (m_zoomLabel)
    m_zoomLabel->setText(QString("%1%").arg(int(m_zoomFactor * 100)));
  updateInfoPanel();
  applyZoom();
}

void HdrStudioPage::onZoomFit() {
  m_zoomFactor = 0;
  if (m_zoomLabel)
    m_zoomLabel->setText("Fit");
  updateInfoPanel();
  applyZoom();
}

void HdrStudioPage::applyZoom() {
  if (!m_pixmapItem || m_pixmapItem->pixmap().isNull())
    return;

  if (m_zoomFactor <= 0) {
    // Fit mode: fit entire image in viewport — smooth for nice downscaling
    m_graphicsView->setRenderHint(QPainter::SmoothPixmapTransform, true);
    m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
    m_graphicsView->resetTransform();
    m_graphicsView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
  } else {
    // Zoomed: disable smooth above 100% to show true pixels (like Lightroom)
    bool smooth = m_zoomFactor < 1.0;
    m_graphicsView->setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    m_pixmapItem->setTransformationMode(smooth ? Qt::SmoothTransformation
                                               : Qt::FastTransformation);
    // Preserve viewport center while zooming
    QPointF center =
        m_graphicsView->mapToScene(m_graphicsView->viewport()->rect().center());
    m_graphicsView->resetTransform();
    m_graphicsView->scale(m_zoomFactor, m_zoomFactor);
    m_graphicsView->centerOn(center);
  }
}

void HdrStudioPage::animateZoom() {
  if (m_targetZoom <= 0 || !m_pixmapItem || m_pixmapItem->pixmap().isNull()) {
    if (m_zoomTimer)
      m_zoomTimer->stop();
    return;
  }

  // Ease toward target: move 50% of remaining distance per frame (fast +
  // smooth)
  double diff = m_targetZoom - m_zoomFactor;
  if (std::abs(diff) < 0.001) {
    // Close enough — snap to target and stop
    m_zoomFactor = m_targetZoom;
    m_zoomTimer->stop();
  } else {
    m_zoomFactor += diff * 0.5;
  }

  // Apply scale relative to current transform for smooth movement
  m_graphicsView->resetTransform();
  m_graphicsView->scale(m_zoomFactor, m_zoomFactor);

  if (m_zoomLabel)
    m_zoomLabel->setText(QString("%1%").arg(int(m_zoomFactor * 100)));
  updateInfoPanel();
}

void HdrStudioPage::onRemoveImage(int index) {
  if (index >= 0 && index < m_loadedPaths.size()) {
    m_loadedPaths.removeAt(index);
  }
}

// ════════════════════════════════════════════════
//  Reset
// ════════════════════════════════════════════════

void HdrStudioPage::onResetAll() {
  pushUndo();
  m_settings = HdrSettings{};

  std::vector<QSlider *> allSliders = {
      m_exposureSlider, m_contrastSlider,   m_claritySlider,
      m_textureSlider,  m_highlightsSlider, m_shadowsSlider,
      m_whitesSlider,   m_blacksSlider,     m_temperatureSlider,
      m_tintSlider,     m_vibranceSlider,   m_saturationSlider,
      m_dehazeSlider,   m_vignetteSlider,   m_sharpeningSlider,
      m_noiseSlider};

  m_cgShadowWheel->blockSignals(true);
  m_cgShadowWheel->setColor(0, 0);
  m_cgShadowWheel->blockSignals(false);

  m_cgMidtoneWheel->blockSignals(true);
  m_cgMidtoneWheel->setColor(0, 0);
  m_cgMidtoneWheel->blockSignals(false);

  m_cgHighlightWheel->blockSignals(true);
  m_cgHighlightWheel->setColor(0, 0);
  m_cgHighlightWheel->blockSignals(false);

  for (auto *s : allSliders)
    s->blockSignals(true);
  // Reset HSL wheel
  m_hslWheel->blockSignals(true);
  m_hslWheel->resetAll();
  m_hslWheel->blockSignals(false);

  // Update value labels
  emit m_exposureSlider->valueChanged(0);

  if (!m_sourceImage.empty()) {
    updatePreview();
    ToastWidget::show(this, "Settings reset", ToastWidget::Info);
  }
}

// ════════════════════════════════════════════════
//  Export
// ════════════════════════════════════════════════

void HdrStudioPage::onExport() {
  if (m_sourceImage.empty()) {
    ToastWidget::show(this, "No image loaded", ToastWidget::Info);
    return;
  }

  QString filter = "JPEG (*.jpg);;PNG (*.png);;TIFF (*.tiff)";
  QString defaultPath =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
      "/blacklys_export.jpg";
  QString selectedFilter;
  QString savePath = QFileDialog::getSaveFileName(
      this, "Export image", defaultPath, filter, &selectedFilter);
  if (savePath.isEmpty())
    return;

  // Show export overlay
  m_loadingLabel->setText("Exporting...");
  m_progressBar->setValue(20);
  m_loadingOverlay->setGeometry(m_graphicsView->viewport()->rect());
  m_loadingOverlay->show();
  m_loadingOverlay->raise();
  m_loadingOverlay->repaint();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  QFileInfo fi(savePath);
  m_settings.outputFormat = fi.suffix().toLower();
  m_settings.mergeMethod = m_methodCombo->currentText().toLower();
  m_settings.jpegQuality = 95;

  m_loadingLabel->setText("Applying adjustments...");
  m_progressBar->setValue(50);
  m_loadingOverlay->repaint();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  cv::Mat adjusted = HdrEngine::applyAdjustments(m_sourceImage, m_settings);

  m_loadingLabel->setText("Saving file...");
  m_progressBar->setValue(80);
  m_loadingOverlay->repaint();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  if (HdrEngine::saveImage(adjusted, savePath, m_settings)) {
    m_progressBar->setValue(100);
    m_loadingOverlay->hide();
    ToastWidget::show(this, QString("Exported: %1").arg(fi.fileName()),
                      ToastWidget::Success);
  } else {
    m_loadingOverlay->hide();
    ToastWidget::show(this, "Export failed", ToastWidget::Error);
  }
}

// ════════════════════════════════════════════════
//  Topaz AI
// ════════════════════════════════════════════════

void HdrStudioPage::onTopazAI() {
  if (m_sourceImage.empty()) {
    ToastWidget::show(this, "No image loaded", ToastWidget::Info);
    return;
  }

  // Check if already running
  if (m_topazProcess && m_topazProcess->state() != QProcess::NotRunning) {
    ToastWidget::show(this, "Topaz AI is already processing",
                      ToastWidget::Info);
    return;
  }

  // Get Topaz path from settings
  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT value FROM settings WHERE key = ?");
  q.addBindValue("topaz_path");
  q.exec();
  QString topazPath;
  if (q.next())
    topazPath = q.value(0).toString();

  if (topazPath.isEmpty() || !QFile::exists(topazPath)) {
    ToastWidget::show(this, "Topaz path not configured (Settings)",
                      ToastWidget::Info);
    return;
  }

  // Export current image as 16-bit TIFF
  QString tempDir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QString inputPath = tempDir + "/blacklys_topaz_input.tiff";
  QString outputDir = tempDir + "/blacklys_topaz_output";
  QDir().mkpath(outputDir);

  cv::Mat adjusted = HdrEngine::applyAdjustments(m_sourceImage, m_settings);
  if (!HdrEngine::saveImage(adjusted, inputPath, m_settings)) {
    ToastWidget::show(this, "Temp export error", ToastWidget::Error);
    return;
  }

  // Expected output path (Topaz keeps the filename)
  m_topazOutputPath = outputDir + "/blacklys_topaz_input.tiff";

  // Setup process
  if (!m_topazProcess) {
    m_topazProcess = new QProcess(this);
    connect(m_topazProcess, &QProcess::readyReadStandardOutput, this,
            &HdrStudioPage::onTopazOutput);
    connect(m_topazProcess, &QProcess::readyReadStandardError, this,
            &HdrStudioPage::onTopazOutput);
    connect(m_topazProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &HdrStudioPage::onTopazFinished);
  }

  // Launch CLI
  QStringList args;
  args << inputPath << "--output" << outputDir << "--format"
       << "tiff"
       << "--bit-depth"
       << "16"
       << "--overwrite";

  // Topaz AI manual settings (undocumented CLI flags)
  int denoiseVal = m_topazDenoiseSlider ? m_topazDenoiseSlider->value() : 0;
  int sharpenVal = m_topazSharpenSlider ? m_topazSharpenSlider->value() : 0;
  QString upscaleText =
      m_topazUpscaleCombo ? m_topazUpscaleCombo->currentText() : "Off";

  if (denoiseVal > 0) {
    args << "--denoise" << QString("strength=%1").arg(denoiseVal);
  }
  if (sharpenVal > 0) {
    args << "--sharpen" << QString("strength=%1").arg(sharpenVal);
  }
  if (upscaleText != "Off") {
    int scale = upscaleText.replace("x", "").toInt();
    args << "--upscale" << QString("scale=%1").arg(scale);
  }

  showFullResBanner("Topaz AI processing…");
  m_topazProcess->start(topazPath, args);
}

void HdrStudioPage::onTopazOutput() {
  if (!m_topazProcess)
    return;

  // Read both stdout and stderr
  QByteArray out = m_topazProcess->readAllStandardOutput();
  QByteArray err = m_topazProcess->readAllStandardError();
  QString text = QString::fromUtf8(out + err).trimmed();

  if (text.isEmpty())
    return;

  // Try to extract percentage from Topaz output (e.g., "Processing: 45%")
  QRegularExpression rx("(\\d+)\\s*%");
  auto match = rx.match(text);
  if (match.hasMatch()) {
    int pct = match.captured(1).toInt();
    if (m_fullResProgress) {
      m_fullResProgress->setRange(0, 100);
      m_fullResProgress->setValue(pct);
    }
    showFullResBanner(QString("Topaz AI — %1%").arg(pct));
  } else {
    // Show last meaningful line
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    if (!lines.isEmpty()) {
      QString last = lines.last().trimmed();
      if (last.length() > 50)
        last = last.left(47) + "…";
      showFullResBanner("Topaz AI — " + last);
    }
  }
}

void HdrStudioPage::onTopazFinished(int exitCode, QProcess::ExitStatus status) {
  if (status != QProcess::NormalExit || exitCode != 0) {
    showFullResBanner("✗ Topaz AI failed", false);
    QTimer::singleShot(3000, this, &HdrStudioPage::hideFullResBanner);
    return;
  }

  // Check if output file exists
  if (!QFile::exists(m_topazOutputPath)) {
    showFullResBanner("✗ Output file not found", false);
    QTimer::singleShot(3000, this, &HdrStudioPage::hideFullResBanner);
    return;
  }

  // Reimport the processed image
  cv::Mat processed = HdrEngine::loadImage(m_topazOutputPath);
  if (processed.empty()) {
    showFullResBanner("✗ Could not load result", false);
    QTimer::singleShot(3000, this, &HdrStudioPage::hideFullResBanner);
    return;
  }

  // Swap source image and refresh
  m_sourceImage = processed;
  // Reset adjustments since Topaz already applied its processing
  m_settings = HdrSettings();
  updatePreviewSource();
  updatePreview();

  showFullResBanner("✓ Topaz AI complete", false);
  QTimer::singleShot(2000, this, &HdrStudioPage::hideFullResBanner);
}

// ════════════════════════════════════════════════
//  Batch Processing
// ════════════════════════════════════════════════

void HdrStudioPage::onBatchProcess() {
  QString inputDir = QFileDialog::getExistingDirectory(
      this, "Bracketing images folder", QString(), QFileDialog::ShowDirsOnly);
  if (inputDir.isEmpty())
    return;

  QSqlQuery q(Database::instance().db());
  q.prepare("SELECT value FROM settings WHERE key = ?");
  q.addBindValue("output_dir");
  q.exec();
  QString outputDir;
  if (q.next())
    outputDir = q.value(0).toString();
  if (outputDir.isEmpty()) {
    outputDir = inputDir + "/output";
    QDir().mkpath(outputDir);
  }

  QDir dir(inputDir);
  QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.tiff", "*.tif",
                         "*.cr2", "*.nef",  "*.arw", "*.dng",  "*.raf"};
  QStringList allFiles = dir.entryList(filters, QDir::Files, QDir::Name);

  if (allFiles.size() < 3) {
    ToastWidget::show(this, "Not enough images (min 3)", ToastWidget::Info);
    return;
  }

  int groupCount = allFiles.size() / 3;
  auto result = QMessageBox::question(
      this, "Batch HDR",
      QString("%1 images → %2 HDR groups.\nExport to:\n%3 ?")
          .arg(allFiles.size())
          .arg(groupCount)
          .arg(outputDir),
      QMessageBox::Yes | QMessageBox::No);
  if (result != QMessageBox::Yes)
    return;

  ToastWidget::show(this, "Batch processing...", ToastWidget::Info);

  int success = 0;
  HdrSettings batchSettings = m_settings;
  batchSettings.jpegQuality = 95;

  for (int g = 0; g < groupCount; ++g) {
    QStringList group;
    for (int i = 0; i < 3; ++i)
      group << dir.filePath(allFiles[g * 3 + i]);

    QString baseName = QFileInfo(group[0]).baseName();
    ToastWidget::show(
        this,
        QString("Batch %1/%2: %3").arg(g + 1).arg(groupCount).arg(baseName),
        ToastWidget::Info);
    qApp->processEvents();

    HdrJobResult jobResult = HdrEngine::mergeBracketGroup(
        group, batchSettings, outputDir, QString("HDR_%1").arg(baseName));
    if (jobResult.success)
      ++success;
  }

  ToastWidget::show(
      this, QString("Batch HDR: %1/%2 images").arg(success).arg(groupCount),
      ToastWidget::Success);
}

// ════════════════════════════════════════════════
//  Undo / Redo
// ════════════════════════════════════════════════

void HdrStudioPage::pushUndo() {
  m_undoStack.push(m_settings);
  // Limit stack size if needed
  if (m_undoStack.size() > 50)
    m_undoStack.removeFirst();
  m_redoStack.clear();
}

void HdrStudioPage::undo() {
  if (m_undoStack.isEmpty())
    return;

  m_redoStack.push(m_settings);
  m_settings = m_undoStack.pop();

  // Block signals to prevent double-pushing to undo stack or re-triggering
  // logic Update UI from settings
  m_exposureSlider->blockSignals(true);
  m_contrastSlider->blockSignals(true);
  m_highlightsSlider->blockSignals(true);
  m_shadowsSlider->blockSignals(true);
  m_whitesSlider->blockSignals(true);
  m_blacksSlider->blockSignals(true);
  m_temperatureSlider->blockSignals(true);
  m_tintSlider->blockSignals(true);
  m_vibranceSlider->blockSignals(true);
  m_saturationSlider->blockSignals(true);
  m_dehazeSlider->blockSignals(true);
  m_vignetteSlider->blockSignals(true);
  m_sharpeningSlider->blockSignals(true);
  m_noiseSlider->blockSignals(true);

  m_exposureSlider->setValue(int((m_settings.exposure - 1.0) * 100));
  m_contrastSlider->setValue(int((m_settings.contrast - 1.0) * 100));
  m_highlightsSlider->setValue(int(m_settings.highlights * 100));
  m_shadowsSlider->setValue(int(m_settings.shadows * 100));
  m_whitesSlider->setValue(int(m_settings.whites * 100));
  m_blacksSlider->setValue(int(m_settings.blacks * 100));
  m_temperatureSlider->setValue(int(m_settings.temperature));
  m_tintSlider->setValue(int(m_settings.tint));
  m_vibranceSlider->setValue(int(m_settings.vibrance * 100));
  m_saturationSlider->setValue(int((m_settings.saturation - 1.0) * 100));
  m_dehazeSlider->setValue(int(m_settings.dehaze * 100));
  m_vignetteSlider->setValue(int(m_settings.vignette * 100));
  m_sharpeningSlider->setValue(int(m_settings.sharpening));
  m_noiseSlider->setValue(int(m_settings.noiseReduction));

  m_exposureSlider->blockSignals(false);
  m_contrastSlider->blockSignals(false);
  m_highlightsSlider->blockSignals(false);
  m_shadowsSlider->blockSignals(false);
  m_whitesSlider->blockSignals(false);
  m_blacksSlider->blockSignals(false);
  m_temperatureSlider->blockSignals(false);
  m_tintSlider->blockSignals(false);
  m_vibranceSlider->blockSignals(false);
  m_saturationSlider->blockSignals(false);
  m_dehazeSlider->blockSignals(false);
  m_vignetteSlider->blockSignals(false);
  m_sharpeningSlider->blockSignals(false);
  m_noiseSlider->blockSignals(false);

  // Update value labels manually since we blocked signals
  emit m_exposureSlider->valueChanged(m_exposureSlider->value());
  emit m_contrastSlider->valueChanged(m_contrastSlider->value());
  emit m_highlightsSlider->valueChanged(m_highlightsSlider->value());
  emit m_shadowsSlider->valueChanged(m_shadowsSlider->value());
  emit m_whitesSlider->valueChanged(m_whitesSlider->value());
  emit m_blacksSlider->valueChanged(m_blacksSlider->value());
  emit m_temperatureSlider->valueChanged(m_temperatureSlider->value());
  emit m_tintSlider->valueChanged(m_tintSlider->value());
  emit m_vibranceSlider->valueChanged(m_vibranceSlider->value());
  emit m_saturationSlider->valueChanged(m_saturationSlider->value());
  emit m_dehazeSlider->valueChanged(m_dehazeSlider->value());
  emit m_vignetteSlider->valueChanged(m_vignetteSlider->value());
  emit m_sharpeningSlider->valueChanged(m_sharpeningSlider->value());
  emit m_noiseSlider->valueChanged(m_noiseSlider->value());

  updatePreview();
  ToastWidget::show(this, "Undo", ToastWidget::Info);
}

void HdrStudioPage::redo() {
  if (m_redoStack.isEmpty())
    return;

  m_undoStack.push(m_settings);
  m_settings = m_redoStack.pop();

  // Block signals
  m_exposureSlider->blockSignals(true);
  m_contrastSlider->blockSignals(true);
  m_highlightsSlider->blockSignals(true);
  m_shadowsSlider->blockSignals(true);
  m_whitesSlider->blockSignals(true);
  m_blacksSlider->blockSignals(true);
  m_temperatureSlider->blockSignals(true);
  m_tintSlider->blockSignals(true);
  m_vibranceSlider->blockSignals(true);
  m_saturationSlider->blockSignals(true);
  m_dehazeSlider->blockSignals(true);
  m_vignetteSlider->blockSignals(true);
  m_sharpeningSlider->blockSignals(true);
  m_noiseSlider->blockSignals(true);

  m_exposureSlider->setValue(int((m_settings.exposure - 1.0) * 100));
  m_contrastSlider->setValue(int((m_settings.contrast - 1.0) * 100));
  m_highlightsSlider->setValue(int(m_settings.highlights * 100));
  m_shadowsSlider->setValue(int(m_settings.shadows * 100));
  m_whitesSlider->setValue(int(m_settings.whites * 100));
  m_blacksSlider->setValue(int(m_settings.blacks * 100));
  m_temperatureSlider->setValue(int(m_settings.temperature));
  m_tintSlider->setValue(int(m_settings.tint));
  m_vibranceSlider->setValue(int(m_settings.vibrance * 100));
  m_saturationSlider->setValue(int((m_settings.saturation - 1.0) * 100));
  m_dehazeSlider->setValue(int(m_settings.dehaze * 100));
  m_vignetteSlider->setValue(int(m_settings.vignette * 100));
  m_sharpeningSlider->setValue(int(m_settings.sharpening));
  m_noiseSlider->setValue(int(m_settings.noiseReduction));

  m_exposureSlider->blockSignals(false);
  m_contrastSlider->blockSignals(false);
  m_highlightsSlider->blockSignals(false);
  m_shadowsSlider->blockSignals(false);
  m_whitesSlider->blockSignals(false);
  m_blacksSlider->blockSignals(false);
  m_temperatureSlider->blockSignals(false);
  m_tintSlider->blockSignals(false);
  m_vibranceSlider->blockSignals(false);
  m_saturationSlider->blockSignals(false);
  m_dehazeSlider->blockSignals(false);
  m_vignetteSlider->blockSignals(false);
  m_sharpeningSlider->blockSignals(false);
  m_noiseSlider->blockSignals(false);

  // Update value labels manually
  emit m_exposureSlider->valueChanged(m_exposureSlider->value());
  emit m_contrastSlider->valueChanged(m_contrastSlider->value());
  emit m_highlightsSlider->valueChanged(m_highlightsSlider->value());
  emit m_shadowsSlider->valueChanged(m_shadowsSlider->value());
  emit m_whitesSlider->valueChanged(m_whitesSlider->value());
  emit m_blacksSlider->valueChanged(m_blacksSlider->value());
  emit m_temperatureSlider->valueChanged(m_temperatureSlider->value());
  emit m_tintSlider->valueChanged(m_tintSlider->value());
  emit m_vibranceSlider->valueChanged(m_vibranceSlider->value());
  emit m_saturationSlider->valueChanged(m_saturationSlider->value());
  emit m_dehazeSlider->valueChanged(m_dehazeSlider->value());
  emit m_vignetteSlider->valueChanged(m_vignetteSlider->value());
  emit m_sharpeningSlider->valueChanged(m_sharpeningSlider->value());
  emit m_noiseSlider->valueChanged(m_noiseSlider->value());

  updatePreview();
  ToastWidget::show(this, "Redo", ToastWidget::Info);
}

void HdrStudioPage::keyPressEvent(QKeyEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->key() == Qt::Key_Z) {
      if (event->modifiers() & Qt::ShiftModifier)
        redo();
      else
        undo();
      return;
    } else if (event->key() == Qt::Key_Y) {
      redo();
      return;
    } else if (event->key() == Qt::Key_E) {
      onExport();
      return;
    } else if (event->key() == Qt::Key_O) {
      onLoadImages();
      return;
    } else if (event->key() == Qt::Key_C) {
      // Copy current image to clipboard
      if (m_imageLabel && !m_imageLabel->pixmap().isNull()) {
        QApplication::clipboard()->setPixmap(m_imageLabel->pixmap());
        ToastWidget::show(this, "Copied to clipboard", ToastWidget::Success);
      }
      return;
    }
  }
  switch (event->key()) {
  case Qt::Key_Plus:
  case Qt::Key_Equal:
    onZoomIn();
    return;
  case Qt::Key_Minus:
    onZoomOut();
    return;
  case Qt::Key_0:
    onZoomFit();
    return;
  case Qt::Key_R:
    onRotateCW();
    return;
  case Qt::Key_A:
    onAutoEnhance();
    return;
  case Qt::Key_G:
    onToggleGrid();
    return;
  case Qt::Key_J:
    onToggleClipping();
    return;
  case Qt::Key_F11: {
    auto *mw = qobject_cast<MainWindow *>(window());
    if (mw)
      mw->toggleFullscreen();
  }
    return;
  case Qt::Key_Space:
    onToggleSplitView();
    return;
  case Qt::Key_PageUp:
  case Qt::Key_Left:
    if (m_bracketGroups.size() > 1 && m_currentGroup > 0) {
      loadGroup(m_currentGroup - 1);
    }
    return;
  case Qt::Key_PageDown:
  case Qt::Key_Right:
    if (m_bracketGroups.size() > 1 &&
        m_currentGroup < m_bracketGroups.size() - 1) {
      loadGroup(m_currentGroup + 1);
    }
    return;
  case Qt::Key_H:
  case Qt::Key_Question: {
    QString help = "<b style='color:#f59e0b'>HDR Studio Shortcuts</b><br><br>"
                   "<table style='color:#d4d4d8;font-size:12px'>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>Ctrl+O</td><td>Open images</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>Ctrl+E</td><td>Export</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>Ctrl+Z</td><td>Undo</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>Ctrl+Y</td><td>Redo</td></tr>"
                   "<tr><td style='padding:2px 12px 2px 0;color:#fbbf24'>+ / "
                   "-</td><td>Zoom in / out</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>0</td><td>Fit to view</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>Ctrl+Scroll</td><td>Wheel zoom</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>R</td><td>Rotate 90\u00b0 CW</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>A</td><td>Auto enhance</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>G</td><td>Grid overlay</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>Space</td><td>Before / After</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>F11</td><td>Fullscreen</td></tr>"
                   "<tr><td style='padding:2px 12px 2px "
                   "0;color:#fbbf24'>Ctrl+C</td><td>Copy to clipboard</td></tr>"
                   "<tr><td style='padding:2px 12px 2px 0;color:#fbbf24'>H / "
                   "?</td><td>Show this help</td></tr>"
                   "</table>";
    QMessageBox helpBox(this);
    helpBox.setWindowTitle("Keyboard Shortcuts");
    helpBox.setTextFormat(Qt::RichText);
    helpBox.setText(help);
    helpBox.setStyleSheet("QMessageBox { background: #18181b; }"
                          "QLabel { color: #d4d4d8; }"
                          "QPushButton { background: #27272a; color: #f4f4f5; "
                          "border: 1px solid #3f3f46; "
                          "border-radius: 6px; padding: 6px 20px; } "
                          "QPushButton:hover { background: #3f3f46; }");
    helpBox.exec();
  }
    return;
  default:
    break;
  }
  QWidget::keyPressEvent(event);
}

// ════════════════════════════════════════════════
//  Rotate
// ════════════════════════════════════════════════

void HdrStudioPage::onRotateCW() {
  if (m_sourceImage.empty())
    return;
  cv::rotate(m_sourceImage, m_sourceImage, cv::ROTATE_90_CLOCKWISE);
  updatePreviewSource();
  updatePreview();
  updateInfoPanel();
  ToastWidget::show(this, "Rotated 90\u00b0 CW", ToastWidget::Info);
}

void HdrStudioPage::onRotateCCW() {
  if (m_sourceImage.empty())
    return;
  cv::rotate(m_sourceImage, m_sourceImage, cv::ROTATE_90_COUNTERCLOCKWISE);
  updatePreviewSource();
  updatePreview();
  updateInfoPanel();
  ToastWidget::show(this, "Rotated 90\u00b0 CCW", ToastWidget::Info);
}

void HdrStudioPage::onFlipH() {
  if (m_sourceImage.empty())
    return;
  cv::flip(m_sourceImage, m_sourceImage, 1); // 1 = horizontal
  updatePreviewSource();
  updatePreview();
  ToastWidget::show(this, "Flipped horizontally", ToastWidget::Info);
}

void HdrStudioPage::onFlipV() {
  if (m_sourceImage.empty())
    return;
  cv::flip(m_sourceImage, m_sourceImage, 0); // 0 = vertical
  updatePreviewSource();
  updatePreview();
  ToastWidget::show(this, "Flipped vertically", ToastWidget::Info);
}

void HdrStudioPage::onAutoEnhance() {
  if (m_sourceImage.empty())
    return;
  pushUndo();

  // Analyze the preview to auto-set levels
  const cv::Mat &source =
      m_previewImage.empty() ? m_sourceImage : m_previewImage;

  // Convert to grayscale for analysis
  cv::Mat gray;
  if (source.channels() == 3)
    cv::cvtColor(source, gray, cv::COLOR_RGB2GRAY);
  else
    gray = source;

  // Calculate mean brightness → auto exposure
  cv::Scalar meanVal = cv::mean(gray);
  double brightness = meanVal[0]; // 0.0-1.0 for float images
  // Target: ~0.45 brightness (slightly under 0.5 for natural look)
  double exposureDelta = (0.45 - brightness) * 2.0;
  exposureDelta = std::clamp(exposureDelta, -0.5, 0.5);

  // Calculate histogram for contrast analysis
  double minVal, maxVal;
  cv::minMaxLoc(gray, &minVal, &maxVal);
  double range = maxVal - minVal;
  // If range is narrow, increase contrast
  double contrastDelta = (range < 0.7) ? 0.15 : 0.0;

  // Auto shadows: if dark areas are too dark, lift shadows
  double shadowLift = (minVal < 0.05) ? 0.20 : 0.0;

  // Auto highlights: if bright areas are clipped, recover
  double highlightRecovery = (maxVal > 0.95) ? 0.20 : 0.0;

  // Light sharpening and vibrance
  double sharpening = 25.0;
  double vibrance = 0.10;

  // Block signals and set slider values
  m_exposureSlider->blockSignals(true);
  m_contrastSlider->blockSignals(true);
  m_shadowsSlider->blockSignals(true);
  m_highlightsSlider->blockSignals(true);
  m_sharpeningSlider->blockSignals(true);
  m_vibranceSlider->blockSignals(true);

  m_exposureSlider->setValue((int)(exposureDelta * 100));
  m_contrastSlider->setValue((int)(contrastDelta * 100));
  m_shadowsSlider->setValue((int)(shadowLift * 100));
  m_highlightsSlider->setValue((int)(-highlightRecovery * 100));
  m_sharpeningSlider->setValue((int)sharpening);
  m_vibranceSlider->setValue((int)(vibrance * 100));

  m_exposureSlider->blockSignals(false);
  m_contrastSlider->blockSignals(false);
  m_shadowsSlider->blockSignals(false);
  m_highlightsSlider->blockSignals(false);
  m_sharpeningSlider->blockSignals(false);
  m_vibranceSlider->blockSignals(false);

  // Update value labels
  emit m_exposureSlider->valueChanged(m_exposureSlider->value());
  emit m_contrastSlider->valueChanged(m_contrastSlider->value());
  emit m_shadowsSlider->valueChanged(m_shadowsSlider->value());
  emit m_highlightsSlider->valueChanged(m_highlightsSlider->value());
  emit m_sharpeningSlider->valueChanged(m_sharpeningSlider->value());
  emit m_vibranceSlider->valueChanged(m_vibranceSlider->value());

  // Update settings and preview
  m_settings.exposure = 1.0 + m_exposureSlider->value() * 0.01;
  m_settings.contrast = 1.0 + m_contrastSlider->value() * 0.01;
  m_settings.shadows = m_shadowsSlider->value() * 0.01;
  m_settings.highlights = m_highlightsSlider->value() * 0.01;
  m_settings.sharpening = qMax(0, m_sharpeningSlider->value());
  m_settings.vibrance = m_vibranceSlider->value() * 0.01;

  updatePreview();
  ToastWidget::show(this, "Auto enhanced", ToastWidget::Success);
}

// ════════════════════════════════════════════════
//  Before / After Split-View
// ════════════════════════════════════════════════

void HdrStudioPage::onToggleSplitView() {
  m_splitViewActive = !m_splitViewActive;
  m_splitPos = 0.5;
  if (m_splitViewBtn)
    m_splitViewBtn->setChecked(m_splitViewActive);
  updatePreview();
  ToastWidget::show(this,
                    m_splitViewActive ? "Split-view ON" : "Split-view OFF",
                    ToastWidget::Info);
}

void HdrStudioPage::mousePressEvent(QMouseEvent *event) {
  QWidget::mousePressEvent(event);
}

void HdrStudioPage::mouseMoveEvent(QMouseEvent *event) {
  QWidget::mouseMoveEvent(event);
}

void HdrStudioPage::mouseReleaseEvent(QMouseEvent *event) {
  QWidget::mouseReleaseEvent(event);
}

void HdrStudioPage::wheelEvent(QWheelEvent *event) {
  // Ctrl+Scroll zoom is handled by the eventFilter on the viewport
  QWidget::wheelEvent(event);
}

void HdrStudioPage::onToggleGrid() {
  m_gridOverlay = !m_gridOverlay;
  updatePreview();
  ToastWidget::show(this, m_gridOverlay ? "Grid ON" : "Grid OFF",
                    ToastWidget::Info);
}

void HdrStudioPage::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  menu.setStyleSheet(
      "QMenu { background: #1e1e21; border: 1px solid "
      "rgba(255,255,255,0.08); "
      "border-radius: 8px; padding: 4px; }"
      "QMenu::item { color: #a1a1aa; padding: 6px 24px 6px 12px; "
      "border-radius: 4px; font-size: 11px; }"
      "QMenu::item:selected { background: rgba(245,158,11,0.15); "
      "color: #f4f4f5; }"
      "QMenu::separator { height: 1px; background: rgba(255,255,255,0.06); "
      "margin: 4px 8px; }");

  // File
  menu.addAction(QString(QChar(0x1F4C2)) + "  Open images  (Ctrl+O)", this,
                 &HdrStudioPage::onLoadImages);
  menu.addAction(QString(QChar(0x1F4BE)) + "  Export  (Ctrl+E)", this,
                 &HdrStudioPage::onExport);
  menu.addSeparator();

  // Transform
  menu.addAction(QString(QChar(0x21BB)) + "  Rotate 90\u00b0 CW  (R)", this,
                 &HdrStudioPage::onRotateCW);
  menu.addAction(QString(QChar(0x2194)) + "  Flip Horizontal", this,
                 &HdrStudioPage::onFlipH);
  menu.addAction(QString(QChar(0x2195)) + "  Flip Vertical", this,
                 &HdrStudioPage::onFlipV);
  menu.addSeparator();

  // Adjust
  menu.addAction(QString(QChar(0x2728)) + "  Auto Enhance  (A)", this,
                 &HdrStudioPage::onAutoEnhance);
  menu.addAction(QString(QChar(0x21BA)) + "  Reset All", this,
                 &HdrStudioPage::onResetAll);
  menu.addSeparator();

  // View
  menu.addAction(QString(QChar(0x2795)) + "  Zoom In  (+)", this,
                 &HdrStudioPage::onZoomIn);
  menu.addAction(QString(QChar(0x2796)) + "  Zoom Out  (-)", this,
                 &HdrStudioPage::onZoomOut);
  menu.addAction(QString(QChar(0x2B1C)) + "  Fit  (0)", this,
                 &HdrStudioPage::onZoomFit);
  QAction *gridAction = menu.addAction(QString(QChar(0x25A6)) + "  Grid  (G)");
  connect(gridAction, &QAction::triggered, this, &HdrStudioPage::onToggleGrid);
  gridAction->setCheckable(true);
  gridAction->setChecked(m_gridOverlay);
  menu.addAction(QString(QChar(0x25E7)) + "  Before / After  (Space)", this,
                 &HdrStudioPage::onToggleSplitView);
  QAction *clipAction = menu.addAction("Clipping  (J)");
  connect(clipAction, &QAction::triggered, this,
          &HdrStudioPage::onToggleClipping);
  clipAction->setCheckable(true);
  clipAction->setChecked(m_showClipping);

  menu.exec(event->globalPos());
}

// ════════════════════════════════════════════════
//  Clipping / WB Picker / Presets
// ════════════════════════════════════════════════

void HdrStudioPage::onToggleClipping() {
  m_showClipping = !m_showClipping;
  if (m_clippingBtn)
    m_clippingBtn->setChecked(m_showClipping);
  updatePreview();
  ToastWidget::show(this, m_showClipping ? "Clipping ON" : "Clipping OFF",
                    ToastWidget::Info);
}

void HdrStudioPage::onWbPickerToggle() {
  m_wbPickerActive = !m_wbPickerActive;
  if (m_wbPickerBtn)
    m_wbPickerBtn->setChecked(m_wbPickerActive);
  m_graphicsView->viewport()->setCursor(m_wbPickerActive ? Qt::CrossCursor
                                                         : Qt::ArrowCursor);
  ToastWidget::show(this,
                    m_wbPickerActive ? "WB Picker: click a neutral gray area"
                                     : "WB Picker OFF",
                    ToastWidget::Info);
}

void HdrStudioPage::onSavePreset() {
  QString presetsDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/presets";
  QDir().mkpath(presetsDir);

  QString path = QFileDialog::getSaveFileName(
      this, "Save Preset", presetsDir + "/preset.json", "JSON (*.json)");
  if (path.isEmpty())
    return;

  QJsonObject obj;
  obj["exposure"] = m_settings.exposure;
  obj["contrast"] = m_settings.contrast;
  obj["clarity"] = m_settings.clarity;
  obj["texture"] = m_settings.texture;
  obj["highlights"] = m_settings.highlights;
  obj["shadows"] = m_settings.shadows;
  obj["whites"] = m_settings.whites;
  obj["blacks"] = m_settings.blacks;
  obj["temperature"] = m_settings.temperature;
  obj["tint"] = m_settings.tint;
  obj["vibrance"] = m_settings.vibrance;
  obj["saturation"] = m_settings.saturation;
  obj["dehaze"] = m_settings.dehaze;
  obj["vignette"] = m_settings.vignette;
  obj["sharpening"] = m_settings.sharpening;
  obj["noiseReduction"] = m_settings.noiseReduction;

  // HSL arrays
  QJsonArray hslH, hslS, hslL;
  for (int i = 0; i < 8; ++i) {
    hslH.append(m_settings.hslHue[i]);
    hslS.append(m_settings.hslSat[i]);
    hslL.append(m_settings.hslLum[i]);
  }
  obj["hslHue"] = hslH;
  obj["hslSat"] = hslS;
  obj["hslLum"] = hslL;

  // Color Grading
  obj["cgShadowHue"] = m_settings.cgShadowHue;
  obj["cgShadowSat"] = m_settings.cgShadowSat;
  obj["cgMidtoneHue"] = m_settings.cgMidtoneHue;
  obj["cgMidtoneSat"] = m_settings.cgMidtoneSat;
  obj["cgHighlightHue"] = m_settings.cgHighlightHue;
  obj["cgHighlightSat"] = m_settings.cgHighlightSat;

  QFile file(path);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
    ToastWidget::show(
        this, QString("Preset saved: %1").arg(QFileInfo(path).fileName()),
        ToastWidget::Success);
  } else {
    ToastWidget::show(this, "Failed to save preset", ToastWidget::Error);
  }
}

void HdrStudioPage::onLoadPreset() {
  QString presetsDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/presets";

  QString path = QFileDialog::getOpenFileName(this, "Load Preset", presetsDir,
                                              "JSON (*.json)");
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    ToastWidget::show(this, "Failed to open preset", ToastWidget::Error);
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (!doc.isObject()) {
    ToastWidget::show(this, "Invalid preset file", ToastWidget::Error);
    return;
  }

  pushUndo();
  QJsonObject obj = doc.object();

  m_settings.exposure = obj["exposure"].toDouble(1.0);
  m_settings.contrast = obj["contrast"].toDouble(1.0);
  m_settings.clarity = obj["clarity"].toDouble(0.0);
  m_settings.texture = obj["texture"].toDouble(0.0);
  m_settings.highlights = obj["highlights"].toDouble(0.0);
  m_settings.shadows = obj["shadows"].toDouble(0.0);
  m_settings.whites = obj["whites"].toDouble(0.0);
  m_settings.blacks = obj["blacks"].toDouble(0.0);
  m_settings.temperature = obj["temperature"].toDouble(0.0);
  m_settings.tint = obj["tint"].toDouble(0.0);
  m_settings.vibrance = obj["vibrance"].toDouble(0.0);
  m_settings.saturation = obj["saturation"].toDouble(1.0);
  m_settings.dehaze = obj["dehaze"].toDouble(0.0);
  m_settings.vignette = obj["vignette"].toDouble(0.0);
  m_settings.sharpening = obj["sharpening"].toDouble(0.0);
  m_settings.noiseReduction = obj["noiseReduction"].toDouble(0.0);

  // HSL arrays
  QJsonArray hslH = obj["hslHue"].toArray();
  QJsonArray hslS = obj["hslSat"].toArray();
  QJsonArray hslL = obj["hslLum"].toArray();
  for (int i = 0; i < 8; ++i) {
    m_settings.hslHue[i] = (i < hslH.size()) ? hslH[i].toDouble() : 0.0;
    m_settings.hslSat[i] = (i < hslS.size()) ? hslS[i].toDouble() : 0.0;
    m_settings.hslLum[i] = (i < hslL.size()) ? hslL[i].toDouble() : 0.0;
  }

  // Color Grading
  m_settings.cgShadowHue = obj["cgShadowHue"].toDouble(0.0);
  m_settings.cgShadowSat = obj["cgShadowSat"].toDouble(0.0);
  m_settings.cgMidtoneHue = obj["cgMidtoneHue"].toDouble(0.0);
  m_settings.cgMidtoneSat = obj["cgMidtoneSat"].toDouble(0.0);
  m_settings.cgHighlightHue = obj["cgHighlightHue"].toDouble(0.0);
  m_settings.cgHighlightSat = obj["cgHighlightSat"].toDouble(0.0);

  // Sync sliders to loaded settings (block signals then set values)
  auto setSlider = [](QSlider *s, int val) {
    s->blockSignals(true);
    s->setValue(val);
    s->blockSignals(false);
    emit s->valueChanged(val);
  };

  setSlider(m_exposureSlider, (int)((m_settings.exposure - 1.0) * 100));
  setSlider(m_contrastSlider, (int)((m_settings.contrast - 1.0) * 100));
  setSlider(m_claritySlider, (int)(m_settings.clarity * 100));
  setSlider(m_textureSlider, (int)(m_settings.texture * 100));
  setSlider(m_highlightsSlider, (int)(m_settings.highlights * 100));
  setSlider(m_shadowsSlider, (int)(m_settings.shadows * 100));
  setSlider(m_whitesSlider, (int)(m_settings.whites * 100));
  setSlider(m_blacksSlider, (int)(m_settings.blacks * 100));
  setSlider(m_temperatureSlider, (int)m_settings.temperature);
  setSlider(m_tintSlider, (int)m_settings.tint);
  setSlider(m_vibranceSlider, (int)(m_settings.vibrance * 100));
  setSlider(m_saturationSlider, (int)((m_settings.saturation - 1.0) * 100));
  setSlider(m_dehazeSlider, (int)(m_settings.dehaze * 100));
  setSlider(m_vignetteSlider, (int)(m_settings.vignette * 100));
  setSlider(m_sharpeningSlider, (int)m_settings.sharpening);
  setSlider(m_noiseSlider, (int)m_settings.noiseReduction);

  // Sync HSL wheel widget
  m_hslWheel->blockSignals(true);
  for (int i = 0; i < 8; ++i) {
    m_hslWheel->setValues(i, m_settings.hslHue[i], m_settings.hslSat[i],
                          m_settings.hslLum[i]);
  }
  m_hslWheel->blockSignals(false);

  m_cgShadowWheel->blockSignals(true);
  m_cgShadowWheel->setColor(m_settings.cgShadowHue, m_settings.cgShadowSat);
  m_cgShadowWheel->blockSignals(false);

  m_cgMidtoneWheel->blockSignals(true);
  m_cgMidtoneWheel->setColor(m_settings.cgMidtoneHue, m_settings.cgMidtoneSat);
  m_cgMidtoneWheel->blockSignals(false);

  m_cgHighlightWheel->blockSignals(true);
  m_cgHighlightWheel->setColor(m_settings.cgHighlightHue,
                               m_settings.cgHighlightSat);
  m_cgHighlightWheel->blockSignals(false);

  if (!m_sourceImage.empty())
    updatePreview();

  ToastWidget::show(
      this, QString("Preset loaded: %1").arg(QFileInfo(path).fileName()),
      ToastWidget::Success);
}
