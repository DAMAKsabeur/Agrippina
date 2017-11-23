/*
 * Quee.h
 *
 *  Created on: Nov 19, 2017
 *      Author: sabeur
 */

#ifndef BLOCKINGQUEUE_H_
#define BLOCKINGQUEUE_H_
#include <queue>
#include <string>
#include <pthread.h>
#include "NeroConstants.h"
#include "NeroIntelCE4x00Constants.h"

class NeroBlockingQueue {

public:
	NeroBlockingQueue();
	NeroBlockingQueue(size_t Queue_size);
~NeroBlockingQueue();
ES_t pop() ;
ES_t front();
Nero_error_t push(ES_t value) ;

bool empty() ;
bool ready() ;
bool full() ;
private:
	pthread_mutex_t mutex_;
    std::queue<ES_t> queue_;
    size_t m_Queue_size;
    size_t level;
};
#endif /* BLOCKINGQUEUE_H_ */
