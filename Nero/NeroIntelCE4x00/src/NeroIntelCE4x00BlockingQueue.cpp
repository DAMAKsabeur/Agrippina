/*
 * NeroIntelCE4x00BlockingQueue.cpp
 *
 *  Created on: Nov 19, 2017
 *      Author: sabeur
 */

#include "NeroIntelCE4x00BlockingQueue.h"
NeroIntelCE4x00BlockingQueue::NeroIntelCE4x00BlockingQueue()
{
	pthread_mutex_init(&mutex_,NULL);
	m_Queue_size = 2000000;
}
NeroIntelCE4x00BlockingQueue::NeroIntelCE4x00BlockingQueue(size_t Queue_size)
{
	pthread_mutex_init(&mutex_,NULL);
	m_Queue_size = Queue_size;
}
NeroIntelCE4x00BlockingQueue::~NeroIntelCE4x00BlockingQueue()
{
	pthread_mutex_destroy(&mutex_);
}
ES_t NeroIntelCE4x00BlockingQueue::pop() {
	    ES_t value;
	    pthread_mutex_lock(&mutex_);
        value = queue_.front();
        queue_.pop();
        pthread_mutex_unlock(&mutex_);
        return value;
    }
ES_t NeroIntelCE4x00BlockingQueue::front() {
	    ES_t value;
	    pthread_mutex_lock(&mutex_);
        value = queue_.front();
        pthread_mutex_unlock(&mutex_);
        return value;
    }
Nero_error_t NeroIntelCE4x00BlockingQueue::push(ES_t value) {
        pthread_mutex_lock(&mutex_);
        queue_.push(value);
        pthread_mutex_unlock(&mutex_);
    }

bool NeroIntelCE4x00BlockingQueue::empty() {
        pthread_mutex_lock(&mutex_);
        bool check = queue_.empty();
        pthread_mutex_unlock(&mutex_);
        return check;
    }

