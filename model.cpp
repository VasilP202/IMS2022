#include "simlib.h"

#include <stdio.h>

#include <iostream>

#include <algorithm>

using namespace std;

#define TRUCK_UNLOADING 0
#define TRUCK_LOADING 1

#define TRUCK_ACTIVATE_TIME_MEAN 60
#define A_WORKER_SERVICE_COUNT 1
#define B_WORKER_SERVICE_COUNT 2
#define A_WORKER_STORING_SERVICE_COUNT 2
#define B_WORKER_CONTROL_SERVICE_COUNT 2

Queue rampQueue("Cekani na rampu");
Queue serviceQueue("Cekani na obsluhu");

Histogram truckSystemTime("Celkova doba v systemu", 0, 5, 7);

int je_plno = 0;
int aWorkers_full = 0;
int bWorkers_full = 0;
int workers_unavailable = 0;
int celkem = 0;
int obsluha = 0;
int kontrola = 0;
int storing = 0;

int loading_count = 0;
int unloading_count = 0;

double totalServiceTime = 0;

void showHelpMessage(string name) {
    cerr << "Usage: " << name << " <option(s)>\n" <<
        "Options:\n" <<
        "\t-r / --ramps COUNT\t\t Number of open ramps in the warehouse.\n" <<
        "\t-a / --awork COUNT\t\t Number of professional/qualified workers.\n" <<
        "\t-b / --bwork COUNT\t\t Number of ordinary/unqualified workers.\n\n" <<
        "\t-f / --file FILENAME\t\t Save output to the file FILENAME.\n" <<
        "\t-h / --help\tShow this help message." <<
        endl;
}

bool findOptionString(char * start[], char * end[],
    const string & optionString) {
    return find(start, end, optionString) != end;
}

char * getOptionValue(char * start[], char * end[],
    const string & optionString, const string & optionStringLong) {
    /* Find option string and return option value */
    /* https://stackoverflow.com/questions/865668/parsing-command-line-arguments-in-c */
    char ** itr = find(start, end, optionString);
    if (itr != end && ++itr != end)
        return * itr;
    
    char ** itrLong = find(start, end, optionStringLong);
    if (itrLong != end && ++itrLong != end)
        return * itrLong;
    return 0;
}

class ControlService: public Process {
    public: ControlService(Store * bWorkers): Process() {
        b = bWorkers;
    }
    void Behavior() {
        /* 
            Checking of received goods after unloading the truck.
            Checking is done by ordinary workers.
        */

        //if(bWorkers.Free() < 2) printf("[%d]: B kontrola: Nedostupni: %d\n", kontrola, bWorkers.Used());
        kontrola++;
        Enter( * b, B_WORKER_CONTROL_SERVICE_COUNT);
        //printf("[%d]: Pouziti B: %d\n", kontrola, bWorkers.Used());
        Wait(Uniform(10, 15));

        Leave( * b, B_WORKER_CONTROL_SERVICE_COUNT);
    }
    Store * b;
};

class StoringService: public Process {
    public: StoringService(
        Store * aWorkers
    ): Process() {
        a = aWorkers;
    }
    void Behavior() {
        /*
            Storage of recieved goods after unload.
            Done by professional workers.
        */
        storing++;
        Enter( * a, A_WORKER_STORING_SERVICE_COUNT);

        Wait(Uniform(15, 20));
        Leave( * a, A_WORKER_STORING_SERVICE_COUNT);
    }
    Store * a;
};

class Truck: public Process {
    public: Truck(
        unsigned int type,
        Store * ramps,
        Store * aWorkers,
        Store * bWorkers
    ): Process() {
        serviceType = type;
        r = ramps;
        a = aWorkers;
        b = bWorkers;
    }

    void Service() {
        /* 
            Takes 1 professional and 2 ordinary storemen.
            Simulate truck unloading or loading.
        */
        obsluha++;
        Enter( * a, A_WORKER_SERVICE_COUNT);
        Enter( * b, B_WORKER_SERVICE_COUNT);

        double serviceTime = Time;

        Wait(Uniform(15, 30)); // Unloading/Loading

        Leave( * a, A_WORKER_SERVICE_COUNT);
        Leave( * b, B_WORKER_SERVICE_COUNT);

        totalServiceTime += Time - serviceTime;
    }

    void Action(double enterTime) {
        /*
            Finds free storemen for unloading/loading.
            After the service is done, free the ramp.
        */
        Enter( * r, 1);

        if (a -> Full()) aWorkers_full++;
        if (b -> Full()) bWorkers_full++;
        if (!(a -> Free() >= A_WORKER_SERVICE_COUNT && b -> Free() >= B_WORKER_SERVICE_COUNT)) {
            //printf("[%d] !!! Free: %d, %d\n", celkem, a->Free(), b->Free());
            // Stats
            workers_unavailable++;
        }
        while (!(a -> Free() >= A_WORKER_SERVICE_COUNT && b -> Free() >= B_WORKER_SERVICE_COUNT)) {
            serviceQueue.Insert(this);
            Passivate();

            break;
        }

        Service();

        if (serviceQueue.Length() > 0) {
            //printf("[%d] Aktivuje pro obsluhu...\n", celkem);
            (serviceQueue.GetFirst()) -> Activate();
        }
        truckSystemTime(Time - enterTime);
        Leave( * r, 1);

        if (serviceType == TRUCK_UNLOADING) {
            unloading_count++;
            (new ControlService(b)) -> Activate(); //Activate goods control process
            (new StoringService(a)) -> Activate(); //Activate storing process
        } else if (serviceType == TRUCK_LOADING) loading_count++;

        if (rampQueue.Length() > 0) {
            (rampQueue.GetFirst()) -> Activate();
        }
    }
    void Behavior() {
        /*
            Truck finds the free ramp and waits for storemen to load/unload.   
        */
        celkem++;
        double enterTime = Time;
        if (r -> Full()) {
            je_plno++;
        }

        while (r -> Full()) {
            rampQueue.Insert(this);
            Passivate();
            break;
        }
        Action(enterTime);
    }
    unsigned int serviceType;
    Store * r;
    Store * a;
    Store * b;
};

class TruckGenerator: public Event {
    public: TruckGenerator(
        Store * ramps,
        Store * aWorkers,
        Store * bWorkers
    ): Event() {
        r = ramps;
        a = aWorkers;
        b = bWorkers;
    }
    void Behavior() {
        /* 
            Periodically generate new Truck process.
            50% - truck loading
            50% - truck unloading
        */
        if (Random() <= 0.5)
            (new Truck(TRUCK_LOADING, r, a, b)) -> Activate();
        else
            (new Truck(TRUCK_UNLOADING, r, a, b)) -> Activate();

        double next = Time + Exponential(TRUCK_ACTIVATE_TIME_MEAN);
        Activate(next);
    }
    Store * r;
    Store * a;
    Store * b;
};

int main(int argc, char * argv[]) {
    if (findOptionString(argv, argv + argc, "-h") || findOptionString(argv, argv + argc, "--help")) {
        showHelpMessage(argv[0]);
        exit(0);
    }
    char *outputFile = getOptionValue(argv, argv + argc, "-f", "--file");
    if(outputFile)
        SetOutput(outputFile);

    char * rampCount = getOptionValue(argv, argv + argc, "-r", "--ramps");
    char * aWorkerCount = getOptionValue(argv, argv + argc, "-a", "--awork");
    char * bWorkerCount = getOptionValue(argv, argv + argc, "-b", "--bwork");

    if (!rampCount || !aWorkerCount || !bWorkerCount) {
        showHelpMessage(argv[0]);
        exit(0);
    }

    Store ramps("Ramps", stoi(rampCount));
    Store aWorkers("PROFESSIONAL WORKERS", stoi(aWorkerCount));
    Store bWorkers("ORDINARY WORKERS", stoi(bWorkerCount));

    Init(0, 10000);
    (new TruckGenerator(ramps, aWorkers, bWorkers)) -> Activate();

    Run();

    ramps.Output();
    aWorkers.Output();
    bWorkers.Output();
    truckSystemTime.Output();

    Print("Celkem aut prijelo: %d\n", celkem);
    Print("Je plno: %d\n", je_plno);
    Print("aWorkers Je plno: %d\n", aWorkers_full);
    Print("bWorkers Je plno: %d\n", bWorkers_full);
    Print("Pravdepodobnost, ze bude plno: %f\n", (float) je_plno / celkem);
    Print("Pracovnici nedostupni: %d\n", workers_unavailable);
    Print("loading_count %d, unloading_count %d\n", loading_count, unloading_count);

    Print("Doba obsluha - jeden pracovnik: %f\n", (float) totalServiceTime / 4);
    Print("Kontrola: %d . Storing: %d\n", kontrola, storing);
}