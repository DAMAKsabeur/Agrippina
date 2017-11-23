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

class NeroIntelCE4x00BlockingQueue {

public:
NeroIntelCE4x00BlockingQueue();
NeroIntelCE4x00BlockingQueue(size_t Queue_size);
~NeroIntelCE4x00BlockingQueue();
ES_t pop() ;
ES_t front();
Nero_error_t push(ES_t value) ;

bool empty() ;
private:
	pthread_mutex_t mutex_;
    std::queue<ES_t> queue_;
    size_t m_Queue_size;
};
#endif /* BLOCKINGQUEUE_H_ */
