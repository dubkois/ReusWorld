#include <QGraphicsScene>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"

namespace gui {

MainView::MainView(QWidget *parent)
  : QGraphicsView(parent), _scene(new QGraphicsScene(parent)) {
  new Graphics_view_zoom(this);
  setScene(_scene);
}

void MainView::addPlantItem(const simu::Plant &sp) {
  Plant *gp = new Plant(sp);
  _map[sp.pos().x] = gp;
  _scene->addItem(gp);
}

void MainView::delPlantItem(float x) {
  Plant *p = _map.value(x);
  assert(p);
  _scene->removeItem(p);
}

} // end of namespace gui
