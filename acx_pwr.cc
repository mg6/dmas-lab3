#include <omnetpp.h>
#include "queue.h"
#include <string>
#include "notification_m.h"
#include "packet_m.h"
#include "global_stats_listener.h"

using namespace omnetpp;

class Acx_pwr : public cSimpleModule {
private:
    bool singleQueue;
    // For single queue operation
    Queue pwr;
    // For multi queue operation
    Queue pcss;
    simtime_t getInterfaceDelay(int destination);
    void sendPacket(Packet *data);

    unsigned long numPacketsDropped;
    unsigned long numPacketsReceived;

    GlobalStatsListener* globalStats;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;
};

Define_Module(Acx_pwr);

simtime_t Acx_pwr::getInterfaceDelay(int destination){
    simtime_t txFT;
    if(destination == 2){
        cChannel *txChn = gate("pwr")->getTransmissionChannel();
        txFT = txChn->getTransmissionFinishTime();
    }else{
        cChannel *txChn = gate("pcss$o")->getTransmissionChannel();
        txFT = txChn->getTransmissionFinishTime();
    }
    return txFT;
}

void Acx_pwr::sendPacket(Packet *data){
    if(data->getDestinationAddress()==2){
        send(data,"pwr");
    }else{
        send(data,"pcss$o");
    }
}

void Acx_pwr::initialize(){
    singleQueue = par("singleQueue");

    numPacketsDropped = 0;
    numPacketsReceived = 0;

    cModule * mod = getModuleByPath("total_stats");
    if (mod) {
        globalStats = dynamic_cast<GlobalStatsListener*>(mod);
    } else {
        error("No total_stats module.");
    }
}

void Acx_pwr::handleMessage(cMessage *msg){
    std::string type = typeid(*msg).name(); // RTTI, *might* become troublesome due to implementation differences.
    std::size_t found;
    found = type.find("Notification");
    if(singleQueue){
        if(found != std::string::npos){
            Notification *notif = check_and_cast<Notification*>(msg);
//            check_and_cast<Notification*>(msg); // Casting just to check if proper notification was received.
            simtime_t txEnd = getInterfaceDelay(pwr.getFirstDestination());
            if(txEnd <= simTime()){
                sendPacket(pwr.dequeue());
                delete(notif);
                if(pwr.size()>0){
                    simtime_t txEnd = getInterfaceDelay(pwr.getFirstDestination());
                    Notification *notif = new Notification();
                    scheduleAt(txEnd>simTime()?txEnd:simTime(), notif);
                }
            }else{
                //Should NOT happen
                EV<<"Unexpected load in outgoing channel - delaying";
//                Notification *notif = new Notification();
                scheduleAt(txEnd, notif);
            }
        }else{
            ++numPacketsReceived;
            Packet *pack = check_and_cast<Packet*>(msg);
            bool test = pwr.enqueue(pack);
            if(test){
                if(pwr.size() == 1){
                    simtime_t txEnd = getInterfaceDelay(pwr.getFirstDestination());
                    Notification *notif = new Notification();
                    scheduleAt(txEnd>simTime()?txEnd:simTime(), notif);
                }
            }else{
                delete(pack);
                EV<<"Packet dropped!";
                ++numPacketsDropped;
                ++globalStats->getNumTotalDropped();
            }
        }
    }else{
        if(found != std::string::npos){
            Notification *notif = check_and_cast<Notification*>(msg);
            if(notif->getQueue() == 0){
                simtime_t txEnd = getInterfaceDelay(pwr.getFirstDestination());
                if(txEnd <= simTime()){
                    sendPacket(pwr.dequeue());
                    delete(notif);
                    if(pwr.size()>0){
                        simtime_t txEnd = getInterfaceDelay(pwr.getFirstDestination());
                        Notification *notif = new Notification();
                        notif->setQueue(0);
                        scheduleAt(txEnd>simTime()?txEnd:simTime(), notif);
                    }
                }else{
                    //Should NOT happen
                    EV<<"Unexpected load in outgoing channel - delaying";
//                    Notification *notif = new Notification();
//                    notif->setQueue(0);
                    scheduleAt(txEnd, notif);
                }
            }else{
                simtime_t txEnd = getInterfaceDelay(pcss.getFirstDestination());
                if(txEnd <= simTime()){
                    sendPacket(pcss.dequeue());
                    delete(notif);
                    if(pcss.size()>0){
                        simtime_t txEnd = getInterfaceDelay(pcss.getFirstDestination());
                        Notification *notif = new Notification();
                        notif->setQueue(1);
                        scheduleAt(txEnd>simTime()?txEnd:simTime(), notif);
                    }
                }else{
                    //Should NOT happen
                    EV<<"Unexpected load in outgoing channel - delaying";
//                    Notification *notif = new Notification();
//                    notif->setQueue(1);
                    scheduleAt(txEnd, notif);
                }
            }
        }else{
            ++numPacketsReceived;
            Packet *input = check_and_cast<Packet*>(msg);
            bool test;
            if(input->getDestinationAddress() == 2){
                test=pwr.enqueue(input);
                if(test){
                    if(pwr.size() == 1){
                        simtime_t txEnd = getInterfaceDelay(pwr.getFirstDestination());
                        Notification *notif = new Notification();
                        notif->setQueue(0);
                        scheduleAt(txEnd>simTime()?txEnd:simTime(), notif);
                    }
                }else{
                    delete(input);
                    EV<<"Packet dropped!";
                    ++numPacketsDropped;
                    ++globalStats->getNumTotalDropped();
                }
            }else{
                if(input->getDestinationAddress() == 3){
                    test=pcss.enqueue(input);
                    if(test){
                        if(pcss.size() == 1){
                            simtime_t txEnd = getInterfaceDelay(pcss.getFirstDestination());
                            Notification *notif = new Notification();
                            notif->setQueue(1);
                            scheduleAt(txEnd>simTime()?txEnd:simTime(), notif);
                        }
                    }else{
                        delete(input);
                        EV<<"Packet dropped!";
                        ++numPacketsDropped;
                        ++globalStats->getNumTotalDropped();
                    }
                }else{
                    delete(input);
                    EV<<"This packet should not appear here!";
                    ++numPacketsDropped;
                    ++globalStats->getNumTotalDropped();
                }
            }
        }
    }
}

void Acx_pwr::refreshDisplay() const {
    char buf[40];
    sprintf(buf, "rcvd: %lu drpd: %lu", numPacketsReceived, numPacketsDropped);
    getDisplayString().setTagArg("t", 0, buf);
}
