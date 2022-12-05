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
int basePredictorIndexSize;
uint16_t basePredictorIndexMask;
uint16_t** taggedPredictors; // 4 tagged predictors
int taggedPredictorIndexSize;
uint16_t taggedPredictorIndexMask;
int tagSize;
uint8_t tagMask;
int usefulBitSize;
uint8_t usefulBitMask;
int taggedCounterSize;
uint8_t taggedCounterBitMask;
int tableAccessed;
int customGlobalHistoryBits[] = {1, 5, 10, 20};
uint64_t customGlobalHistoryMasks[4];
uint8_t altpred;
int useAltOnNa;
int uBitCounter;

void resetUbitsInPredictor()
{
  int taggedPredictorSize = 1 << taggedPredictorIndexSize;
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
  uint16_t taggedPredictorIndex = (uint16_t)(taggedPredictorIndexMask & pc);
  //uint32_t _pc = pc;
  /*for (int i = 0; i < 32; i += taggedPredictorIndexSize)
  {
    taggedPredictorIndex ^= (uint16_t)(taggedPredictorIndexMask & _pc);
    _pc >>= taggedPredictorIndexSize;
  }*/
  /*uint32_t _pc = pc >> taggedPredictorIndexSize;
  index ^= (_pc & indexMask);
  _pc >>= taggedPredictorIndexSize;
  index ^= (_pc & indexMask);
  _pc >>= taggedPredictorIndexSize;
  index ^= _pc;*/
  //uint64_t _history = history;
  for (int j = 0; j < globalHistoryBits; j += taggedPredictorIndexSize)
  {
    taggedPredictorIndex ^= (uint16_t)(taggedPredictorIndexMask & history);
    history >>= taggedPredictorIndexSize;
  }

  return taggedPredictorIndex;
}

uint8_t calculateCustomTag(uint32_t pc, uint64_t history, int globalHistoryBits)
{
  int csr2Size = 7;
  uint8_t tagMask1 = tagMask, tagMask2 = (1 << csr2Size) - 1;
  uint8_t csr1 = 0;
  for (int k = 0; k < globalHistoryBits; k += tagSize)
  {
    csr1 ^= (history & tagMask1);
    history >>= tagSize;
  }

  uint8_t csr2 = 0;
  for (int l = 0; l < globalHistoryBits; l += csr2Size)
  {
    csr2 ^= (history & tagMask2);
    history >>= csr2Size;
  }

  return csr1 ^ (csr2 << 1) ^ (uint8_t)(tagMask1 & pc);
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
        //printf("CUSTOM INITIALIZE\n");
        int basePredictorSize = 1 << 12;
        basePredictor = (uint8_t*)calloc(basePredictorSize, sizeof(uint8_t));
        basePredictorIndexSize = 12;
        basePredictorIndexMask = (1 << basePredictorIndexSize) - 1;
        //printf("basePredictorIndexMask: %u\n", basePredictorIndexMask);

        taggedPredictorIndexSize = 10;
        int taggedPredictorSize = 1 << taggedPredictorIndexSize;
        taggedPredictors = (uint16_t**)malloc(4 * sizeof(uint16_t*));
        for (int i = 0; i < 4; i++)
        {
          taggedPredictors[i] = (uint16_t*)calloc(taggedPredictorSize, sizeof(uint16_t));
          if (customGlobalHistoryBits[i] != 64)
            customGlobalHistoryMasks[i] = ((uint64_t)1 << customGlobalHistoryBits[i]) - 1;
          else
          {
            customGlobalHistoryMasks[i] = 0;
            customGlobalHistoryMasks[i] = ~customGlobalHistoryMasks[i];
          }

          //printf("customGlobalHistoryMasks[%u] = %lu\n", i, customGlobalHistoryMasks[i]);
        }

        taggedPredictorIndexMask = taggedPredictorSize - 1;
        //printf("taggedPredictorIndexMask: %u\n", taggedPredictorIndexMask);
        tagSize = 8;
        tagMask = 0;
        tagMask = ~tagMask;
        //printf("tagMask: %u\n", tagMask);
        taggedCounterSize = 3;
        taggedCounterBitMask = (1 << taggedCounterSize) - 1;
        //printf("taggedCounterBitMask: %u\n", taggedCounterBitMask);
        usefulBitSize = 1;
        usefulBitMask = (1 << usefulBitSize) - 1;
        //printf("usefulBitMask: %u\n", usefulBitMask);
        useAltOnNa = 0;
        uBitCounter = 0;

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
      uint16_t basePredictorIndex = (uint16_t)(basePredictorIndexMask & pc);
      //printf("##########################\n");
      //printf("pc: %u\n", pc);
      //printf("basePredictorIndex: %u\n", basePredictorIndex);
      uint8_t basePrediction = basePredictor[basePredictorIndex] & (1 << 1);
      altpred = basePrediction;
      //printf("basePrediction: %u: altpred\n", basePrediction);
      uint8_t taggedCounter, taggedPrediction;
      for (int i = 0; i < 4; i++)
      {
        //printf("##########################\n");
        //printf("i: %d\n", i);
        uint64_t history = customGlobalHistory & customGlobalHistoryMasks[i];
        //printf("history: %lu\n", history);

        // Calculate tag
        uint8_t computedTag = calculateCustomTag(pc, history, customGlobalHistoryBits[i]);
        //printf("computedTag: %u\n", computedTag);

        // Calculate index
        uint16_t index = calculateIndexTaggedPredictor(pc, history, customGlobalHistoryBits[i]);
        //printf("index: %u\n", index);

        // Get prediction
        uint16_t counter = taggedPredictors[i][index];
        //printf("counter: %u\n", counter);

        uint16_t _counter = counter;
        uint8_t usefulBits = (uint8_t)(usefulBitMask & _counter);
        //printf("usefulBits: %u\n", usefulBits);
        _counter >>= usefulBitSize;
        uint8_t counterBits = (uint8_t)(taggedCounterBitMask & _counter);
        //printf("counterBits: %u\n", counterBits);
        _counter >>= taggedCounterSize;
        uint8_t tagBits = (uint8_t)(tagMask & _counter);
        //printf("tagBits: %u\n", tagBits);
        if (tagBits == computedTag)
        {
          //printf("TAG MATCH\n");
          if (tableAccessed != -1)
          {
            altpred = taggedPrediction;
            //printf("altpred: %u\n", altpred);
          }

          taggedCounter = counterBits;
          taggedPrediction = taggedCounter >> (taggedCounterSize - 1);
          tableAccessed = i;
          //printf("tableAccessed: %d\n", tableAccessed);
        }
      }

      if (tableAccessed != -1)
      {
        if ((taggedCounter >= SWN && taggedCounter <= SWT) || useAltOnNa < 0)
        {
          //printf("decrement alt counter\n");
          decrementAltCounter();
          //printf("useAltonNa: %d\n", useAltOnNa);
          return taggedPrediction;
        }
        
        //printf("increment alt counter\n");
        incrementAltCounter();
        //printf("useAltonNa: %d\n", useAltOnNa);
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
        //printf("########################\n");
        //printf("Table: %d\n", tableAccessed);
        if (tableAccessed == -1)
        {
          uint16_t basePredictorIndex = (uint16_t)(basePredictorIndexMask & pc);
          //printf("basePredictorIndex: %u\n", basePredictorIndex);
          uint8_t predictorState = basePredictor[basePredictorIndex];
          //printf("predictorState: %u\n", predictorState);
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
          //printf("new value of basePredictor[%u]: %u\n", basePredictorIndex, basePredictor[basePredictorIndex]);
        }
        else
        {
          uint64_t history = customGlobalHistory & customGlobalHistoryMasks[tableAccessed];
          //printf("history: %lu\n", history);
          uint16_t taggedPredictorIndex = calculateIndexTaggedPredictor(pc, history, customGlobalHistoryBits[tableAccessed]);
          //printf("taggedPredictorIndex: %u\n", taggedPredictorIndex);
          uint16_t counter = taggedPredictors[tableAccessed][taggedPredictorIndex];
          //printf("counter: %u\n", counter);
          uint8_t usefulBits = (uint8_t)(usefulBitMask & counter);
          //printf("usefulBits: %u\n", usefulBits);
          counter >>= usefulBitSize;
          uint8_t counterBits = (uint8_t)(taggedCounterBitMask & counter);
          //printf("counterBits: %u\n", counterBits);
          prediction = counterBits >> (taggedCounterSize - 1);
          //printf("prediction: %u\n", prediction);
          if (prediction == outcome && altpred != outcome)
            usefulBits = 1;

          counter >>= taggedCounterSize;
          if (outcome == TAKEN && counterBits < SST)
          {
            counterBits++;
          }

          if (outcome == NOTTAKEN && counterBits > SSN)
          {
            counterBits--;
          }

          counter <<= taggedCounterSize;
          //printf("new value of counterBits: %u\n", counterBits);
          counter |= counterBits;
          counter <<= usefulBitSize;
          //printf("new value of usefulBits: %u\n", usefulBits);
          counter |= usefulBits;
          taggedPredictors[tableAccessed][taggedPredictorIndex] = counter;
          //printf("new value of taggedPredictors[%d][%u]: %u\n", tableAccessed, taggedPredictorIndex, taggedPredictors[tableAccessed][taggedPredictorIndex]);
        }

        // Misprediction
        if (prediction != outcome && tableAccessed < 3)
        {
          //printf("Misprediction\n");
          int unallocatedEntryFound = 0;
          for (int k = tableAccessed + 1; k < 4; k++)
          {
            uint64_t history = customGlobalHistory & customGlobalHistoryMasks[k];
            //printf("history: %lu\n", history);
            uint16_t index = calculateIndexTaggedPredictor(pc, history, customGlobalHistoryBits[k]);
            //printf("index: %u\n", index);
            uint16_t taggedEntry = taggedPredictors[k][index];
            //printf("Entry pre (taggedPredictors[%d][%u]): %d\n", k, index, taggedEntry);
            if ((taggedEntry & 1) == 0) // successful allocation
            {
              //printf("Successful allocation\n");
              unallocatedEntryFound = 1;
              uint8_t computedTag = calculateCustomTag(pc, history, customGlobalHistoryBits[k]);
              //printf("computedTag: %u\n", computedTag);
              uint16_t entry = (uint16_t)computedTag;
              entry <<= 3;
              entry |= outcome == TAKEN ? WWT : WWN;
              entry <<= 1;
              taggedPredictors[k][index] = entry;
              //printf("Entry post (taggedPredictors[%d][%u]): %d\n", k, index, taggedPredictors[k][index]);
              //printf("decrement uBit counter\n");
              decrementUbitCounter();
              //printf("uBitCounter: %d\n", uBitCounter);
              break;
            }
          }

          if (!unallocatedEntryFound)
          {
            //printf("increment uBit counter\n");
            incrementUbitCounter();
            //printf("uBitCounter: %d\n", uBitCounter);
          }
        }

        customGlobalHistory = (customGlobalHistory << 1) | outcome;
        //printf("New history: %ld\n", customGlobalHistory);
        break;
      }
    default:
      break;
  }
}
