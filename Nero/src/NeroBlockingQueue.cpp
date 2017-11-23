/*
 * NeroBlockingQueue.cpp
 *
 *  Created on: Nov 19, 2017
 *      Author: sabeur
 */

#include "NeroBlockingQueue.h"
NeroBlockingQueue::NeroBlockingQueue()
{
	pthread_mutex_init(&mutex_,NULL);
	m_Queue_size = 2000000;
	level = 0;
}
NeroBlockingQueue::NeroBlockingQueue(size_t Queue_size)
{
	pthread_mutex_init(&mutex_,NULL);
	m_Queue_size = Queue_size;
	level = 0;
}
NeroBlockingQueue::~NeroBlockingQueue()
{
	pthread_mutex_destroy(&mutex_);
	level = 0;
}
ES_t NeroBlockingQueue::pop() {
	    ES_t value;
	    pthread_mutex_lock(&mutex_);
        value = queue_.front();
        queue_.pop();
        level --;
        pthread_mutex_unlock(&mutex_);
        return value;
    }
ES_t NeroBlockingQueue::front() {
	    ES_t value;
	    pthread_mutex_lock(&mutex_);
        value = queue_.front();
        pthread_mutex_unlock(&mutex_);
        return value;
    }
Nero_error_t NeroBlockingQueue::push(ES_t value) {
        pthread_mutex_lock(&mutex_);
        queue_.push(value);
        level ++;
        pthread_mutex_unlock(&mutex_);
    }

bool NeroBlockingQueue::empty() {
        pthread_mutex_lock(&mutex_);
        bool check = queue_.empty();
        pthread_mutex_unlock(&mutex_);
        return check;
    }
bool NeroBlockingQueue::ready() {
        pthread_mutex_lock(&mutex_);
        bool check = (level >= (size_t)(0.1*m_Queue_size));
        pthread_mutex_unlock(&mutex_);
        return check;
    }
bool NeroBlockingQueue::full() {
        pthread_mutex_lock(&mutex_);
        bool check = (level == m_Queue_size);
        pthread_mutex_unlock(&mutex_);
        return check;
    }
