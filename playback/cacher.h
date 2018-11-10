#ifndef CACHER_H
#define CACHER_H

#include <QThread>
#include <QVector>

struct Clip;

class Cacher : public QThread
{
//	Q_OBJECT
public:
	Cacher(Clip* c);
    void run();

	bool caching;

	// must be set before caching
	long playhead;
	bool reset;
    bool scrubbing;
	QVector<Clip*> nests;

private:
	Clip* clip;
};

void open_clip_worker(Clip* clip);
void cache_clip_worker(Clip* clip, long playhead, bool reset, bool scrubbing, QVector<Clip *> nests);
void close_clip_worker(Clip* clip);

#endif // CACHER_H
