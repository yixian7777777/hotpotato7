#include "taskReassign.h"
#include <algorithm>
#include <iomanip>

using namespace std;

TaskReassign::TaskReassign(
        ThermalModel *thermalModel,
        const PerformanceCounters *performanceCounters,
        int coreRows,
        int coreColumns,
        float criticalTemperature)
    : thermalModel(thermalModel),
      performanceCounters(performanceCounters),
      coreRows(coreRows),
      coreColumns(coreColumns),
      criticalTemperature(criticalTemperature) {
    pb_epoch_length = (thermalModel->pb_epoch_length) * pow(10,-9) ;
    numberUnits = thermalModel->numberUnits;
    numberThermalNodes = thermalModel->numberThermalNodes;
    eigenVectors = thermalModel->eigenVectors;
    eigenValues = thermalModel->eigenValues;
    eigenVectorsInv = thermalModel->eigenVectorsInv;
    HelpW = thermalModel->HelpW;
    ambientTemperature = thermalModel->ambientTemperature;
    //std::cout << "HelpW[0]  " << HelpW[0][0]<< endl;

}

std::vector<int> TaskReassign::map(
        String taskName,
        int taskCoreRequirement,
        const std::vector<bool> &availableCoresRO,
        const std::vector<bool> &activeCores) {

    std::vector<bool> availableCores(availableCoresRO);

    std::vector<int> cores;
    

    //logTemperatures(availableCores);

    for (; taskCoreRequirement > 0; taskCoreRequirement--) {
        //int coldestCore = getColdestCore(availableCores);
        int coldestCore = getPeriodicCore(availableCores);
        cout << "[checking]Selectedc core is " << coldestCore << endl;

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

double* TaskReassign::componentCal(int para, int nActive, double* p){
    nexponentials = new double[numberThermalNodes];
    eachItem = new double*[numberThermalNodes];
    eachComp = new double*[numberThermalNodes];
    Item = new double[numberThermalNodes];

    for(int i = 0; i < numberThermalNodes; i++){
        eachItem[i] = new double[numberThermalNodes];
        eachComp[i] = new double[numberThermalNodes];
    }

    for(int i = 0; i < numberThermalNodes; i++){
        nexponentials[i] = pow((double)M_E, eigenValues[i] * pb_epoch_length  * para) 
            /(1 - pow((double)M_E, eigenValues[i] * pb_epoch_length  * nActive));
    }

    for(int k = 0; k < numberThermalNodes; k++){
        for(int j = 0; j < numberThermalNodes; j++){
            eachItem[k][j] = 0;
            for(int i = 0; i < numberThermalNodes; i++){
                eachItem[k][j] += eigenVectors[k][i]*eigenVectorsInv[i][j]*nexponentials[i];       
            }
        }
        for(int j = 0; j < numberThermalNodes; j++){
            eachComp[k][j] = 0;
            for(int i = 0; i < numberThermalNodes; i++){
                eachComp[k][j] += eachItem[k][i] * HelpW[i][j];
            }
        }
        Item[k] = 0;
        for(int i = 0;i < 16;i++){
            Item[k] +=  eachComp[k][i] * p[i];
        }
    }
    return Item;
}


double TaskReassign::collectComp(){
    double maxPeak = -99999999;
    double pTemp[16] = {6.194, 4.836, 4.325, 1.859, 2.407, 2.315, 4.513, 5.865, 3.058, 4.035, 3.144, 4.943, 3.531, 0.27,0.27,0.27};
    double** P = new double*[numberUnits];  
    for(int i = 0;i < numberUnits;i++){
        P[i] = new double[numberUnits];
        for(int j = 0; j < numberUnits;j++) P[i][j] = pTemp[j];
        double dataTemp = pTemp[0];
        for(int j = 1; j < numberUnits;j++) pTemp[j-1] = pTemp[j];
        pTemp[numberUnits-1] = dataTemp;
    }
    //for(int j = 0; j < numberUnits; j ++) cout << P[1][j] << endl;
    double* tmp = new double[numberUnits];
    double *tmpC;
    //for(int i = 0; i < numberUnits;i++) tmp[i] = 0;
    int para;
    for(int i = 0;i < numberUnits;i++){
        int count = 0;
        para = i;
        for(int i = 0; i < numberUnits;i++) tmp[i] = 0;
        for (int j = 0;j < i;j++){  
            tmpC = componentCal(para,numberUnits,P[count]);
            for(int k = 0;k < numberUnits;k++) tmp[k] += tmpC[k];
            count++;
            para--;
        }
        tmpC = componentCal(0,numberUnits,P[count]);
        //for(int h = 0;h < numberUnits;h++) cout << "P[count] is " << P[count][h] << endl;
        for(int k = 0; k < numberUnits;k++) tmp[k] += tmpC[k];
        //cout << "tmp[0] is " << tmp[0] << endl;
        para = numberUnits - 1;
        for(int j = i+1; j < numberUnits;j++){
            count++;
            tmpC = componentCal(para,numberUnits,P[count]);
            for(int k = 0; k < numberUnits;k++) tmp[k] += tmpC[k];
            para--;
        }
        for(int k = 0; k < numberUnits;k++){
            if(maxPeak < tmp[k]) maxPeak = tmp[k];
        } 
    }
    return maxPeak + ambientTemperature;       

}
double* TaskReassign::matrixAdd(double* A, double* B){
    double*C = new double[numberUnits];
    for (int i = 0;i < numberUnits;i++){
        C[i] = A[i] + B[i];
    }
    return C;
}

std::vector<migration> TaskReassign::migrate(
        SubsecondTime time,
        const std::vector<int> &taskIds,
        const std::vector<bool> &activeCores) {


    double predictTemp = collectComp();
    cout << "The peak temperature is " << collectComp() << endl;

    std::vector<migration> migrations;

    std::vector<bool> availableCores(coreRows * coreColumns);
    for (int c = 0; c < coreRows * coreColumns; c++) {
        availableCores.at(c) = taskIds.at(c) == -1;
    }

    for (int c = 0; c < coreRows * coreColumns; c++) {
        if (activeCores.at(c)) {
            float temperature = performanceCounters->getTemperatureOfCore(c);
            if (temperature > criticalTemperature  || 1) {
                cout << "[Scheduler][coldestCore-migrate]: core" << c << " too hot (";
                cout << fixed << setprecision(1) << temperature << ") -> migrate";
                //logTemperatures(availableCores);

                //int targetCore = getColdestCore(availableCores);
                int targetCore = getPeriodicCore(activeCores);

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



int TaskReassign::getColdestCore(const std::vector<bool> &availableCores) {
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

int TaskReassign::getPeriodicCore(const std::vector<bool> &activeCores){
    int periodicCore = 0;
    for (int c = 0; c < coreRows * coreColumns; c++) {  
        if (activeCores.at(c)) {
            if(c == 0) periodicCore = 2;
            else if (c == 2) periodicCore = 4;
            else if (c == 4) periodicCore = 6;
            else if (c == 6) periodicCore = 0;
        }
    }
    return periodicCore;

    
}



void TaskReassign::logTemperatures(const std::vector<bool> &availableCores) {
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