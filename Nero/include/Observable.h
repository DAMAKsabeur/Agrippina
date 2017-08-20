#ifndef Observable_H
#define Observable_H
#include <iostream>
#include <list>
#include <iterator>
#include <algorithm>
#include "Observateur.h"
typedef int Info;
class Observateur;
class Observable
{
    std::list<Observateur*> m_list;

   typedef std::list<Observateur*>::iterator iterator; 
   typedef std::list<Observateur*>::const_iterator const_iterator;

 public:
    void AddObs( Observateur* obs);
    void DelObs(Observateur* obs);
 
        virtual Info Statut(void) const =0;
    virtual ~Observable();
 //protected:
    void Notify(void);

};
#endif
