#ifndef _FAST_FORWARD_DIALOG_H_
#define _FAST_FORWARD_DIALOG_H_

#include <QtWidgets>

#include "../graphicsimulation.h"
#include "smoothdurations.hpp"

/// TODO This is not really working due to the computational cost staying in the main thread.

namespace gui {

//class FastForwardDialogWorker : public QObject {
//  Q_OBJECT

//  using Simulation = visu::GraphicSimulation;
//  Simulation &simulation;
//  bool aborted;
//  uint start, stop;

//  static const uint smoothDurationBufferSize;

//  SmoothDurationManager progressSpeed;

//public:
//  FastForwardDialogWorker(Simulation &simulation, uint start, uint stop)
//    : simulation(simulation), aborted(false), start(start), stop(stop),
//      progressSpeed(smoothDurationBufferSize) {}

//public slots:
//  void doWork (void);
//  void abort (void) { aborted = true; }

//signals:
//  void stepDone (uint step, int eta);
//  void done (uint step, double duration);
//};

class FastForwardDialog : public QDialog {
  Q_OBJECT

  QLineEdit *text;
  QSpinBox *spinner;

  QPushButton *ok, *cancel;

  QProgressBar *progress;

  using Simulation = visu::GraphicSimulation;
  Simulation &simulation;

  bool aborted;
  uint start, stop;

  static const uint smoothDurationBufferSize;

  SmoothDurationManager progressSpeed;

public:
  FastForwardDialog (QWidget *parent, Simulation &s);

  void keyPressEvent(QKeyEvent *e) {
    if(e->key() != Qt::Key_Escape)
      QDialog::keyPressEvent(e);
    else    cancel->animateClick();
  }

  void process (void);

private:
  QString prettySeconds (int duration);

public slots:
  void progressUpdate (uint step, int eta);
  void processComplete (uint step, double totalDuration);
};

} // end of namespace gui

#endif // _FAST_FORWARD_DIALOG_H_
