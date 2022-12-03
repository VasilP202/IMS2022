#include "simlib.h"
#include <stdio.h>

Store ramps("Rampy", 3); //TODO arg
Queue rampQueue("Cekani na rampu");
Queue serviceQueue("Cekani na obsluhu");

Store pracA("Pracovnici PROFI", 3);
Store pracB("Pracovnici OBYCEJNI", 6);

int je_plno = 0;

int PracA_je_plno = 0;
int PracB_je_plno = 0;

int workers_unavailable = 0;
int celkem = 0;
int obsluha = 0;
// TODO Proces pracovnik- maji svoji klasickou praci
// Time + 4h: pauza -> if zrovna vykladka/nakladka skonci a teprve pak pujde na pauzu
// Jinak rovnou jde na pauzu, kontrola zbozi

class TruckService : public Process {
public:
   /*  TruckService(Truck *truck) : Process() {
        //Print("TruckService %d\n", truck);
        truck = truck;
    } */
    void Behavior() {
        Enter(pracA, 1);
        Enter(pracB, 3);
        
        Wait(Exponential(40)); // Obsluha

        Leave(pracA, 1);
        Leave(pracB, 3);

    };
};

class Truck : public Process {
public:

    void Action() {
        /* */
        Enter(ramps, 1);
        
        printf("%d, %d\n", pracA.Used(), pracB.Used());
        
        if (!(pracA.Free() >= 1 && pracB.Free() >= 3)) {
        //    printf("%d, %d\n", pracA.Free(), pracB.Free());
            // Stats
            workers_unavailable++; 
        }
        while(!(pracA.Free() >= 1 && pracB.Free() >= 3)) {
            printf("QUEUE");
            serviceQueue.Insert(this);
            Passivate();

            break;
        }
        (new TruckService())->Activate();

        if (serviceQueue.Length() > 0) {
            //printf("Nekdo cekaaaa\n");
			(serviceQueue.GetFirst())->Activate();
		}

        Leave(ramps, 1);
        if (rampQueue.Length() > 0) {
            //printf("Nekdo cekaaaa\n");
			(rampQueue.GetFirst())->Activate();
		}
    }
    void Behavior() {
        celkem++;

        if(ramps.Full()) {
            je_plno++;
        }

        while(ramps.Full()) {
            rampQueue.Insert(this);
            Passivate();
            break;
        }
        Action();
    
    }
    int ready = 0;
};


class TruckGenerator : public Event {
    void Behavior() {
        /* (new Truck)->Activate();
        (new TruckService)->Activate(); */
        Truck * truck = new Truck;
        truck->Activate();
        
        /* TruckService *service = new TruckService(truck);
        service->Activate(); */

        float next = Time + Exponential(60);
        Activate(next); // //TODO arg - Kazdou hodinu prijizdi novy nakladak
    }
};


int main() {
    Init(0, 10000);
    
    (new TruckGenerator)->Activate();

    Run();
    ramps.Output();
    pracA.Output();
    pracB.Output();

    Print("Celkem aut prijelo: %d\n", celkem);
    Print("Je plno: %d\n", je_plno);
    Print("PracA Je plno: %d\n", PracA_je_plno);
    Print("PracB Je plno: %d\n", PracB_je_plno);
	Print("Pravdepodobnost, ze bude plno: %f\n", (float)je_plno/celkem);
    Print("Pracovnici nedostupni: %d\n", workers_unavailable);
}
