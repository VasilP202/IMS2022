#include "simlib.h"
#include <stdio.h>

Store ramps("Rampy", 3); //TODO arg
Queue rampQueue("Cekani na rampu");

Store pracA("Pracovnici PROFI", 3);
Store pracB("Pracovnici OBYCEJNI", 6);

int je_plno = 0;

int PracA_je_plno = 0;
int PracB_je_plno = 0;

int celkem = 0; 

int obsluha = 0;

// TODO Proces pracovnik- maji svoji klasickou praci
// Time + 4h: pauza -> if zrovna vykladka/nakladka skonci a teprve pak pujde na pauzu
// Jinak rovnou jde na pauzu, kontrola zbozi
#define KOLIK 6
Facility PracA[KOLIK];

class TruckService : public Process {
public:
   /*  TruckService(Truck *truck) : Process() {
        //Print("TruckService %d\n", truck);
        truck = truck;
    } */
    void Behavior() {
        //Najdi volne pracovniky
        finished = 0;
        if (pracA.Free() >= 2 && pracB.Free() >= 3) printf("Mozeeee\n");
        else printf(" Nemozeee\n");
        
        Enter(pracA, 1);
        Enter(pracB, 2);
        
        Wait(Uniform(20, 40)); // Obsluha

        Leave(pracA, 1);
        Leave(pracB, 2);

        finished = 1;
        //Wait(Uniform(8, 10));

    };
    //Truck *truck;
    int finished;

};

class Truck : public Process {
public:
    void akce() {
        ready = 1;
        Enter(ramps, 1);
        //(new TruckService)->Activate();
        TruckService *service = new TruckService;
        service->Activate();

        while(1) {
            printf("%d ", service->finished);
            if (service->finished == 1) break;
        }
        //Print("%d\t", this);

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
        akce();
    
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
    Init(0,10000);
    
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
}
