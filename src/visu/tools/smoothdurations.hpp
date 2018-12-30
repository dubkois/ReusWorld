#ifndef _SMOOTH_DURATIONS_HPP_
#define _SMOOTH_DURATIONS_HPP_

#include <QElapsedTimer>
#include <QQueue>

class SmoothDurationManager {
public:
  SmoothDurationManager (uint frames, int debugLevel=0) : frames(frames), debugLevel(debugLevel) {
    timer.start();
  }

  /*! \brief in ms
     */
  double smoothDuration (void) {
    queue.enqueue(timer.elapsed());
    if (queue.size() > frames) queue.dequeue();

    double sum = 0;
    for (auto d: queue)    sum += d;

    restart();
    return sum/queue.size();
  }

  void restart (void) {
    timer.restart();
  }

private:
  /// Counts time between to events
  QElapsedTimer timer;

  /// Durations buffer
  QQueue<double> queue;

  /// Size of the buffer
  int frames;

  /// Debug level
  const int debugLevel;
};

#endif // _SMOOTH_DURATIONS_HPP_
