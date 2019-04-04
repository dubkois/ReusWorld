#ifndef GRAPHICSIMULATION_H
#define GRAPHICSIMULATION_H

#include <QObject>

#include "kgd/apt/visu/phylogenyviewer.h"

#include "../simu/simulation.h"

namespace visu {
struct Controller;

class GraphicSimulation : public QObject, public simu::Simulation {
  Q_OBJECT
  Controller *_controller;

  using PViewer = gui::PhylogenyViewer<
    simu::Simulation::PTree::Genome,
    simu::Simulation::PTree::UserData>;
  PViewer _pviewer;

public:
  GraphicSimulation (void);

  void setController (Controller *_controller);

  bool init (const EGenome &env, const PGenome &plant) override;

  void graphicalStep (uint speed);

  void saveAs (void) const;

  void togglePViewer (void) {
    _pviewer.setVisible(!_pviewer.isVisible());
  }

  void savePhylogeny (void) const;

  static void load (const std::string &file, GraphicSimulation &s,
                    const std::string &constraints);

private:
  bool addPlant (const PGenome &p, float x, float biomass) override;
  void delPlant (float x, simu::Plant::Seeds &seeds) override;

  void updatePlantAltitude(simu::Plant &p, float h) override;

  void doScreenshot (void) const;

signals:
  void initialized (bool ok);
  void completed (void);
};

} // end of namespace visu

#endif // GRAPHICSIMULATION_H
