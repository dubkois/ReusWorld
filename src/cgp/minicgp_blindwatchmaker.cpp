#include <QApplication>
#include <QMainWindow>
#include <QGridLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include "kgd/external/cxxopts.hpp"

#include "kgd/utils/indentingostream.h"

#include "minicgp.h"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(watchmaker, VideoInputs, X, Y, T, R, G, B)
DEFINE_NAMESPACE_PRETTY_ENUMERATION(watchmaker, RGBOutputs, R_, G_, B_)

using CGP = cgp::CGP<watchmaker::VideoInputs, 10, watchmaker::RGBOutputs>;

namespace watchmaker {

using Callback = std::function<void(const CGP&)>;

static constexpr int STEPS = 50;
static constexpr int LOOP_DURATION = 2;  // seconds

struct CGPViewer : public QPushButton {
  static constexpr int SIDE = 50;
  static constexpr int MARGIN = 10;
  static constexpr QSize SIZE = QSize(SIDE + 2 * MARGIN, SIDE + 2 * MARGIN);

  CGP &cgp;
  Callback callback;
  QImage img;

  int step;
  bool up = true;

  CGPViewer(CGP &cgp, Callback c)
    : QPushButton(nullptr), cgp(cgp), callback(c),
      img(SIDE, SIDE, QImage::Format_RGB32) {
    img.fill(Qt::gray);

    setFocusPolicy(Qt::NoFocus);
    setAutoExclusive(true);

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    step = 0;
  }

  void cgpUpdated (void) {
    setToolTip(QString::fromStdString(cgp.toTex()));
    img.fill(Qt::gray);
  }

  void nextFrame (void) {
    QRgb *pixels = reinterpret_cast<QRgb*>(img.scanLine(0));
    CGP::Inputs inputs;
    inputs[T] = 2. * step / (STEPS - 1) - 1;
    for (int i=0; i<SIDE; i++) {
      for (int j=0; j<SIDE; j++) {
        inputs[X] = double(2) * i / (SIDE-1) - 1;
        inputs[Y] = double(2) * j / (SIDE-1) - 1;

        QRgb &pixel = pixels[i*SIDE+j];
        inputs[R] = 2 * qRed(pixel) / 255. - 1;
        inputs[G] = 2 * qGreen(pixel) / 255. - 1;
        inputs[B] = 2 * qBlue(pixel) / 255. - 1;

        CGP::Outputs outputs;
        cgp.evaluate(inputs, outputs);
        pixel = qRgb(255 * (outputs[R_] * .5 + .5),
                     255 * (outputs[G_] * .5 + .5),
                     255 * (outputs[B_] * .5 + .5));
      }
    }

    if (up) {
      step++;
      up = (step < STEPS);
    } else {
      step--;
      up = (step <= 0);
    }

    update();
  }

  QSize sizeHint (void) const {
    return SIZE;
  }

  void paintEvent(QPaintEvent *event) {
    QPushButton::paintEvent(event);
    QPainter painter (this);
    painter.drawImage(rect().adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN), img);
  }

  void mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
      callback(cgp);
    else if (event->button() == Qt::RightButton) {
      static int i = 0;
      std::ostringstream oss;
      oss << "cgp_bw_test_" << i++ << ".pdf";
      stdfs::path path = oss.str();
      cgp.toDot(path, CGP::FULL | CGP::NO_ARITY);

      oss.str("");
      oss << "xdg-open " << path;
      std::string cmd = oss.str();
      system(cmd.c_str());
    }
  }
};

} // end of namespace watchmaker

int main (int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  cxxopts::Options options("MiniCGP-blindwatchmaker",
                           "Exhibit MiniCGP capabilities through a blind"
                           " watchmaker algorithm");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  config::CGP::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::CGP::printConfig("");

  rng::FastDice dice;
  auto cgps = utils::make_array<CGP, 9>([&dice] (auto) {
    return CGP::null(dice);
  });

  std::cout << "Generated with seed " << dice.getSeed() << std::endl;

  QApplication app (argc, argv);
  QMainWindow w;

  QWidget *widget = new QWidget;
  QGridLayout *layout = new QGridLayout;

  w.setCentralWidget(widget);
  widget->setLayout(layout);

  std::array<watchmaker::CGPViewer*, 9> viewers;
  auto updateCGPs = [&dice, &viewers, &cgps]
                    (const CGP &newParent) {
    cgps[4] = newParent;

    for (uint i=0; i<3; i++) {
      for (uint j=0; j<3; j++) {
        uint ix = i*3+j;

        if (ix != 4) {
          CGP child = newParent;
//          std::cerr << "Mutating IX=" << ix << ", i=" << i << ", j=" << j
//                    << ":\n";
//          utils::IndentingOStreambuf indent (std::cerr);

          child.mutate(dice);
          cgps[ix] = child;

//          std::cerr << std::endl;
        }

        viewers[ix]->cgpUpdated();
      }
    }
  };

  for (int i=0; i<3; i++) {
    for (int j=0; j<3; j++) {
      uint ix = i*3+j;
      auto viewer = new watchmaker::CGPViewer(cgps[ix], updateCGPs);
      viewers[ix] = viewer;
      layout->addWidget(viewer, i, j);
    }
  }

  updateCGPs(cgps[4]);

  w.show();

  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&viewers] {
    for (watchmaker::CGPViewer *v: viewers) v->nextFrame();
  });
  timer.start(watchmaker::LOOP_DURATION * 1000.f / watchmaker::STEPS);

  return app.exec();
}
