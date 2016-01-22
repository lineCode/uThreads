/*
 * IOHandler.cpp
 *
 *  Created on: Aug 17, 2015
 *      Author: Saman Barghi
 */
#include "IOHandler.h"
#include "Network.h"
#include "runtime/kThread.h"
#include <unistd.h>
#include <sys/types.h>
#include <iostream>

IOHandler::IOHandler(Cluster& cluster): bulkCounter(0), localCluster(&cluster), ioKT(cluster, &IOHandler::pollerFunc, (ptr_t)this){}

IOHandler* IOHandler::create(Cluster& cluster){
    IOHandler* ioh = nullptr;
#if defined (__linux__)
    ioh = new EpollIOHandler(cluster);
#else
#error unsupported system: only __linux__ supported at this moment
#endif
    return ioh;
}

void IOHandler::open(PollData &pd){
    assert(pd.fd > 0);

    int res = _Open(pd.fd, &pd);
    //TODO: handle epoll errors
}
void IOHandler::wait(PollData& pd, int flag){
    assert(pd.fd > 0);
    if(flag & UT_IOREAD) block(pd, true);
    if(flag & UT_IOWRITE) block(pd, false);

}
void IOHandler::block(PollData &pd, bool isRead){

    std::unique_lock<std::mutex> pdlock(pd.mtx);
    uThread** utp = isRead ? &pd.rut : &pd.wut;
    //TODO:check other states

    if(slowpath(*utp == POLL_READY))    //This is unlikely since we just did a nonblocking read
    {
        *utp = nullptr;  //consume the notification and return;
        return;
    }else if(fastpath(*utp == nullptr))
            //set the state to Waiting
            *utp = POLL_WAIT;
    else
        std::cerr << "Exception on open rut" << std::endl;

    pdlock.unlock();

    uThread* old = kThread::currentKT->currentUT;
    //TODO:decrease the capture list to avoid hitting the heap
    auto lambda([&pd, &utp, &old](){
        if(pd.closing) return;
        std::lock_guard<std::mutex> pdlock(pd.mtx);
          if(*utp == POLL_READY){
                *utp = nullptr;         //consume the notification and resume
                old->resume();
          }else if(*utp == POLL_WAIT){
                *utp = old;
          }else
                std::cerr << "Exception on rut"<< std::endl;
    });
    std::function<void()> f(std::cref(lambda));
    kThread::currentKT->currentUT->suspend(f); //ask for immediate suspension so the possible closing/notifications do not get lost
    //when epoll returns this ut will be back on readyQueue and pick up from here
}
int IOHandler::close(PollData &pd){

    std::lock_guard<std::mutex> pdlock(pd.mtx);
    assert(pd.rut < POLL_WAIT && pd.wut < POLL_WAIT);
    //remove from underlying poll structure
    int res = _Close(pd.fd);

    pd.reset();
    //TODO: handle epoll errors
    return res;
}

void IOHandler::poll(int timeout, int flag){
    _Poll(timeout);
}

void IOHandler::reset(PollData& pd){
    pd.reset();
}

void IOHandler::unblock(PollData &pd, bool isRead){
    //if closing is set no need to process
    if(slowpath(pd.closing.load()))  return;

    std::lock_guard<std::mutex> pdlock(pd.mtx);
    uThread** ut = isRead ? &pd.rut : &pd.wut;
    uThread* old = *ut;

    if(slowpath(old == POLL_READY))
        return;
    if(old == nullptr || old == POLL_WAIT)
       *ut = POLL_READY;
    else if(old > POLL_WAIT){
        *ut = nullptr;
        old->resume();
    }

}

void IOHandler::unblockBulk(PollData &pd, int flag, bool isLast){

    //if it's closing no need to process
    if(fastpath(!pd.closing.load())){
        std::lock_guard<std::mutex> pdlock(pd.mtx);
        uThread **rut = &pd.rut, **wut = &pd.wut;
        uThread *rold = *rut, *wold = *wut;

        if(flag & UT_IOREAD){
            if(rold == POLL_READY);
            else if(rold == nullptr || rold == POLL_WAIT)
               *rut = POLL_READY;
            else if(rold > POLL_WAIT){
                *rut = nullptr;
                rold->state = READY;
                bulkQueue.push_back(*rold);
                bulkCounter++;
            }
        }
        if(flag & UT_IOWRITE){
            if(wold == POLL_READY);
            else if(wold == nullptr || wold == POLL_WAIT)
               *wut = POLL_READY;
            else if(wold > POLL_WAIT){
                *wut = nullptr;
                wold->state = READY;
                bulkQueue.push_back(*wold);
                bulkCounter++;
            }
        }
    }
    //if this is the last item return by the poller
    //Bulk push everything to the related cluster ready Queue
    if(slowpath(isLast && bulkCounter >0)){
        localCluster->scheduleMany(bulkQueue, bulkCounter);
        bulkCounter =0;
    }
}

void IOHandler::PollReady(PollData &pd, int flag){
    if(flag & UT_IOREAD) unblock(pd, true);
    if(flag & UT_IOWRITE) unblock(pd, false);
}
void IOHandler::PollReadyBulk(PollData &pd, int flag, bool isLast){
        unblockBulk(pd, flag, isLast);
}

void IOHandler::pollerFunc(void* ioh){
    //wait for IO
    //TODO: fix this
    usleep(10000);
    IOHandler* cioh = (IOHandler*)ioh;
    while(true){
//       std::cout << "Waiting ... " << std::endl;
       cioh->poll(-1, 0);
   }
}

