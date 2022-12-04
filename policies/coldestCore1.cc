#include "coldestCore.h"

#include <iomanip>

using namespace std;

ColdestCore::ColdestCore(
        const PerformanceCounters *performanceCounters,
        int coreRows,
        int coreColumns,
        float criticalTemperature)
    : performanceCounters(performanceCounters),
      coreRows(coreRows),
      coreColumns(coreColumns),
      criticalTemperature(criticalTemperature) {

}

std::vector<int> ColdestCore::map(
        String taskName,
        int taskCoreRequirement,
        const std::vector<bool> &availableCoresRO,
        const std::vector<bool> &activeCores) {

    std::vector<bool> availableCores(availableCoresRO);

    std::vector<int> cores;

    //logTemperatures(availableCores);

    for (; taskCoreRequirement > 0; taskCoreRequirement--) {
        //cout << "taskCoreRequirement is " << taskCoreRequirement << endl;
        //int coldestCore = getColdestCore(availableCores);
        int coldestCore = getPeriodicCore(availableCores);

        if (coldestCore == -1) {
            // not enough free cores
            std::vector<int> empty;
            return empty;
        } else {
            cores.push_back(coldestCore);
            availableCores.at(coldestCore) = false;
        }
    }

    return cores;
}

std::vector<migration> ColdestCore::migrate(
        SubsecondTime time,
        const std::vector<int> &taskIds,
        const std::vector<bool> &activeCores) {

    std::vector<migration> migrations;

    std::vector<bool> availableCores(coreRows * coreColumns);
    for (int c = 0; c < coreRows * coreColumns; c++) {
        availableCores.at(c) = taskIds.at(c) == -1;
    }

    for (int c = 0; c < coreRows * coreColumns; c++) {
        // if (activeCores.at(c)) {
        //     float temperature = performanceCounters->getTemperatureOfCore(c);
        //     if (temperature > criticalTemperature || 1) {
        //         cout << "[Scheduler][coldestCore-migrate]: core" << c << " too hot (";
        //         cout << fixed << setprecision(1) << temperature << ") -> migrate";
        //         //logTemperatures(availableCores);


        if (activeCores.at(c)) {
            float temperature = performanceCounters->getTemperatureOfCore(c);
            if (temperature > criticalTemperature || 1) {
                cout << "[Scheduler][coldestCore-migrate]: core" << c << " too hot (";
                cout << fixed << setprecision(1) << temperature << ") -> migrate";
                cout << endl;
                //logTemperatures(availableCores);

                //int targetCore = getColdestCore(availableCores);
                int targetCore = getPeriodicCore(availableCores);

                if (targetCore == -1) {
                    cout << "[Scheduler][coldestCore-migrate]: no target core found, cannot migrate" << endl;
                } else {
                    migration m;
                    m.fromCore = c;
                    m.toCore = targetCore;
                    m.swap = false;
                    migrations.push_back(m);
                    availableCores.at(targetCore) = false;
                }
            }
        }

    }
    return migrations;
}

int ColdestCore::getColdestCore(const std::vector<bool> &availableCores) {
    int coldestCore = -1;
    float coldestTemperature = 0;
    //iterate all cores to find coldest
    for (int c = 0; c < coreRows * coreColumns; c++) {
        if (availableCores.at(c)) {
            float temperature = performanceCounters->getTemperatureOfCore(c);
            if ((coldestCore == -1) || (temperature < coldestTemperature)) {
                coldestCore = c;
                coldestTemperature = temperature;
            }
        }
    }   
    return coldestCore;
}

int ColdestCore::getPeriodicCore(const std::vector<bool> &availableCores){
    for (int c = 0; c < coreRows * coreColumns; c++) {  
        if (availableCores.at(c)) {
            if(c == 0)  return availableCores.at(56) ?56 : c;
            else if (c == 1) return availableCores.at(48) ?48 : c;
            else if (c == 48) return availableCores.at(62) ?62 : c;
            else if (c == 56) return availableCores.at(63) ?63 : c;
            else if (c == 62) return availableCores.at(14) ?14 : c;
            else if (c == 63) return availableCores.at(7) ?7 : c;
            else if (c == 7) return availableCores.at(0) ?0 : c;
            else if (c == 14) return availableCores.at(1) ?1 : c;
        }
    }

    
}

void ColdestCore::logTemperatures(const std::vector<bool> &availableCores) {
    cout << "[Scheduler][coldestCore-map]: temperatures of available cores:" << endl;
    for (int y = 0; y < coreRows; y++) {
        for (int x = 0; x < coreColumns; x++) {
            if (x > 0) {
                cout << " ";
            }
            int coreId = y * coreColumns + x;

            if (!availableCores.at(coreId)) {
                cout << "  - ";
            } else {
                float temperature = performanceCounters->getTemperatureOfCore(coreId);
                cout << fixed << setprecision(1) << temperature;
            }
        }
        cout << endl;
    }
}