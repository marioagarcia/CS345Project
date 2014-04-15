#ifndef QUEUE_H
#define QUEUE_H

typedef struct {
	int tid;
	int priority;
} task;

typedef struct {
	task queue[128];
	int count;
}PQueue;

task enQ(PQueue* pq, int tid, int p);
task deQ(PQueue* pq, int tid);
task deQtop(PQueue* pq);
PQueue * initQueue();

#endif // !QUEUE_H

