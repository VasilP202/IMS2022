/*  FIT VUT - Modelování a simulace
**  SHO v logistice
**
**  Author: Vasil Poposki
**  Date: 05. 12. 2022
*/

#include "simlib.h"
#include <iostream>
#include <algorithm>

using namespace std;

#define SIMULATION_END_TIME 1000000

#define TRUCK_UNLOADING 0
#define TRUCK_LOADING 1

#define TRUCK_ACTIVATE_TIME_MEAN 60
#define A_WORKER_SERVICE_COUNT 1
#define B_WORKER_SERVICE_COUNT 2
#define A_WORKER_STORING_SERVICE_COUNT 2
#define B_WORKER_CONTROL_SERVICE_COUNT 1

Queue rampQueue("Cekani na rampu");
Queue serviceQueue("Cekani na obsluhu");

int rampsFullCount = 0;
int aWorkersFullCount = 0;
int bWorkersFullCount = 0;
int workersUnavailable = 0;
int truckCount = 0;
int truckService = 0;

int loadingCount = 0;
int unloadingCount = 0;

double totalServiceTime = 0;
double totalAWorkersTime = 0;
double totalBWorkersTime = 0;

void showHelpMessage(string name) {
    cerr << "Usage: " << name << " <option(s)>\n" <<
        "Options:\n" <<
        "\t-r / --ramps COUNT\t\t Number of open ramps in the warehouse.\n" <<
        "\t-a / --awork COUNT\t\t Number of professional/qualified workers.\n" <<
        "\t-b / --bwork COUNT\t\t Number of ordinary/unqualified workers.\n\n" <<
        "\t[-f / --file FILENAME]\t\t Save output to the file FILENAME.\n" <<
        "\t-h / --help\tShow this help message.\n" <<
        "Validation:\n" <<
        "\t RAMP COUNT > 0\n" <<
        "\t A WORKER COUNT > 1\n" <<
        "\t B WORKER COUNT > 1\n" <<
        endl;
}

bool findOptionString(char * start[], char * end[],
    const string & optionString) {
    return find(start, end, optionString) != end;
}

char * getOptionValue(char * start[], char * end[],
    const string & optionString,
        const string & optionStringLong) {
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

void printOutputStats() {
    Print("+--------------------------------------------------------------------------+\n");
    Print(
        "Number of trucks: %d; Unloadings: %d. Loadings: %d.\n-\tLoading/unloading was performed %d time(s).\n",
        truckCount, unloadingCount, loadingCount, truckService
    );
    Print(
        "-\tTruck could not find free ramp %d time(s) (%f%).\n\n",
        rampsFullCount, (float) rampsFullCount / truckCount * 100
    );
    Print("Workers A full capacity: %f%\n", (float) aWorkersFullCount / truckCount * 100);
    Print("Workers B full capacity: %f%\n", (float) bWorkersFullCount / truckCount * 100);
    Print(
        "Storemen were not available for loading/unloading %d time(s) (%f%).\n",
        workersUnavailable, (float) workersUnavailable / truckCount * 100
    );
    Print(
        "\nAverage time spent per storeman:\n"
        "-\tWorker A (loading/unloading + storage): %f%\n"
        "-\tWorker B (loading/unloading + palettes control): %f%\n",
        (float) totalAWorkersTime / (float) SIMULATION_END_TIME * 100,
        ((float) totalBWorkersTime / (float) SIMULATION_END_TIME * 100)
    );
    Print("+--------------------------------------------------------------------------+\n");
}

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
        double enterTime = Time;

        Enter( * a, A_WORKER_STORING_SERVICE_COUNT);

        Wait(Uniform(25, 35));
        Leave( * a, A_WORKER_STORING_SERVICE_COUNT);
        totalAWorkersTime += (Time - enterTime) * A_WORKER_STORING_SERVICE_COUNT / a -> Capacity();
    }
    Store * a;
};

class ControlService: public Process {
    public: ControlService(Store * bWorkers, Store * aWorkers): Process() {
        b = bWorkers;
        a = aWorkers;
    }
    void Behavior() {
        /* 
            Checking of received goods after unloading the truck.
            Checking is done by ordinary workers.
        */
        double enterTime = Time;

        Enter( * b, B_WORKER_CONTROL_SERVICE_COUNT);
        Wait(Uniform(15, 25));

        Leave( * b, B_WORKER_CONTROL_SERVICE_COUNT);
        (new StoringService(a)) -> Activate(); //Activate storing process
        totalBWorkersTime += (Time - enterTime) * B_WORKER_CONTROL_SERVICE_COUNT / b -> Capacity();
    }
    Store * b;
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
        double enterTime = Time;
        Enter( * a, A_WORKER_SERVICE_COUNT);
        Enter( * b, B_WORKER_SERVICE_COUNT);

        Wait(Uniform(20, 45)); // Unloading/Loading

        Leave( * a, A_WORKER_SERVICE_COUNT);
        Leave( * b, B_WORKER_SERVICE_COUNT);

        totalAWorkersTime += (Time - enterTime) * A_WORKER_SERVICE_COUNT / a -> Capacity();
        totalBWorkersTime += (Time - enterTime) * B_WORKER_SERVICE_COUNT / b -> Capacity();
        truckService++;
    }

    void Action(double enterTime) {
        /*
            Finds free storemen for unloading/loading.
            After the service is done, truck releases ramp.
        */
        Enter( * r, 1);

        if (a -> Full()) aWorkersFullCount++;
        if (b -> Full()) bWorkersFullCount++;
        if (!(a -> Free() >= A_WORKER_SERVICE_COUNT && b -> Free() >= B_WORKER_SERVICE_COUNT)) {
            // Stats
            workersUnavailable++;
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

        Leave( * r, 1);

        if (serviceType == TRUCK_UNLOADING) {
            unloadingCount++;
            (new ControlService(b, a)) -> Activate(); //Activate goods control process
            //(new StoringService(a)) -> Activate(); //Activate storing process
        } else if (serviceType == TRUCK_LOADING) loadingCount++;

        if (rampQueue.Length() > 0) {
            (rampQueue.GetFirst()) -> Activate();
        }
    }
    void Behavior() {
        /*
            Truck finds the free ramp and waits for storemen to load/unload.   
        */
        truckCount++;
        double enterTime = Time;
        if (r -> Full())
            rampsFullCount++;

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
    char * outputFile = getOptionValue(argv, argv + argc, "-f", "--file");
    if (outputFile)
        SetOutput(outputFile);

    char * rampCount = getOptionValue(argv, argv + argc, "-r", "--ramps");
    char * aWorkerCount = getOptionValue(argv, argv + argc, "-a", "--awork");
    char * bWorkerCount = getOptionValue(argv, argv + argc, "-b", "--bwork");

    if (!rampCount || !aWorkerCount || !bWorkerCount || stoi(rampCount) < 1 || stoi(aWorkerCount) < 2 || stoi(bWorkerCount) < 2) {
        showHelpMessage(argv[0]);
        exit(0);
    }

    Store ramps("Ramps", stoi(rampCount)); // Ramps store 
    Store aWorkers("PROFESSIONAL WORKERS", stoi(aWorkerCount)); // Professional workers store
    Store bWorkers("ORDINARY WORKERS", stoi(bWorkerCount)); // Ordinary workers store

    Init(0, SIMULATION_END_TIME); // Initialize simulation

    (new TruckGenerator(ramps, aWorkers, bWorkers)) -> Activate(); // Activate truck process generator

    Run(); // Run simulation

    ramps.Output();
    rampQueue.Output();
    aWorkers.Output();
    bWorkers.Output();

    printOutputStats();

    return 0;
}