#include "simlib.h"

#include <stdio.h>

#define TRUCK_UNLOADING 0
#define TRUCK_LOADING 1

Histogram dobaVSystemu("Celkova doba v systemu", 0, 40, 20);

Store ramps("Rampy", 3);
Queue rampQueue("Cekani na rampu");
Queue serviceQueue("Cekani na obsluhu");
Queue controlQueue("Kontrola zbozi");

Queue q("Fronta");

Store pracA("Pracovnici PROFI", 2);
Store pracB("Pracovnici OBYCEJNI", 2);

int je_plno = 0;

int PracA_full = 0;
int PracB_full = 0;

int workers_unavailable = 0;
int celkem = 0;
int obsluha = 0;

int loading_count = 0;
int unloading_count = 0;


class ControlService: public Process {
    public: void Behavior() {
    }
};

class Truck: public Process {
    unsigned int serviceType;
    unsigned int loadAfterUnload = 1;
    public: Truck(unsigned int type): Process() {
        //printf("contr %d\n", type);
        serviceType = type;
    }

    void Service() {
        if (serviceType == TRUCK_UNLOADING && Random() <= 0.2)
            loadAfterUnload = 1;

        Enter(pracA, 1);
        Enter(pracB, 2);

        //Wait(Uniform(20, 50)); // Obsluha
        Wait(Exponential(100)); // Obsluha
        
        if(loadAfterUnload) 
            Wait(Uniform(20, 50));

        Leave(pracA, 1);
        Leave(pracB, 2);
    }

    void Action() {
        Enter(ramps, 1);
        //printf("%d, %d\n", pracA.Used(), pracB.Used());
        if(pracA.Full()) PracA_full++;
        if(pracB.Full()) PracB_full++;
        if (!(pracA.Free() >= 1 && pracB.Free() >= 2)) {
            // Stats
            workers_unavailable++;
        }
        while (!(pracA.Free() >= 1 && pracB.Free() >= 2)) {
            serviceQueue.Insert(this);
            Passivate();
            break;
        }

        Service();

        Leave(ramps, 1);

        if (serviceType == TRUCK_UNLOADING) {
            unloading_count ++;
            (new ControlService)->Activate();
        } else if(serviceType == TRUCK_LOADING) loading_count++;

        if (serviceQueue.Length() > 0) {
            //printf("Nekdo cekaaaa\n");
            (serviceQueue.GetFirst()) -> Activate();
        }
        if (rampQueue.Length() > 0) {
            //printf("Nekdo cekaaaa\n");
            (rampQueue.GetFirst()) -> Activate();
        }
    }
    void Behavior() {
        //printf("Service type %d\n", serviceType);
        double tvstup = Time;

        celkem++;

        if (ramps.Full()) {
            je_plno++;
        }

        while (ramps.Full()) {
            rampQueue.Insert(this);
            Passivate();
            break;
        }
        Action();
        dobaVSystemu(Time - tvstup);
    }

};

class TruckGenerator: public Event {
    void Behavior() {
        /* (new Truck)->Activate();
        (new TruckService)->Activate(); */

        double r = Random();
        if (r <= 0.5) {
            (new Truck(TRUCK_LOADING)) -> Activate();
        }
        else{
            (new Truck(TRUCK_UNLOADING)) -> Activate();
        }
        /* TruckService *service = new TruckService(truck);
        service->Activate(); */

        float next = Time + Exponential(30);
        Activate(next); // //TODO arg - Kazdou hodinu prijizdi novy nakladak
    }
};

int main() {
    Init(0, 100000);

    (new TruckGenerator) -> Activate();

    Run();
    ramps.Output();
    pracA.Output();
    pracB.Output();
    //dobaVSystemu.Output();

    Print("Celkem aut prijelo: %d\n", celkem);
    Print("Je plno: %d\n", je_plno);
    Print("PracA Je plno: %d\n", PracA_full);
    Print("PracB Je plno: %d\n", PracB_full);
    Print("Pravdepodobnost, ze bude plno: %f\n", (float) je_plno / celkem);
    Print("Pracovnici nedostupni: %d\n", workers_unavailable);
    Print("loading_count %d, unloading_count %d\n", loading_count, unloading_count);
}