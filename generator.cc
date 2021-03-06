#include <omnetpp.h>
#include "packet_m.h"
#include "global_stats_listener.h"

using namespace omnetpp;

class Generator : public cSimpleModule {
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void refreshDisplay() const override;

    int packetLength;
    int destination;
    simtime_t secondsPerPacket;

    unsigned long numPacketsDropped;
    unsigned long numPacketsEmitted;

    GlobalStatsListener* globalStats;
};

Define_Module(Generator);

void Generator::initialize(){
    packetLength = par("packetLength");
    long sendRate = par("sendRate");
    secondsPerPacket = packetLength * (8.f / (float)sendRate);
    destination = 0;
    numPacketsDropped = 0;
    numPacketsEmitted = 0;
    cMessage *selfMessage = new cMessage();
    scheduleAt(simTime(), selfMessage);

    cModule * mod = getModuleByPath("total_stats");
    if (mod) {
        globalStats = dynamic_cast<GlobalStatsListener*>(mod);
    } else {
        error("No global_stats module.");
    }
}

void Generator::handleMessage(cMessage *msg){
    Packet *pkt = new Packet();
    pkt->setDestinationAddress(destination);
    pkt->setByteLength((int64_t)packetLength);
    char name[] = "Packet             ";
    snprintf(&name[7], sizeof(name)/sizeof(char) - 7, "%lu", numPacketsEmitted);
    pkt->setName(name);

    cChannel *outChannel = gate("out")->getTransmissionChannel();
    simtime_t finishTime = outChannel->getTransmissionFinishTime();
    simtime_t varSimTime = simTime();
    simtime_t timeDiff = secondsPerPacket - ((finishTime<0)?0:finishTime - varSimTime);

    if(finishTime <= varSimTime)
    {
        send(pkt, "out");
        EV << "Packet sent to " << pkt->getDestinationAddress() << " at time " << simTime() << ".\n";
        destination = (++destination)%4;
        numPacketsEmitted++;

        if(timeDiff > secondsPerPacket)
            scheduleAt(varSimTime + secondsPerPacket, msg);
        else
            scheduleAt(varSimTime + timeDiff, msg);
    }
    else
    {
        delete pkt;
        ++numPacketsDropped;
        ++globalStats->getNumTotalDropped();
        scheduleAt(finishTime, msg);
    }
}

void Generator::refreshDisplay() const {
    char buf[40];
    sprintf(buf, "emit: %lu drpd: %lu", numPacketsEmitted, numPacketsDropped);
    getDisplayString().setTagArg("t", 0, buf);
}
