#include <QPainter>

#include "environment.h"

namespace gui {

const QColor sky = QColor::fromRgbF(.13, .5, .7);
const QColor ground = QColor::fromRgbF(.5, .4, .3);

void Environment::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  painter->fillRect(-_object.xextent(), -_object.yextent(),
                    _object.width(), _object.yextent(), sky);

  painter->fillRect(-_object.xextent(), 0,
                    _object.width(), _object.yextent(), ground);
}

} // end of namespace gui
