// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktopdepthpreview.h"

#include "depthplanes.h"

#include "effect/effecthandler.h"
#include "input_event.h"

#include <QAction>
#include <QDebug>
#include <QKeyEvent>

#include <KGlobalAccel>

#include <algorithm>
#include <utility>

namespace KWin {

namespace {
constexpr int failSafeTimeoutMs = 60000;
constexpr int wheelStep = 120;
} // namespace

DesktopDepthPreviewEffect::DesktopDepthPreviewEffect() {
  auto *toggleAction = new QAction(this);
  toggleAction->setAutoRepeat(false);
  toggleAction->setObjectName(QStringLiteral("ToggleDesktopDepthPreview"));
  toggleAction->setText(QStringLiteral("Toggle Desktop Depth Preview"));
  KGlobalAccel::self()->setGlobalShortcut(toggleAction,
                                          QKeySequence(Qt::META | Qt::Key_Z));
  connect(toggleAction, &QAction::triggered, this,
          &DesktopDepthPreviewEffect::toggle);

  m_failSafeTimer.setSingleShot(true);
  m_failSafeTimer.setInterval(failSafeTimeoutMs);
  connect(&m_failSafeTimer, &QTimer::timeout, this, [this]() { finish(); });

  const auto cancelIfActive = [this]() {
    if (m_active) {
      finish();
    }
  };
  connect(effects, &EffectsHandler::windowAdded, this, cancelIfActive);
  connect(effects, &EffectsHandler::windowClosed, this,
          [cancelIfActive](EffectWindow *) { cancelIfActive(); });
  connect(effects, &EffectsHandler::desktopChanged, this,
          [cancelIfActive](VirtualDesktop *, VirtualDesktop *, EffectWindow *,
                           LogicalOutput *) { cancelIfActive(); });
  connect(effects, &EffectsHandler::currentActivityChanged, this,
          cancelIfActive);
  connect(effects, &EffectsHandler::screenLockingChanged, this,
          [cancelIfActive](bool locked) {
            if (locked) {
              cancelIfActive();
            }
          });
}

DesktopDepthPreviewEffect::~DesktopDepthPreviewEffect() { finish(); }

bool DesktopDepthPreviewEffect::isActive() const { return m_active; }

QString DesktopDepthPreviewEffect::debug(const QString &parameter) const {
  auto *mutableThis = const_cast<DesktopDepthPreviewEffect *>(this);
  if (parameter == QLatin1StringView("enter")) {
    mutableThis->start();
  } else if (parameter == QLatin1StringView("cancel")) {
    mutableThis->finish();
  }

  return QStringLiteral("active=%1 depth=%2 planes=%3 windows=%4 minimized=%5")
      .arg(m_active)
      .arg(m_depth)
      .arg(m_planeCount)
      .arg(m_windows.size())
      .arg(m_minimizedVisibility.size());
}

void DesktopDepthPreviewEffect::toggle() {
  if (m_active) {
    finish();
  } else {
    start();
  }
}

void DesktopDepthPreviewEffect::start() {
  if (m_active || effects->isScreenLocked() ||
      effects->activeFullScreenEffect()) {
    return;
  }

  const QList<EffectWindow *> stackingOrder = effects->stackingOrder();
  QVector<EffectWindow *> minimizedWindows;
  minimizedWindows.reserve(stackingOrder.size());

  m_windows.clear();
  m_windows.reserve(stackingOrder.size());

  for (auto it = stackingOrder.crbegin(); it != stackingOrder.crend(); ++it) {
    EffectWindow *window = *it;
    if (!isCandidate(window)) {
      continue;
    }
    m_windows.push_back(window);
    if (window->isMinimized()) {
      minimizedWindows.push_back(window);
    }
  }

  if (m_windows.isEmpty()) {
    return;
  }
  if (!effects->grabKeyboard(this)) {
    m_windows.clear();
    return;
  }

  std::vector<DesktopDepthPreview::Rect> geometries;
  geometries.reserve(m_windows.size());
  for (const QPointer<EffectWindow> &window : std::as_const(m_windows)) {
    const RectF geometry = window->frameGeometry();
    geometries.push_back(
        {geometry.x(), geometry.y(), geometry.width(), geometry.height()});
  }

  const DesktopDepthPreview::PlaneAssignment assignment =
      DesktopDepthPreview::assignPlanes(geometries);
  m_planeByWindow.clear();
  for (qsizetype index = 0; index < m_windows.size(); ++index) {
    m_planeByWindow.insert(
        m_windows[index],
        static_cast<int>(assignment.planes[static_cast<std::size_t>(index)]));
  }

  m_minimizedVisibility.clear();
  m_minimizedVisibility.reserve(minimizedWindows.size());
  for (EffectWindow *window : minimizedWindows) {
    m_minimizedVisibility.push_back(std::make_unique<EffectWindowVisibleRef>(
        window, EffectWindow::PAINT_DISABLED_BY_MINIMIZE));
  }

  m_depth = 0;
  m_planeCount = static_cast<int>(assignment.planeCount);
  m_axisAccumulator = 0;
  m_active = true;
  effects->setActiveFullScreenEffect(this);
  effects->startMouseInterception(this, Qt::ArrowCursor);
  m_failSafeTimer.start();
  effects->addRepaintFull();
  qInfo().nospace() << "desktop-depth-preview: entered windows="
                    << m_windows.size()
                    << " minimized=" << minimizedWindows.size()
                    << " planes=" << m_planeCount;
}

void DesktopDepthPreviewEffect::finish(EffectWindow *selectedWindow) {
  if (!m_active) {
    return;
  }

  m_failSafeTimer.stop();
  effects->stopMouseInterception(this);
  effects->ungrabKeyboard();
  if (effects->activeFullScreenEffect() == this) {
    effects->setActiveFullScreenEffect(nullptr);
  }

  m_minimizedVisibility.clear();
  m_planeByWindow.clear();
  m_windows.clear();
  m_depth = 0;
  m_planeCount = 0;
  m_axisAccumulator = 0;
  m_active = false;
  effects->addRepaintFull();
  qInfo() << "desktop-depth-preview: exited";

  if (selectedWindow && !selectedWindow->isDeleted()) {
    selectedWindow->unminimize();
    effects->activateWindow(selectedWindow);
  }
}

bool DesktopDepthPreviewEffect::isCandidate(const EffectWindow *window) const {
  return window && !window->isDeleted() && window->isManaged() &&
         !window->isSpecialWindow() && !window->isSkipSwitcher() &&
         window->isOnCurrentDesktop() && window->isOnCurrentActivity() &&
         window->acceptsFocus() && window->frameGeometry().isValid();
}

bool DesktopDepthPreviewEffect::isHidden(EffectWindow *window) const {
  const auto plane = m_planeByWindow.constFind(window);
  return m_active && plane != m_planeByWindow.cend() && plane.value() < m_depth;
}

EffectWindow *
DesktopDepthPreviewEffect::windowAt(const QPointF &position) const {
  for (const QPointer<EffectWindow> &window : m_windows) {
    if (window && !isHidden(window) &&
        window->frameGeometry().contains(position)) {
      return window;
    }
  }
  return nullptr;
}

void DesktopDepthPreviewEffect::changeDepth(int delta) {
  const int maximumDepth = std::max(0, m_planeCount - 1);
  const int nextDepth = std::clamp(m_depth + delta, 0, maximumDepth);
  if (nextDepth == m_depth) {
    return;
  }
  m_depth = nextDepth;
  m_failSafeTimer.start();
  effects->addRepaintFull();
  qInfo().nospace() << "desktop-depth-preview: depth=" << m_depth << '/'
                    << maximumDepth;
}

void DesktopDepthPreviewEffect::prePaintWindow(RenderView *view,
                                               EffectWindow *window,
                                               WindowPrePaintData &data) {
  if (isHidden(window)) {
    data.setTranslucent();
  }
  effects->prePaintWindow(view, window, data);
}

void DesktopDepthPreviewEffect::paintWindow(const RenderTarget &renderTarget,
                                            const RenderViewport &viewport,
                                            EffectWindow *window, int mask,
                                            const Region &deviceRegion,
                                            WindowPaintData &data) {
  if (!isHidden(window)) {
    effects->paintWindow(renderTarget, viewport, window, mask, deviceRegion,
                         data);
  }
}

void DesktopDepthPreviewEffect::grabbedKeyboardEvent(QKeyEvent *event) {
  if (!m_active || event->type() != QEvent::KeyPress || event->isAutoRepeat()) {
    return;
  }

  switch (event->key()) {
  case Qt::Key_Escape:
    finish();
    break;
  case Qt::Key_Z:
    if (event->modifiers().testFlag(Qt::MetaModifier)) {
      finish();
    }
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    finish(windowAt(effects->cursorPos()));
    break;
  case Qt::Key_PageDown:
  case Qt::Key_Down:
    changeDepth(1);
    break;
  case Qt::Key_PageUp:
  case Qt::Key_Up:
    changeDepth(-1);
    break;
  default:
    break;
  }
}

void DesktopDepthPreviewEffect::pointerButton(PointerButtonEvent *event) {
  if (!m_active || event->state != PointerButtonState::Pressed) {
    return;
  }

  if (event->button == Qt::LeftButton) {
    finish(windowAt(event->position));
  } else if (event->button == Qt::RightButton ||
             event->button == Qt::MiddleButton) {
    finish();
  }
}

void DesktopDepthPreviewEffect::pointerAxis(PointerAxisEvent *event) {
  if (!m_active || event->orientation != Qt::Vertical) {
    return;
  }

  int delta = event->deltaV120;
  if (delta == 0 && event->delta != 0.0) {
    delta = event->delta > 0.0 ? wheelStep : -wheelStep;
  }
  if ((delta > 0 && m_axisAccumulator < 0) ||
      (delta < 0 && m_axisAccumulator > 0)) {
    m_axisAccumulator = 0;
  }
  m_axisAccumulator += delta;

  while (m_axisAccumulator >= wheelStep) {
    changeDepth(1);
    m_axisAccumulator -= wheelStep;
  }
  while (m_axisAccumulator <= -wheelStep) {
    changeDepth(-1);
    m_axisAccumulator += wheelStep;
  }
}

} // namespace KWin
