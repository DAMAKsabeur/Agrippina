#ifndef Observateur_H
#define Observateur_H
#include <iostream>
#include <list>
#include <iterator>
#include <algorithm>
#include "Observable.h"
typedef int Info;
class Observable;
class Observateur
{
 protected:
    std::list<Observable*> m_list;
   typedef std::list<Observable*>::iterator iterator; 
   typedef std::list<Observable*>::const_iterator const_iterator;
   virtual ~Observateur() = 0;
 public:
    virtual void Update(const Observable* observable) const ;

    void AddObs(Observable* obs);
    void DelObs(Observable* obs);
};
#endif
