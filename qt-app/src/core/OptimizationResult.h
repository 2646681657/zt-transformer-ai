#ifndef OPTIMIZATIONRESULT_H
#define OPTIMIZATIONRESULT_H

struct OptimizationResult {
    int schemeIdx = 0;
    double costCuFeOil = 0.0;
    double costCuFe = 0.0;
    double coreD = 0.0;
    double coreL = 0.0;
    int lvTurns = 0;
    double lvRuleT = 0.0;
    double lvRuleW = 0.0;
    double hvRuleT = 0.0;
    double hvRuleW = 0.0;
    int hvLayers = 0;
    int lvOilDucts = 0;
    int hvOilDucts = 0;
    double lvToYoke = 0.0;
    double mainDuct = 0.0;
    int lvHalfOilDucts = 0;
    int hvHalfOilDucts = 0;
    double lvHalfDist = 0.0;
};

#endif // OPTIMIZATIONRESULT_H
