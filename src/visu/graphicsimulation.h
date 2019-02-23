#ifndef GRAPHICSIMULATION_H
#define GRAPHICSIMULATION_H

#include <QObject>

#include "../simu/simulation.h"

namespace visu {
struct Controller;

class GraphicSimulation : public QObject, public simu::Simulation {
  Q_OBJECT
  Controller *_controller;
public:
  void setController (Controller *_controller);

  bool init (const EGenome &env, const PGenome &plant) override;

  void graphicalStep (void);

  void saveAs (void) const;

  void savePhylogeny (void) const;

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
