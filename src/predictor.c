//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"

//
// TODO:Student Information
//
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

//
//TODO: Add your own Branch Predictor data structures here
//

// GShare Data Structures
uint8_t* globalHistoryTable; // Global History Table for GShare
uint8_t globalHistoryTableEntries;
uint8_t globalHistory;      // Store the History
uint8_t indexMask;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  //GShare Initialization
  switch (bpType) {
    case STATIC:
      return;
    case GSHARE:

      // init gShare
      globalHistoryTableEntries = 1 << ghistoryBits;
      indexMask = globalHistoryTableEntries - 1;
      globalHistoryTable = (uint8_t*)malloc(globalHistoryTableEntries * sizeof(uint8_t));

      for (uint8_t i = 0; i < globalHistoryTableEntries; i++) {
        globalHistoryTable[i] = WN; 
        
      }
      
      globalHistory = 0;
      return;

    case TOURNAMENT:
    case CUSTOM:
    default:
      break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
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
      return;
    }
    break;
  case TOURNAMENT:
  case CUSTOM:
  default:
    break;
  }
}