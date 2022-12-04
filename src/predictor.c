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

// GShare
uint8_t* globalHistoryTable;
uint16_t globalHistoryTableEntries;
uint16_t globalHistory;
uint16_t indexMask;

// Tournament
uint16_t* localHistoryTable;
uint16_t* selector;
uint16_t localHistoryTableEntries;
uint16_t selectorTableEntries;
uint16_t localIndexMask;
uint16_t globalIndexMask;
uint16_t selectorIndexMask;

// Custom - TAGE
uint64_t customGlobalHistory; // Global history
uint8_t* basePredictor; // Base predictor with 2-bit bimodal counter
uint16_t** taggedPredictors; // 4 tagged predictors with 8-, 16-, 32-, and 64-bit histories
int tableAccessed;
int customGlobalHistoryBits[] = {8, 16, 32, 64};
uint8_t altpred;
int useAltOnNa = 0;
int uBitCounter = 0;

void resetUbitsInPredictor()
{
  int taggedPredictorSize = 1 << 10;
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < taggedPredictorSize; j++)
    {
      uint16_t entry = taggedPredictors[i][j];
      entry >>= 1;
      entry <<= 1;
      taggedPredictors[i][j] = entry;
    }
  }
}

void decrementAltCounter()
{
  if (useAltOnNa > -8)
  {
    useAltOnNa--;
  }
}

void decrementUbitCounter()
{
  if (uBitCounter > -256)
  {
    uBitCounter--;
  }
}

void incrementAltCounter()
{
  if (useAltOnNa < 7)
  {
    useAltOnNa++;
  }
}

void incrementUbitCounter()
{
  if (uBitCounter < 255)
  {
    uBitCounter++;
  }
  else
  {
    resetUbitsInPredictor();
  }
}

uint16_t calculateIndexTaggedPredictor(uint32_t pc, uint64_t history, int globalHistoryBits)
{
  int indexSize = 10;
  uint16_t indexMask = (1 << indexSize) - 1;
  uint16_t index = pc & indexMask;
  uint32_t _pc = pc >> indexSize;
  index ^= (_pc & indexMask);
  _pc >>= indexSize;
  index ^= (_pc & indexMask);
  _pc >>= indexSize;
  index ^= _pc;
  uint64_t _history = history;
  for (int j = 0; j < globalHistoryBits; j += indexSize)
  {
    index ^= (_history & indexMask);
    _history >>= indexSize;
  }

  return index;
}

uint8_t calculateCustomTag(uint32_t pc, uint64_t history, int globalHistoryBits)
{
  int tagSize = 8, csr2Size = 7;
  uint8_t tagMask1 = (1 << tagSize) - 1, tagMask2 = (1 << csr2Size) - 1;
  uint8_t csr1 = 0;//history & tagMask1;
  //history = history >> tagSize;
  for (int k = 0; k < globalHistoryBits; k += tagSize)
  {
    csr1 ^= (history & tagMask1);
    history >>= tagSize;
  }

  uint8_t csr2 = 0;//history & tagMask2;
  //history = history >> csr2Size;
  for (int l = 0; l < globalHistoryBits; l += csr2Size)
  {
    csr2 ^= (history & tagMask2);
    history >>= csr2Size;
  }

  return (pc & tagMask1) ^ csr1 ^ (csr2 << 1);
}

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
void init_predictor()
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

        for (uint16_t i = 0; i < globalHistoryTableEntries; i++) {
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
        localIndexMask = localHistoryTableEntries - 1;
        selectorTableEntries = 1 << pcIndexBits;
        selectorIndexMask = selectorTableEntries - 1;

        globalHistoryTable = (uint8_t*)malloc(globalHistoryTableEntries * sizeof(uint8_t));
        localHistoryTable = (uint16_t*)malloc(localHistoryTableEntries * sizeof(uint16_t));
        selector = (uint16_t*)malloc(selectorTableEntries * sizeof(uint16_t));

        for (uint16_t i = 0; i < globalHistoryTableEntries; i++) {
          globalHistoryTable[i] = WN; 
          selector[i] = TWT; // weak taken global predictor
        }

        for (uint16_t i = 0; i < localHistoryTableEntries; i++) {
          localHistoryTable[i] = WN; 
        }

      }
      break;

    case CUSTOM:
      {
        int basePredictorSize = 1 << 12;
        basePredictor = (uint8_t*)calloc(basePredictorSize, sizeof(uint8_t));
        int taggedPredictorSize = 1 << 10;
        taggedPredictors = (uint16_t**)malloc(4 * sizeof(uint16_t*));
        for (int i = 0; i < 4; i++)
          taggedPredictors[i] = (uint16_t*)calloc(taggedPredictorSize, sizeof(uint16_t));

        break;
      }
    default:
      break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
uint8_t make_prediction(uint32_t pc)
{
  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      // predict gShare
      {
        uint32_t xorResult = (pc & indexMask) ^ (globalHistory & indexMask);

        if (globalHistoryTable[xorResult] == WT || globalHistoryTable[xorResult] == ST) {
          return TAKEN;
        } else {
          return NOTTAKEN;
        }
      }
      break;

    case TOURNAMENT:
      // predict tournament
      {
        uint16_t localPrediction;
        uint16_t globalPrediction;

        uint16_t _pc = pc & selectorIndexMask;

        // local predictor
        uint16_t localXorResult = (localHistoryTable[_pc] & localIndexMask);

        if (localHistoryTable[localXorResult] == WT || localHistoryTable[localXorResult] == ST) {
          localPrediction = TAKEN;
        } else {
          localPrediction = NOTTAKEN;
        }

        // global predictor
        uint32_t globalXorResult = globalHistory & globalIndexMask;

        if (globalHistoryTable[globalXorResult] == WT || globalHistoryTable[globalXorResult] == ST) {
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
      // Get base prediction
      tableAccessed = -1;
      uint16_t basePredictorMask = (1 << 12) - 1;
      uint16_t basePredictorIndex = basePredictorMask & pc;
      uint8_t basePrediction = basePredictor[basePredictorIndex] && (1 << 1);
      altpred = basePrediction;
      uint8_t taggedCounter, taggedPrediction;
      for (int i = 0; i < 4; i++)
      {
        uint64_t historyMask = (1 << customGlobalHistoryBits[i]) - 1;
        uint64_t history = customGlobalHistory & historyMask;
        uint64_t _history = history;

        // Calculate tag
        uint8_t computedTag = calculateCustomTag(pc, history, customGlobalHistoryBits[i]);

        // Calculate index
        uint16_t index = calculateIndexTaggedPredictor(pc, history, customGlobalHistoryBits[i]);

        // Get prediction
        uint16_t counter = taggedPredictors[i][index];
        if ((counter & 1) == 0)
        {
          continue;
        }
        
        int usefulBitSize = 1, counterSize = 3;
        uint16_t _counter = counter;
        uint8_t usefulBitMask = (1 << usefulBitSize) - 1;
        uint8_t usefulBits = _counter && usefulBitMask;
        _counter >>= usefulBitSize;
        uint8_t counterMask = (1 << counterSize) - 1;
        uint8_t counterBits = _counter && counterMask;
        _counter >>= counterSize;
        uint8_t tagSize = 8;
        uint8_t tagMask = (1 << tagSize) - 1;
        uint8_t tagBits = _counter && tagMask;
        if (tagBits == computedTag)
        {
          if (tableAccessed != -1)
            altpred = taggedPrediction;

          taggedCounter = counterBits;
          taggedPrediction = taggedCounter >> (counterSize - 1);
          tableAccessed = i;
        }
      }

      if (tableAccessed != -1)
      {
        if ((taggedCounter >= SWN && taggedCounter <= SWT) || useAltOnNa < 0)
        {
          decrementAltCounter();
          return taggedPrediction;
        }
        
        incrementAltCounter();
        return altpred;
      }

      return basePrediction;
      break;
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
void train_predictor(uint32_t pc, uint8_t outcome)
{
  switch (bpType) {
    case STATIC:
      break;
    case GSHARE:
      // train gShare
      {
        uint16_t prediction = (pc & indexMask) ^ (globalHistory & indexMask);

        if (outcome == TAKEN && globalHistoryTable[prediction] < ST) {
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
        uint16_t localResult;
        uint16_t globalResult;

        uint16_t _pc = pc & selectorIndexMask;

        // local predictor
        uint16_t localXorResult = (localHistoryTable[_pc] & localIndexMask);

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
      {
        uint8_t prediction;
        if (tableAccessed == -1)
        {
          uint16_t basePredictorMask = (1 << 12) - 1;
          uint16_t basePredictorIndex = basePredictorMask & pc;
          uint8_t predictorState = basePredictor[basePredictorIndex];
          if (predictorState == ST || predictorState == WT)
          {
            prediction = TAKEN;
          }
          else
          {
            prediction = NOTTAKEN;
          }

          if (outcome == TAKEN && predictorState < ST)
          {
            basePredictor[basePredictorIndex]++;
          }

          if (outcome == NOTTAKEN && predictorState > SN)
          {
            basePredictor[basePredictorIndex]--;
          }
        }
        else
        {
          uint64_t historyMask = (1 << customGlobalHistoryBits[tableAccessed]) - 1;
          uint64_t history = customGlobalHistory & historyMask;
          uint16_t taggedPredictorIndex = calculateIndexTaggedPredictor(pc, history, customGlobalHistoryBits[tableAccessed]);
          int usefulBitSize = 1, counterSize = 3;
          uint16_t counter = taggedPredictors[tableAccessed][taggedPredictorIndex];
          uint8_t usefulBitMask = (1 << usefulBitSize) - 1;
          uint8_t usefulBits = counter && usefulBitMask;
          counter >>= usefulBitSize;
          uint8_t counterMask = (1 << counterSize) - 1;
          uint8_t counterBits = counter && counterMask;
          prediction = counterBits >> (counterSize - 1);
          if (prediction == outcome && altpred != outcome)
            usefulBits = 1;

          counter >>= counterSize;
          if (outcome == TAKEN && counterBits < SST)
          {
            counterBits++;
          }

          if (outcome == NOTTAKEN && counterBits > SSN)
          {
            counterBits--;
          }

          counter <<= counterSize;
          counter |= counterBits;
          counter <<= usefulBitSize;
          counter |= usefulBits;
          taggedPredictors[tableAccessed][taggedPredictorIndex] = counter;
        }

        // Misprediction
        if (prediction != outcome && tableAccessed < 3)
        {
          int unallocatedEntryFound = 0;
          for (int k = 0; k < 4; k++)
          {
            uint64_t historyMask = (1 << customGlobalHistoryBits[k]) - 1;
            uint64_t history = customGlobalHistory & historyMask;
            uint16_t index = calculateIndexTaggedPredictor(pc, history, customGlobalHistoryBits[k]);
            uint16_t taggedEntry = taggedPredictors[k][index];
            if ((taggedEntry & 1) == 0) // successful allocation
            {
              unallocatedEntryFound = 1;
              uint8_t computedTag = calculateCustomTag(pc, history, customGlobalHistoryBits[k]);
              uint16_t entry = computedTag;
              entry <<= 3;
              entry |= WWT;
              entry <<= 1;
              taggedPredictors[k][index] = entry;
              decrementUbitCounter();
              break;
            }
          }

          if (!unallocatedEntryFound)
            incrementUbitCounter();
        }

        customGlobalHistory = (customGlobalHistory << 1) | outcome;
        break;
      }
    default:
      break;
  }
}
