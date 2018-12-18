#include "fastforwarddialog.h"

namespace gui {

//const uint FastForwardDialogWorker::smoothDurationBufferSize = 100;

//void FastForwardDialogWorker::doWork (void) {
////  QTime globalTimer;
////  globalTimer.start();
////  progressSpeed.restart();

//  uint i;
//  for (i=start; i<stop && !aborted && !simulation.finished(); i++) {
//    simulation.step();
////    QCoreApplication::processEvents();

//    int eta = 0;//(stop - i) * progressSpeed.smoothDuration() / 1000;
//    emit stepDone(i, eta);
//  }

//  emit done(i, 0/*globalTimer.elapsed() / 1000.*/);
//}

// =============================================================================
const uint FastForwardDialog::smoothDurationBufferSize = 100;

FastForwardDialog::FastForwardDialog (QWidget *parent, Simulation &s)
  : QDialog(parent), simulation(s), progressSpeed(smoothDurationBufferSize) {

  setWindowTitle("Go to");

  QVBoxLayout *vLayout = new QVBoxLayout;
  QFormLayout *fLayout = new QFormLayout;

  text = new QLineEdit;
  spinner = new QSpinBox;

  progress = new QProgressBar;
  progress->hide();

  ok = new QPushButton("Go");
  cancel = new QPushButton ("Cancel");

  QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
  buttonBox->addButton(ok, QDialogButtonBox::ActionRole);
  buttonBox->addButton(cancel, QDialogButtonBox::ActionRole);

  setLayout(vLayout);
  vLayout->addLayout(fLayout);
    fLayout->addRow("Time", text);
    fLayout->addRow("Tick", spinner);
  vLayout->addWidget(progress);
  vLayout->addWidget(buttonBox);
  vLayout->setSizeConstraint(QLayout::SetFixedSize);

  text->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  text->setReadOnly(true);
  QString baseText = QString::fromStdString(simulation.prettyTime());
  text->setText(baseText);
  baseText += "  ";
  connect(spinner, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          [this] (int i) {
    text->setText(QString::fromStdString(simulation.prettyTime(i)));
  });

  spinner->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  spinner->setMinimum(simulation.currentStep());
  spinner->setMaximum(simulation.maxStep());

  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(ok, &QPushButton::clicked, this, &FastForwardDialog::process);

  spinner->setFocus();
}

void FastForwardDialog::process(void) {
  spinner->setEnabled(false);
  ok->setEnabled(false);
  progress->setMinimum(spinner->minimum());
  progress->setMaximum(spinner->value()-1);
  progress->setValue(progress->minimum());
  progress->show();

//  QThread *workerThread = new QThread;

//  using Worker = FastForwardDialogWorker;
//  Worker *worker = new Worker(simulation, progress->minimum(), progress->maximum());
//  worker->moveToThread(workerThread);

//  connect(workerThread, &QThread::started, worker, &Worker::doWork);

//  connect(worker, &Worker::done, workerThread, &QThread::quit);
//  connect(worker, &Worker::done, worker, &QObject::deleteLater);
//  connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

//  connect(cancel, &QPushButton::clicked, worker, &Worker::abort);

//  connect(worker, &Worker::stepDone, this, &FastForwardDialog::progressUpdate);

//  connect(worker, &Worker::done, this, &FastForwardDialog::processComplete);
//  connect(worker, &Worker::done, this, &QDialog::accept);

//  workerThread->start();

  aborted = false;
  start = progress->minimum();
  stop = progress->maximum();

  connect(cancel, &QPushButton::clicked, [this] { aborted = true; });

  QTime globalTimer;
  globalTimer.start();
  progressSpeed.restart();

  uint i;
  for (i=start; i<stop && !aborted && !simulation.finished(); i++) {
    simulation.step();
    QCoreApplication::processEvents();

    int eta = (stop - i) * progressSpeed.smoothDuration() / 1000;
    progressUpdate(i, eta);
  }

  processComplete(i, globalTimer.elapsed() / 1000.);

  if (aborted)
        reject();
  else  accept();
}

QString FastForwardDialog::prettySeconds(int d) {
  QString s;

  if (int hours = d / 3600) s += QString("%1h ").arg(hours), d %= 3600;
  if (int minutes = d / 60) s += QString("%1min ").arg(minutes), d %= 60;
  s += QString("%1s").arg(d);

  return s;
}

void FastForwardDialog::progressUpdate(uint step, int eta) {
  progress->setValue(step);
  progress->setFormat(QString( "%p% (ETA: %1)" ).arg(prettySeconds(eta)));
}

void FastForwardDialog::processComplete (uint step, double totalDuration) {
  uint steps = step - progress->minimum();

  std::cout
      << "fast forward of " << steps
      << " ticks took " << qPrintable(prettySeconds(totalDuration))
      << " (" << steps / totalDuration << " sps)" << std::endl;
}

} // end of namespace gui
