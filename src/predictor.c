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

// gshare
uint32_t* globalHistoryTable;
uint32_t globalHistoryTableEntries;
uint32_t globalHistory;
uint32_t indexMask;
uint32_t xorResult;

// tournament
uint32_t* localHistoryTable;
uint32_t* localPredictionTable;
uint32_t* selector;
uint32_t localHistoryTableEntries;
uint32_t selectorTableEntries;
uint32_t localIndexMask;
uint32_t globalIndexMask;
uint32_t selectorIndexMask;

uint8_t localXorResult;
uint8_t globalXorResult;

uint8_t localPrediction;
uint8_t globalPrediction;

uint8_t localResult;
uint8_t globalResult;
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
      // init gshare
      {
        globalHistoryTableEntries = 1 << ghistoryBits;
        indexMask = globalHistoryTableEntries - 1;
        
        globalHistoryTable = (uint32_t*)malloc(globalHistoryTableEntries * sizeof(uint32_t));

        for (uint32_t i = 0; i < globalHistoryTableEntries; i++) {
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

        localHistoryTable = (uint32_t*)malloc(selectorTableEntries * sizeof(uint32_t));
        globalHistoryTable = (uint32_t*)malloc(globalHistoryTableEntries * sizeof(uint32_t));
        localPredictionTable = (uint32_t*)malloc(localHistoryTableEntries * sizeof(uint32_t));
        selector = (uint32_t*)malloc(globalHistoryTableEntries * sizeof(uint32_t));

        for (uint32_t i = 0; i < globalHistoryTableEntries; i++) {
          globalHistoryTable[i] = WN; 
          selector[i] = TWT; // weak taken global predictor
        }

        for (uint32_t i = 0; i < localHistoryTableEntries; i++) {
          localHistoryTable[i] = 0; 
        }

        for (uint32_t i = 0; i < localHistoryTableEntries; i++) {
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
uint8_t
make_prediction(uint32_t pc)
{
  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      // predict gshare
      {
        xorResult = (pc & indexMask) ^ (globalHistory & indexMask);
        return (globalHistoryTable[xorResult] == WT || globalHistoryTable[xorResult] == ST)?TAKEN:NOTTAKEN;
      }
      break;

    case TOURNAMENT:
      // predict tournament
      {
        uint32_t _pc = pc & selectorIndexMask;

        // local predictor
        localXorResult = (localHistoryTable[_pc] & localIndexMask);
        localPrediction = (localPredictionTable[localXorResult] == WT || localPredictionTable[localXorResult] == ST)?TAKEN:NOTTAKEN;

        // global predictor
        globalXorResult = globalHistory & globalIndexMask;
        globalPrediction = (globalHistoryTable[globalXorResult] == WT || globalHistoryTable[globalXorResult] == ST)?TAKEN:NOTTAKEN;

        // selector
        return (selector[globalXorResult] == TWT || selector[globalXorResult] == TST)?globalPrediction:localPrediction;
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

    // train gshare
    {
      xorResult = (pc & indexMask) ^ (globalHistory & indexMask);

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
      uint32_t _pc = pc & selectorIndexMask;

      // local predictor      
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
        localResult = (outcome == TAKEN)?LOCAL_CORRECT:LOCAL_CORRECT;
      } else {
        localResult = (outcome == NOTTAKEN)?LOCAL_CORRECT:LOCAL_CORRECT;
      }

      if (globalHistoryTable[globalXorResult] == WT || globalHistoryTable[globalXorResult] == ST){
        globalResult = (outcome == TAKEN)?GLOBAL_CORRECT:GLOBAL_INCORRECT;
      } else {
        globalResult = (outcome == NOTTAKEN)?GLOBAL_CORRECT:GLOBAL_INCORRECT;
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