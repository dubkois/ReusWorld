#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QGraphicsView>

#include "../simu/simulation.h"
#include "plant.h"

namespace gui {

class MainView : public QGraphicsView {
  Q_OBJECT

  QGraphicsScene *_scene;

  using PlantToItemMap = QMap<float,Plant*>;
  PlantToItemMap _map;

public:
  explicit MainView(QWidget *parent = nullptr);

  void addPlantItem(const simu::Plant &sp);
  void delPlantItem(float x);

signals:

public slots:
};

} // end of namespace gui

#endif // MAINVIEW_H
