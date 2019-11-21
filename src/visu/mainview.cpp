#include <QDir>
#include <QFileDialog>

#include <QGraphicsScene>
#include <QEvent>
#include <QMouseEvent>

#include <QScrollBar>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"
#include "controller.h"

#include <QDebug>

namespace gui {

MainView::MainView(const simu::Environment &e, QWidget *parent)
  : QGraphicsView(parent), _scene(new QGraphicsScene(parent)),
    _env(new Environment(e)), _selection(nullptr), _zoom(1) {

  setScene(_scene);
  setDragMode(ScrollHandDrag);
  new Graphics_view_zoom(this);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  _scene->addItem(_env);

  setRenderHint(QPainter::Antialiasing, true);

  installEventFilter(this);
}

void MainView::setController(visu::Controller *c) {
  _controller = c;
}

void MainView::updateEnvironment (void) {
  _env->updateBounds();
  _scene->setSceneRect(_env->boundingRect());
}

void MainView::addPlantItem(simu::Plant &sp, phylogeny::SID species) {
  Plant *gp = new Plant(sp, species);
  _plants[sp.pos().x] = gp;
  _scene->addItem(gp);

  connect(gp, &Plant::selected, this, &MainView::updateSelection);
}

void MainView::updatePlantItem (simu::Plant &sp) {
  auto it = _plants.find(sp.pos().x);
  assert(it != _plants.end());
  Plant *p = *it;
  assert(p);
  p->updateGeometry();
}

void MainView::delPlantItem(simu::Plant &sp) {
  auto it = _plants.find(sp.pos().x);
  assert(it != _plants.end());
  gui::Plant *vp = *it;
  assert(vp);
  if (vp == _selection)  selectNextPlant();
  _plants.erase(it);
  _scene->removeItem(vp);
  vp->deleteLater();
}

void MainView::update(void) {
  _env->update();
  for (Plant *p: _plants) {
    assert(p);
    p->updatePlantData();

    if (p == _selection && _selection->isSelected())  focusOnSelection();
  }
}

void MainView::speciesHovered(phylogeny::SID sid, bool hovered) {
  for (Plant *p: _plants) {
    if (hovered)
      p->setHighlighted(p->species() == sid ? PlantHighlighting::ON
                                            : PlantHighlighting::OFF);
    else
      p->setHighlighted(PlantHighlighting::IGNORED);
  }
}

bool MainView::eventFilter(QObject*, QEvent *event) {
  if (event->type() == QEvent::Resize)
    _viewportCenter = mapToScene(viewport()->geometry().center());
  return false;
}

void MainView::resizeEvent (QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  centerOn(_viewportCenter);
}

void MainView::mouseMoveEvent(QMouseEvent *e) {
  _controller->updateMousePosition(mapToScene(e->pos()));
  QGraphicsView::mouseMoveEvent(e);
}

void MainView::mouseDoubleClickEvent(QMouseEvent *e) {
  updateSelection(nullptr);
  QGraphicsView::mouseDoubleClickEvent(e);
}

void MainView::selectPreviousPlant(void) {
  if (!_plants.empty()) {
    Plant *p = nullptr;
    if (!_selection)  p = _plants.last();
    else {
      auto itBase = _plants.find(_selection->pos().x()), it = itBase;
      do {
        if (it != _plants.begin())
          --it;
        else
          it = std::prev(_plants.end());
        p = *it;
      } while (p->plant().isInSeedState() && it != itBase);
    }
    updateSelection(p);
  }
}

void MainView::selectNextPlant(void) {
  if (!_plants.empty()) {
    Plant *p = nullptr;
    if (!_selection)  p = _plants.first();
    else {
      auto itBase = _plants.find(_selection->pos().x()), it = itBase;
      do {
        ++it;
        if (it == _plants.end())  it = _plants.begin();
        p = *it;
      } while (p->plant().isInSeedState() && it != itBase);
    }
    updateSelection(p);
  }
}

void MainView::updateSelection(Plant *that) {
  if (_selection) {
    _selection->setSelected(false);
    _selection->update();
  }
  _selection = that;
  if (_selection) {
    _selection->setSelected(true);
    _selection->update();
  }
  focusOnSelection();
}

void MainView::focusOnSelection(void) {
  if (!_selection)  return;
  fitInView(_selection->boundingRect().translated(_selection->pos()),
            Qt::KeepAspectRatio);
  scale(_zoom, _zoom);
  _controller->updateMousePosition(mapToScene(_selection->pos().toPoint()));
}

void MainView::paintEvent(QPaintEvent *e) {
  QGraphicsView::paintEvent(e);

  static constexpr float M = 10;
  static constexpr float H = 10;

  QPainter painter (viewport());

  QPointF p0 = rect().bottomLeft();
  p0 += QPointF(M, - M - .5*H - horizontalScrollBar()->height());

  QPointF p1 = p0 + QPointF(.1*rect().width(), 0);

  painter.drawLine(p0, p1);
  painter.drawLine(p0 + QPointF(0, -.5*H), p0 + QPointF(0, .5*H));
  painter.drawLine(p1 + QPointF(0, -.5*H), p1 + QPointF(0, .5*H));

  qreal width = mapToScene(p1.toPoint()).x() - mapToScene(p0.toPoint()).x();
  QString unit = "m";

  if (width < 1) {
    width *= 100.;
    unit = "cm";
  }
  if (width < 1) {
    width *= 10.;
    unit = "mm";
  }

  painter.drawText(QRectF(p0.x(), p0.y() -1.5*H, p1.x() - p0.x(), 1.5*H),
                   Qt::AlignCenter, QString::number(width, 'f', 2) + unit);
}

QPixmap MainView::screenshot(QSize size) {
  QRect src = viewport()->rect();

  if (!size.isValid() || size.isNull())
    size = src.size();

  else {
    if (size.width() > size.height())
      size.setHeight(size.width() * src.height() / src.width());
    else
      size.setWidth(size.height() * src.width() / src.height());
  }

  QPixmap pixmap (size);
  QPainter painter (&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  render(&painter, pixmap.rect(), src);

  return pixmap;
}

QPixmap MainView::fullScreenShot(void) {
  QSize size = QSize(1680,1050);

  QRect src = sceneRect().toRect();

  if (!size.isValid() || size.isNull())
    size = src.size();

  else {
    if (size.width() > size.height())
      size.setHeight(size.width() * src.height() / src.width());
    else
      size.setWidth(size.height() * src.width() / src.height());
  }

  QPixmap pixmap (size);
  QPainter painter (&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  scene()->render(&painter, pixmap.rect(), src);

  return pixmap;
}

void MainView::saveMorphologiesInto(void) {
  QString folder = QFileDialog::getExistingDirectory(this,
                                                     "Save morphologies into");
  if (!folder.isEmpty())  saveMorphologies(folder);
}

struct FormatP {
  double v;

  friend std::ostream& operator<< (std::ostream &os, const FormatP &f) {
    return os << std::setw(7) << std::setprecision(3) << std::setfill('0')
              << std::fixed << 100.0 * f.v;
  }
};

struct FormatA {
  double v;

  FormatA (const simu::Plant &p)
    : v(double(p.age()) / p.genome().dethklok) {}

  friend std::ostream& operator<< (std::ostream &os, const FormatA &f) {
    return os << std::setw(5) << std::setprecision(3) << std::setfill('0')
              << std::fixed << f.v;
  }
};

void MainView::saveMorphologies (const QString &dir, int width) {
  using Layer = simu::Plant::Layer;

  std::map<std::string, std::pair<gui::Plant*, uint>> map;
  float total = _plants.size(), plants = 0;
  float seeds = 0;

  size_t maxw = 0;

  std::cout << "Building nested map\r";
  for (Plant *p: _plants) {
    if (p->plant().isInSeedState()) {
      seeds++;
      continue;
    } else
      plants++;

    std::ostringstream poss;
    poss << "[+++" << p->plant().toString(Layer::SHOOT)
         << "][---" << p->plant().toString(Layer::ROOT)
         << "]";

    std::string phenotype = poss.str();
    maxw = std::max(maxw, phenotype.size());

    std::cout << simu::PlantID(&p->plant())
              << ", phenotype: " << phenotype
              << std::string(maxw, ' ') << "\r";

    auto it = map.find(phenotype);
    if (it == map.end())
      map[phenotype] = std::make_pair(p, 1);
    else
      it->second.second++;
  }

  std::map<uint, uint> duplicates;

  for (auto &ppair: map) {
    uint count = ppair.second.second;
    uint duplicate = 1;

    auto it = duplicates.find(count);
    if (it == duplicates.end())
      duplicates[count] = duplicate;
    else
      duplicate = ++it->second;

    Plant *plant = ppair.second.first;

    std::ostringstream oss;
    oss << dir.toStdString() << "/"
        << FormatP{count / plants} << "p"
        << "_" << plant->plant().organs().size() << "o"
        << "_" << FormatA(plant->plant()) << "a";

    if (duplicate > 1)  oss << "_" << duplicate;
    std::string basename = oss.str();

    QString isolatedFile = QString::fromStdString(basename) + "_isolated.png";
    std::cout << "Rendering " << isolatedFile.toStdString()
              << std::string(maxw, ' ') << "\r";
    plant->renderTo(isolatedFile, width);


    QString inplaceFile = QString::fromStdString(basename) + "_inplace.png";
    std::cout << "Rendering " << inplaceFile.toStdString()
              << std::string(maxw, ' ') << "\r";

    QRectF pbounds = plant->boundingRect();
    QSizeF psize = pbounds.size();
    int height = width * psize.height() / psize.width();
    QPixmap image (width, height);
    image.fill(Qt::transparent);

    QPainter painter (&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    updateSelection(plant);
    render(&painter, image.rect(),
           mapFromScene(plant->boundingRect().translated(plant->pos())).boundingRect(),
           Qt::KeepAspectRatioByExpanding);
    image.save(inplaceFile);
  }

  std::cout << "Ignored " << seeds << " (" << FormatP{seeds/total} << "%)"
            << std::endl;
}

void MainView::saveMorphologiesWithSpecies (const QString &dir, int width) {
  using SID = phylogeny::SID;
  using Layer = simu::Plant::Layer;

  std::map<SID, std::map<std::string, std::pair<gui::Plant*, uint>>> map;
  std::map<SID, uint> totals;
  float total = _plants.size(), plants = 0;
  float seeds = 0;

  size_t maxw = 0;

  std::cout << "Building nested map\r";
  for (Plant *p: _plants) {
    if (p->plant().isInSeedState()) {
      seeds++;
      continue;
    } else
      plants++;

    auto &smap = map[p->species()];
    std::ostringstream poss;
    poss << "[+++" << p->plant().toString(Layer::SHOOT)
         << "][---" << p->plant().toString(Layer::ROOT)
         << "]";

    std::string phenotype = poss.str();
    maxw = std::max(maxw, phenotype.size());

    std::cout << simu::PlantID(&p->plant()) << " from species " << p->species()
              << ", phenotype: " << phenotype
              << std::string(maxw, ' ') << "\r";

    auto it = smap.find(phenotype);
    if (it == smap.end())
      smap[phenotype] = std::make_pair(p, 1);
    else
      it->second.second++;
    totals[p->species()]++;
  }

  for (auto &spair: map) {
    SID species = spair.first;
    std::map<uint, uint> duplicates;

    for (auto &ppair: spair.second) {
      uint count = ppair.second.second;
      uint duplicate = 1;

      auto it = duplicates.find(count);
      if (it == duplicates.end())
        duplicates[count] = duplicate;
      else
        duplicate = ++it->second;

      Plant *plant = ppair.second.first;

      std::ostringstream oss;
      oss << dir.toStdString() << "/"
          << FormatP{count / plants} << "gp_"
          << "SID" << spair.first
          << "_" << FormatP{count / float(totals.at(species))} << "lp"
          << "_" << plant->plant().organs().size() << "o"
          << "_" << FormatA(plant->plant()) << "a";

      if (duplicate > 1)  oss << "_" << duplicate;
      std::string basename = oss.str();

      QString isolatedFile = QString::fromStdString(basename) + "_isolated.png";
      std::cout << "Rendering " << isolatedFile.toStdString()
                << std::string(maxw, ' ') << "\r";
      plant->renderTo(isolatedFile, width);


      QString inplaceFile = QString::fromStdString(basename) + "_inplace.png";
      std::cout << "Rendering " << inplaceFile.toStdString()
                << std::string(maxw, ' ') << "\r";

      QRectF pbounds = plant->boundingRect();
      QSizeF psize = pbounds.size();
      int height = width * psize.height() / psize.width();
      QPixmap image (width, height);
      image.fill(Qt::transparent);

      QPainter painter (&image);
      painter.setRenderHint(QPainter::Antialiasing, true);

      updateSelection(plant);
      render(&painter, image.rect(),
             mapFromScene(plant->boundingRect().translated(plant->pos())).boundingRect(),
             Qt::KeepAspectRatioByExpanding);
      image.save(inplaceFile);
    }
  }

  std::cout << "Ignored " << seeds << " (" << FormatP{seeds/total} << "%)"
            << std::endl;
}

} // end of namespace gui
