#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QGraphicsView>

#include "../simu/simulation.h"
#include "environment.h"
#include "plant.h"

namespace visu {  struct Controller;  }

namespace gui {

class MainView : public QGraphicsView {
  Q_OBJECT

  visu::Controller *_controller;

  QGraphicsScene *_scene;

  using PlantToItemMap = QMap<float,Plant*>;
  PlantToItemMap _plants;

  Environment *_env;

  /// Only used to keep the focal point in the center of the viewport when resizing
  QPointF _viewportCenter;

  Plant *_selection;
  float _zoom;

public:
  explicit MainView(const simu::Environment &e, QWidget *parent = nullptr);

  void setController (visu::Controller *c);

  void addPlantItem(simu::Plant &sp);
  void updatePlantItem(float x);
  void delPlantItem(float x);

  void mouseMoveEvent(QMouseEvent *e);

private:
  bool eventFilter (QObject*, QEvent *event) override;
  void resizeEvent (QResizeEvent *event) override;

signals:

public slots:
  void selectPreviousPlant (void);
  void selectNextPlant (void);

  void updateSelection(Plant *that);
  void focusOnSelection(void);

  void zoomIn (void) {
    _zoom *= 2.f;
    focusOnSelection();
  }

  void zoomOut (void) {
    _zoom *= .5f;
    focusOnSelection();
  }
};

} // end of namespace gui

#endif // MAINVIEW_H
