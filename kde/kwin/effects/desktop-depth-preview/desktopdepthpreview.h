// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "effect/effect.h"
#include "effect/effectwindow.h"

#include <QHash>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <vector>

namespace KWin {

class DesktopDepthPreviewEffect : public Effect {
  Q_OBJECT

public:
  DesktopDepthPreviewEffect();
  ~DesktopDepthPreviewEffect() override;

  bool isActive() const override;
  QString debug(const QString &parameter) const override;
  void prePaintWindow(RenderView *view, EffectWindow *window,
                      WindowPrePaintData &data) override;
  void paintWindow(const RenderTarget &renderTarget,
                   const RenderViewport &viewport, EffectWindow *window,
                   int mask, const Region &deviceRegion,
                   WindowPaintData &data) override;
  void grabbedKeyboardEvent(QKeyEvent *event) override;
  void pointerButton(PointerButtonEvent *event) override;
  void pointerAxis(PointerAxisEvent *event) override;

private:
  void toggle();
  void start();
  void finish(EffectWindow *selectedWindow = nullptr);
  bool isCandidate(const EffectWindow *window) const;
  bool isHidden(EffectWindow *window) const;
  EffectWindow *windowAt(const QPointF &position) const;
  void changeDepth(int delta);

  bool m_active = false;
  int m_depth = 0;
  int m_planeCount = 0;
  int m_axisAccumulator = 0;
  QVector<QPointer<EffectWindow>> m_windows;
  QHash<EffectWindow *, int> m_planeByWindow;
  std::vector<std::unique_ptr<EffectWindowVisibleRef>> m_minimizedVisibility;
  QTimer m_failSafeTimer;
};

} // namespace KWin
