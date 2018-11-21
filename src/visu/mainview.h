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
  PlantToItemMap _map;

  Environment *_env;

  /// Only used to keep the focal point in the center of the viewport when resizing
  QPointF _viewportCenter;

public:
  explicit MainView(const simu::Environment &e, QWidget *parent = nullptr);

  void setController (visu::Controller *c);

  void addPlantItem(const simu::Plant &sp);
  void delPlantItem(float x);

private:
  bool eventFilter (QObject*, QEvent *event) override;
  void resizeEvent (QResizeEvent *event) override;

signals:

public slots:
  void viewPlant (Plant *p);
  void viewPlant (float x) {
    viewPlant(_map.value(x));
  }
};

} // end of namespace gui

#endif // MAINVIEW_H
