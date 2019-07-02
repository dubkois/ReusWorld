#include <QPainter>
#include <qmath.h>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>

#include "plant.h"
#include "qtconversions.hpp"

#include <QDebug>

namespace gui {

using GConfig = config::PlantGenome;

static constexpr bool drawOrganID = false;
static constexpr bool drawPlantID = false;
static constexpr bool drawOrganContour = false;
static constexpr bool drawOrganCorners = false;
static constexpr bool drawQtBoundingBox = false;

struct Plant::PlantMenu : public QMenu {
  Plant *plantVisu;

  PlantMenu (void) {
    QAction *del = addAction("Delete");
    QAction *delConf = addAction("Confirm");
    addSeparator();
    QAction *photo = addAction("Photo");

    connect(del, &QAction::triggered, [this, delConf] {
      if (delConf->isChecked() &&
        QMessageBox::Ok != QMessageBox::question(
          parentWidget(), "Confirm?",
          QString("Do you want to delete plant ") + uint(plantVisu->_plant.id()) + "?",
          QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel))

          return;

      plantVisu->_plant.kill();
      plantVisu->update();
    });

    delConf->setCheckable(true);
    delConf->setChecked(true);

    connect(photo, &QAction::triggered, [this] {
      QString pid = QString::number(uint(plantVisu->_plant.id()), 16);
      QString filename = QFileDialog::getSaveFileName(
        parentWidget(),
        QString("Save picture of plant ") + pid,
        "./P" + pid + ".png", "Portable Network Graphic (*.png)");
      if (!filename.isEmpty())  plantVisu->renderTo(filename, 200);
    });
  }

  void popup(const QPoint &pos, Plant *visu) {
    plantVisu = visu;
    QMenu::popup(pos);
  }
};

const auto& pathForSymbol (char symbol) {
  static const auto apexPath = [] {
    QPainterPath p;
//    p.moveTo(0, -.5);
//    p.lineTo(1, 0);
//    p.lineTo(0, .5);
//    p.closeSubpath();
//    p.cubicTo({.33, -.5}, {.33,0}, {1, 0});
//    p.cubicTo({.33,0}, {.33,  .5}, {0, 0});
//    p.closeSubpath();
    p.addEllipse({.5,0}, .5, .5);
    return p;
  }();

  static const auto defaultPath = [] {
    QPainterPath p;
    p.moveTo(0, -.5);
    p.lineTo(0, .5);
    p.lineTo(1, .5);
    p.lineTo(1, -.5);
    p.closeSubpath();
    return p;
  }();

  static const auto map = [] {
    std::map<char, QPainterPath> m;
    m['l'] = [] {
      QPainterPath p;
      p.lineTo(.5, .5);
      p.lineTo(1, 0);
      p.lineTo(.5, -.5);
      p.closeSubpath();
      return p;
    }();
    m['f'] = [] {
      float w = 1;
      QPainterPath p;
      p.quadTo(0, -.5*w,  1,  -.5*w);
      p.quadTo(1, -.25*w, .5, -.16*w);
      p.quadTo(1, -.16*w, 1,   0);
      p.quadTo(1,  .16*w, .5,  .16*w);
      p.quadTo(1,  .25*w, 1,   .5*w);
      p.quadTo(0,  .5*w,  0,   0);
      p.closeSubpath();
      return p;
    }();
    m['g'] = [] {
      QPainterPath p;
      p.addEllipse({.5,0}, .5, .5);
      return p;
    }();
    return m;
  }();

  if (genotype::grammar::Rule_base::isTerminal(symbol)) {
    auto it = map.find(symbol);
    if (it == map.end())
      return defaultPath;
    return it->second;
  }
  return apexPath;
}

const QColor seedColor = QColor::fromRgbF(.33,.42,.18);
QColor color (const simu::Organ *o) {
  static const std::map<char, QColor> terminals {
    { 's', QColor::fromRgbF( .55,  .27,  .07) },
    { 'l', QColor(Qt::green) },
    { 'f', QColor::fromRgbF( .75, 0.,   0.) },

    { 't', QColor::fromRgbF( .82,  .71,  .55) },
    { 'h', QColor(Qt::gray) },
  };

  if (o->isNonTerminal())
    return mix(terminals.at('l'), terminals.at('s'), o->fullness());
  else if (o->isFlower())
    return mix(terminals.at('f'), terminals.at('s'), o->fullness());
  else if (o->isFruit())
    return mix(QColor::fromRgbF(.75, .75, 0.), terminals.at('s'), o->fullness());
  else
    return terminals.at(o->symbol());
}

void desaturate (QColor &c) {
  float s = c.saturationF();
  c = QColor::fromHsvF(c.hueF(), .25 * s, .5 * c.valueF());
}

Plant::Plant(simu::Plant &p, Species s)
  : _plant(p), _species(s), _selected(false),
    _highlighted(PlantHighlighting::IGNORED) {
  setPos(toQPoint(_plant.pos()));
  updateGeometry();
  updateTooltip();
}

void Plant::updateGeometry(void) {
  prepareGeometryChange();
  setPos(toQPoint(_plant.pos()));
  _boundingRect = toQRect(_plant.boundingRect());
  float m = .1 * _boundingRect.height();
  _boundingRect.adjust(-m, -2*m, m, m);
}

void Plant::updateTooltip(void) {
  std::ostringstream oss;
  oss << "Species " << _species << "\n" << _plant.genome();
  setToolTip(QString::fromStdString(oss.str()));
}

void Plant::updatePlantData(void) {
  updateGeometry();
  updateTooltip();
  update();
}

void Plant::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    setSelected(true);
    std::cerr << "Genotype for plant(" << _plant.id() << "): "
              << _plant.genome()
              << "LSystem state: " << _plant
              << std::endl;

    emit selected(this);    
  }

  QGraphicsObject::mouseDoubleClickEvent(e);
}

void Plant::contextMenuEvent(QGraphicsSceneContextMenuEvent *e) {
  contextMenu()->popup(e->screenPos(), this);
}

void Plant::paint (QPainter *painter, float scale, uint id,
                   Qt::GlobalColor stroke, const QColor &fill,
                   const simu::Point &p, float r, char s, float w, float l) {

  static const float baseWidth = GConfig::sizeOf('s').width;

  painter->save();
    painter->translate(toQPoint(p));
    painter->rotate(-qRadiansToDegrees(r));

    painter->save();
      const QPainterPath &path = pathForSymbol(s);
      float baseScale = (scale - 1) * std::min(l, w);
      painter->scale(baseScale + l, baseScale + w);

      if (stroke != Qt::transparent) {
        QPen pen = painter->pen();
        pen.setColor(stroke);
        painter->setPen(pen);
        painter->drawPath(path);
      }

      painter->fillPath(path, fill);

    painter->restore();

    if (drawOrganID && id != simu::Organ::OID::INVALID) {
      painter->save();
        QPen pen = painter->pen();
        pen.setColor(Qt::black);
        painter->setPen(pen);
        QFont font = painter->font();
        font.setPointSizeF(.5);
        painter->setFont(font);
        painter->translate(.5 * l, 0);
        painter->scale(.5 * baseWidth, .5 * baseWidth);
        painter->drawText(QRectF(-1,-.75,2,2), Qt::AlignCenter,
                          QString("_%1_").arg(id));
      painter->restore();
    }

  painter->restore();
}

void paint(QPainter *painter, const simu::Organ *o, bool dead,
           Qt::GlobalColor stroke = Qt::transparent, float scale = 1) {

  for (simu::Organ *c: o->children())
    paint(painter, c, dead, stroke, scale);

  const auto &p = o->inPlantCoordinates();

  QColor fill;
  if (scale > 1)  fill = stroke;
  else {
    fill = color(o);
    if (dead)  desaturate(fill);
  }

  Plant::paint(painter, scale, uint(o->id()),
               stroke, fill, p.origin, p.rotation,
               o->symbol(), o->width(), o->length());
}

void Plant::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
  painter->save();
  QPen pen = painter->pen();
  pen.setWidth(0);
  painter->setPen(pen);

  QRectF r = boundingRect();
  if (drawQtBoundingBox) {
    painter->save();
      pen.setColor(Qt::red);
      painter->setPen(pen);
      painter->drawRect(r);
    painter->restore();
  }

  bool dead = _plant.isDead();

  if (_highlighted == PlantHighlighting::ON) {
    for (const simu::Organ *o: _plant.bases())
      gui::paint(painter, o, dead, Qt::yellow, 1.5);

  } else if (_highlighted == PlantHighlighting::IGNORED){
    // To have something to look at even with little resolution
    painter->drawPoint(0,0);
  }

  pen.setColor(Qt::NoPen);
  painter->setPen(pen);
  for (const simu::Organ *o: _plant.bases())
    gui::paint(painter, o, dead);

  if (drawOrganCorners) {
    pen.setColor(Qt::white);
    painter->setPen(pen);
    for (const simu::Organ *o: _plant.organs()) {
      QPolygonF box;
      for (const simu::Point &p: o->inPlantCoordinates().corners)
        box.append(toQPoint(p));
      painter->drawPolygon(box);
    }
  }

  if (isSelected()) {
    pen.setColor(Qt::black);
    painter->setPen(pen);
    for (const simu::Organ *o: _plant.bases())
      gui::paint(painter, o, dead, Qt::black);
  }

  if (drawPlantID) {
    painter->save();
      QPen pen = painter->pen();
      pen.setColor(Qt::black);
      painter->setPen(pen);
      QFont font = painter->font();
      font.setPointSizeF(.5);
      painter->setFont(font);
      painter->scale(.1 * r.width(), .1 * r.height());
      painter->drawText(QRectF(10 * r.left() / r.width(),
                               10 * r.top() / r.height(),
                               10, 2),
                        Qt::AlignCenter,
                        QString("P%1").arg(uint(_plant.id())));
    painter->restore();
  }

  painter->restore();
}

void Plant::renderTo(const QString &filename, int width) const {
  QRectF pbounds = boundingRect();
  QSizeF psize = pbounds.size();
  int height = width * psize.height() / psize.width();

  QPixmap image (width, height);
  image.fill(Qt::transparent);

  QPainter painter (&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.scale(width / psize.width(), height / psize.height());
  painter.translate(-pbounds.left(), -pbounds.top());

  for (const simu::Organ *o: _plant.bases())  gui::paint(&painter, o, false);

  {
    float x0 = pbounds.left(), x1 = pbounds.right();
    float y = pbounds.bottom() - .1 * pbounds.height();
    painter.drawLine(x0, y, x1, y);
  }

  image.save(filename);
}

Plant::PlantMenu* Plant::contextMenu(void) {
  static PlantMenu menu;
  return &menu;
}

} // end of namespace gui
