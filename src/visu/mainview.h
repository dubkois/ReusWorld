#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QGraphicsView>

#include "../simu/simulation.h"
#include "environment.h"
#include "plant.h"

struct QDir;

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

  void updateEnvironment (void);

  void addPlantItem(simu::Plant &sp, phylogeny::SID species);
  void updatePlantItem (simu::Plant &sp);
  void delPlantItem(simu::Plant &sp);

  void speciesHovered (phylogeny::SID sid, bool hovered);

  void mouseMoveEvent(QMouseEvent *e);
  void mouseDoubleClickEvent(QMouseEvent *e);

private:
  bool eventFilter (QObject*, QEvent *event) override;
  void resizeEvent (QResizeEvent *event) override;

  void paintEvent(QPaintEvent *e) override;

signals:

public slots:
  void update (void);

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

  QPixmap screenshot (QSize size = QSize());

  void saveMorphologiesAs (void);
  void saveMorphologies (const QString &dir, int width = 200);
};

} // end of namespace gui

#endif // MAINVIEW_H
