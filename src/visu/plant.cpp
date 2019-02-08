#include <QPainter>
#include <qmath.h>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>

#include "plant.h"
#include "qtconversions.hpp"

#include <QDebug>

namespace gui {

using GConfig = config::PlantGenome;

static constexpr bool drawOrganContour = true;
static constexpr bool drawQtBoundingBox = false;

static auto apexSize (void) {
  static const float s = GConfig::sizeOf('s').width;
  return s;
}

struct Plant::PlantMenu : public QMenu {
  Plant *plantVisu;

  PlantMenu (void) {
    QAction *del = addAction("delete");

    connect(del, &QAction::triggered, [this] {
      plantVisu->_plant.kill();
      plantVisu->update();
    });
  }

  void popup(const QPoint &pos, Plant *visu) {
    qDebug() << "PlantMenu::popup(" << pos << ", " << uint(visu->plant().id()) << ")";
    plantVisu = visu;
    QMenu::popup(pos);
  }
};

const auto& pathForSymbol (char symbol) {
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

  auto it = map.find(symbol);
  if (it == map.end())
    return defaultPath;
  return it->second;
}
const auto& pathForApex (void) {
  static const auto path = [] {
    QPainterPath p;
//    p.moveTo(0, -.5);
//    p.lineTo(1, 0);
//    p.lineTo(0, .5);
//    p.closeSubpath();
//    p.cubicTo({.33, -.5}, {.33,0}, {1, 0});
//    p.cubicTo({.33,0}, {.33,  .5}, {0, 0});
//    p.closeSubpath();
    p.addEllipse({.5,0}, .5, .25);
    return p;
  }();
  return path;
}

const auto& minBoundingBox (void) {
  static const auto box = [] {
    float AS = apexSize();
    return QRectF (-.5*AS, -AS, AS, 2*AS);
  }();
  return box;
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

Plant::Plant(simu::Plant &p) : _plant(p), _selected(false) {
  setPos(toQPoint(_plant.pos()));
  updateGeometry();
  updateTooltip();
}

void Plant::updateGeometry(void) {
  prepareGeometryChange();
  setPos(toQPoint(_plant.pos()));
  _boundingRect = toQRect(_plant.boundingRect())
                    .united(minBoundingBox());
  float m = .1 * std::min(_boundingRect.width(), _boundingRect.height());
  _boundingRect.adjust(-m, -m, m, m);
//  qDebug() << "Bounding rect: " << boundingRect();
}

void Plant::updateTooltip(void) {
  std::ostringstream oss;
  oss << _plant.genome();
  setToolTip(QString::fromStdString(oss.str()));
}

void Plant::updatePlantData(void) {
  updateGeometry();
  updateTooltip();
  update();
}

void Plant::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) {
  qDebug() << "Plant::mouseDoubleClickEvent(" << e->button() << "," << e->buttons() << ")";
  if (e->button() == Qt::LeftButton) {
    setSelected(true);
    std::cerr << "Genotype for plant(" << _plant.genome().cdata.id << "): "
              << _plant.genome()
              << "LSystem state: " << _plant
              << std::endl;

    emit selected(this);    
  }

  QGraphicsObject::mouseDoubleClickEvent(e);
}

void Plant::contextMenuEvent(QGraphicsSceneContextMenuEvent *e) {
  qDebug() << "Plant::contextMenuEvent()";
  contextMenu()->popup(e->screenPos(), this);
}

void paint(QPainter *painter, const simu::Organ *o, bool contour, bool dead) {
  for (simu::Organ *c: o->children())
    paint(painter, c, contour, dead);

  const auto &p = o->inPlantCoordinates();
  QPointF p0 = toQPoint(p.origin);
  float r = -qRadiansToDegrees(p.rotation);
  painter->save();
    painter->translate(p0);
    painter->rotate(r);

    const QPainterPath *path;
    if (o->length() == 0) {
      path = &pathForApex();

      static const float AS = apexSize();
      painter->scale(AS, AS);

    } else {
      path = &pathForSymbol(o->symbol());
      painter->scale(o->length(), o->width());
    }

    if (contour) painter->drawPath(*path);

    QColor c = color(o);
    if (dead)  desaturate(c);
    painter->fillPath(*path, c);

  painter->restore();
}

void Plant::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
  painter->save();
  QPen pen = painter->pen();
  pen.setWidth(0);
  painter->setPen(pen);

  painter->drawPoint(0,0);

  if (drawQtBoundingBox) {
    painter->save();
      QRectF r = boundingRect();
      pen.setColor(Qt::red);
      painter->setPen(pen);
      painter->drawRect(r);
    painter->restore();
  }

  bool doDrawOrganContour = drawOrganContour && isSelected();
  if (doDrawOrganContour) {
    pen.setColor(Qt::black);
    painter->setPen(pen);
  }

//  qDebug() << "Drawing: " << uint(_plant.genome().cdata.id) << _plant.age();
  bool dead = _plant.isDead();
  for (const simu::Organ *o: _plant.bases())
    gui::paint(painter, o, doDrawOrganContour, dead);

  painter->restore();
}

Plant::PlantMenu* Plant::contextMenu(void) {
  static PlantMenu menu;
  return &menu;
}

} // end of namespace gui
