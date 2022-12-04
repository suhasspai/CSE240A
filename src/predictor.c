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
uint64_t* globalHistoryTable;
uint64_t globalHistoryTableEntries;
uint64_t globalHistory;
uint64_t indexMask;
uint64_t xorResult;

// tournament
uint64_t* localHistoryTable;
uint64_t* localPredictionTable;
uint64_t* selector;
uint64_t localHistoryTableEntries;
uint64_t selectorTableEntries;
uint64_t localIndexMask;
uint64_t globalIndexMask;
uint64_t selectorIndexMask;

uint64_t localXorResult;
uint64_t globalXorResult;

uint64_t localPrediction;
uint64_t globalPrediction;

uint64_t localResult;
uint64_t globalResult;
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
        
        globalHistoryTable = (uint64_t*)malloc(globalHistoryTableEntries * sizeof(uint64_t));

        for (uint64_t i = 0; i < globalHistoryTableEntries; i++) {
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

        globalIndexMask = globalHistoryTableEntries - 1;
        localIndexMask = localHistoryTableEntries - 1;
        selectorIndexMask = selectorTableEntries - 1;

        localHistoryTable = (uint64_t*)malloc(selectorTableEntries * sizeof(uint64_t));
        globalHistoryTable = (uint64_t*)malloc(globalHistoryTableEntries * sizeof(uint64_t));
        localPredictionTable = (uint64_t*)malloc(localHistoryTableEntries * sizeof(uint64_t));
        selector = (uint64_t*)malloc(globalHistoryTableEntries * sizeof(uint64_t));

        for (uint64_t i = 0; i < globalHistoryTableEntries; i++) {
          globalHistoryTable[i] = WN; 
          selector[i] = TWT; // weak taken global predictor
        }

        for (uint64_t i = 0; i < localHistoryTableEntries; i++) {
          localHistoryTable[i] = 0; 
        }

        for (uint64_t i = 0; i < localHistoryTableEntries; i++) {
          localPredictionTable[i] = WN; 
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
uint64_t
make_prediction(uint64_t pc)
{
  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      // predict gShare
      {
        uint64_t xorResult = (pc & indexMask) ^ (globalHistory & indexMask);

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
        uint64_t _pc = pc & selectorIndexMask;

        // local predictor
        localXorResult = (localHistoryTable[_pc] & localIndexMask);

        if (localPredictionTable[localXorResult] == WT || localPredictionTable[localXorResult] == ST){
          localPrediction = TAKEN;
        } else {
          localPrediction = NOTTAKEN;
        }

        // global predictor
        globalXorResult = globalHistory & globalIndexMask;

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
train_predictor(uint64_t pc, uint64_t outcome)
{
  switch (bpType) {
  case STATIC:
    break;
  case GSHARE:

    // train gShare
    {
      uint64_t xorResult = (pc & indexMask) ^ (globalHistory & indexMask);

      if(outcome == TAKEN && globalHistoryTable[xorResult] < ST){
        globalHistoryTable[xorResult]++;
      } 
      if (outcome == NOTTAKEN && globalHistoryTable[xorResult] > SN){
        globalHistoryTable[xorResult]--;
      }

      // Shift global history bits to left and append outcome
      globalHistory = (globalHistory << 1) | outcome;
    }
    break;
  case TOURNAMENT:
    // train tournament
    {
      uint64_t _pc = pc & selectorIndexMask;

      // local predictor
      uint64_t localMask = localXorResult - 1;
      localXorResult = (localHistoryTable[_pc] & localIndexMask);

      if(outcome == TAKEN && localPredictionTable[localXorResult] < ST){
        localPredictionTable[localXorResult]++;
      } 
      if (outcome == NOTTAKEN && localPredictionTable[localXorResult] > SN){
        localPredictionTable[localXorResult]--;
      }

      localHistoryTable[_pc] = (localHistoryTable[_pc] << 1 | outcome) & localIndexMask;

      // global predictor
      globalXorResult = globalHistory & globalIndexMask;

      if(outcome == TAKEN && globalHistoryTable[globalXorResult] < ST){
        globalHistoryTable[globalXorResult]++;
      } 
      if (outcome == NOTTAKEN && globalHistoryTable[globalXorResult] > SN){
        globalHistoryTable[globalXorResult]--;
      }

      globalHistory = (globalHistory << 1 | outcome) & globalIndexMask;

      // selector
      if (localPredictionTable[localXorResult] == WT || localPredictionTable[localXorResult] == ST){
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