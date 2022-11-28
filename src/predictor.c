//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"

const char *studentName = "NAME";
const char *studentID   = "PID";
const char *email       = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

// gShare
uint8_t* globalHistoryTable;
uint8_t globalHistoryTableEntries;
uint8_t globalHistory;
uint8_t indexMask;

// touranment
uint8_t* localHistoryTable;
uint8_t* selector;
uint8_t localHistoryTableEntries;
uint8_t selectorTableEntries;
uint8_t localIndexMask;
uint8_t globalIndexMask;
uint8_t selectorIndexMask;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
void
init_predictor()
{
  switch (bpType) {
    case STATIC:
      return;
    case GSHARE:
      // init gShare
      {
        globalHistoryTableEntries = 1 << ghistoryBits;
        indexMask = globalHistoryTableEntries - 1;
        
        globalHistoryTable = (uint8_t*)malloc(globalHistoryTableEntries * sizeof(uint8_t));

        for (uint8_t i = 0; i < globalHistoryTableEntries; i++) {
          globalHistoryTable[i] = WN; 
        }
      }
      globalHistory = 0;
      break;

    case TOURNAMENT:
      // init tournament
      {
        globalHistoryTableEntries = 1 << ghistoryBits;
        localHistoryTableEntries = 1 << lhistoryBits;
        selectorTableEntries = 1 << pcIndexBits;

        globalHistoryTable = (uint8_t*)malloc(globalHistoryTableEntries * sizeof(uint8_t));
        localHistoryTable = (uint8_t*)malloc(localHistoryTableEntries * sizeof(uint8_t));
        selector = (uint8_t*)malloc(selectorTableEntries * sizeof(uint8_t));

        for (uint8_t i = 0; i < globalHistoryTableEntries; i++) {
          globalHistoryTable[i] = WN; 
          selector[i] = TWT; // weal taken global predictor
        }

        for (uint8_t i = 0; i < localHistoryTableEntries; i++) {
          localHistoryTable[i] = WN; 
        }

      }
      break;

    case CUSTOM:
    default:
      break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
uint8_t
make_prediction(uint32_t pc)
{
  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      // predict gShare
      {
        uint32_t xorResult = (pc & indexMask) ^ (globalHistory & indexMask);

        if (globalHistoryTable[xorResult] == WT || globalHistoryTable[xorResult] == ST){
          return TAKEN;
        } else {
          return NOTTAKEN;
        }
      }
      break;

    case TOURNAMENT:
      // predict tournament
      {
        uint8_t localPrediction;
        uint8_t globalPrediction;

        uint8_t _pc = pc & selectorIndexMask;

        // local predictor
        uint8_t localXorResult = (localHistoryTable[_pc] & localIndexMask);

        if (localHistoryTable[localXorResult] == WT || localHistoryTable[localXorResult] == ST){
          localPrediction = TAKEN;
        } else {
          localPrediction = NOTTAKEN;
        }

        // global predictor
        uint32_t globalXorResult = globalHistory & globalIndexMask;

        if (globalHistoryTable[globalXorResult] == WT || globalHistoryTable[globalXorResult] == ST){
          globalPrediction = TAKEN;
        } else {
          globalPrediction = NOTTAKEN;
        }

        // selector
        if (selector[globalXorResult] == TWT || selector[globalXorResult] == TST) {
          return globalPrediction;
        } else {
          return localPrediction;
        }
      }
      break;

    case CUSTOM:
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{
  switch (bpType) {
  case STATIC:
    break;
  case GSHARE:

    // train gShare
    {
      uint8_t prediction = (pc & indexMask) ^ (globalHistory & indexMask);

      if(outcome == TAKEN && globalHistoryTable[prediction] < ST){
        globalHistoryTable[prediction]++;
      } 
      if (outcome == NOTTAKEN && globalHistoryTable[prediction] > SN){
        globalHistoryTable[prediction]--;
      }

      // Shift global history bits to left and append outcome
      globalHistory = (globalHistory << 1) | outcome;
    }
    break;
  case TOURNAMENT:
    // train tournament
    {
      uint8_t localResult;
      uint8_t globalResult;

      uint8_t _pc = pc & selectorIndexMask;

      // local predictor
      uint8_t localXorResult = (localHistoryTable[_pc] & localIndexMask);

      if(outcome == TAKEN && globalHistoryTable[localXorResult] < ST){
        localHistoryTable[localXorResult]++;
      } 
      if (outcome == NOTTAKEN && globalHistoryTable[localXorResult] > SN){
        localHistoryTable[localXorResult]--;
      }

      localHistoryTable[_pc] = (localHistoryTable[_pc] << 1 | outcome) & localIndexMask;

      // global predictor
      uint32_t globalXorResult = globalHistory & globalIndexMask;

      if(outcome == TAKEN && globalHistoryTable[globalXorResult] < ST){
        globalHistoryTable[globalXorResult]++;
      } 
      if (outcome == NOTTAKEN && globalHistoryTable[globalXorResult] > SN){
        globalHistoryTable[globalXorResult]--;
      }

      globalHistoryTable[_pc] = (globalHistoryTable[_pc] << 1 | outcome) & globalIndexMask;

      // selector
      if (localHistoryTable[localXorResult] == WT || localHistoryTable[localXorResult] == ST){
        if (outcome == TAKEN) {
          localResult = LOCAL_CORRECT;
        } else {
          localResult = LOCAL_INCORRECT;
        }
      } else {
        if (outcome == NOTTAKEN) {
          localResult = LOCAL_CORRECT;
        } else {
          localResult = LOCAL_INCORRECT;
        }
      }

      if (globalHistoryTable[globalXorResult] == WT || globalHistoryTable[globalXorResult] == ST){
        if (outcome == TAKEN) {
          globalResult = GLOBAL_CORRECT;
        } else {
          globalResult = GLOBAL_INCORRECT;
        }
      } else {
        if (outcome == NOTTAKEN) {
          globalResult = GLOBAL_CORRECT;
        } else {
          globalResult = GLOBAL_INCORRECT;
        }
      }

      if((globalResult - localResult) == 1 && selector[globalXorResult] < TST){
        selector[globalXorResult]++;
      }
      if((globalResult - localResult) == -1 && selector[globalXorResult] > TSN){
        selector[globalXorResult]--;
      }
    }
    break;
  case CUSTOM:
  default:
    break;
  }
}