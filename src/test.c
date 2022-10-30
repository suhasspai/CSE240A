//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Suhas Pai";
const char *studentID   = "A59001665";
const char *email       = "sspai@ucsd.edu";

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
uint32_t globalHistory;      // Store the History


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  //
  //TODO: Initialize Branch Predictor Data Structures
  //

  //GShare Initialization
  switch (bpType) {
    case STATIC:
      return;
    case GSHARE:
      uint8_t globalHistoryTableEntries = (uint8_t) pow(2, ghistoryBits); // Global History Table will have 2^(number of history bits) entries
      globalHistoryTable = (uint8_t*)malloc(globalHistoryTableEntries * sizeof(uint8_t));

      for (uint8_t i = 0; i < globalHistoryTableEntries; i++) {
        globalHistoryTable[i] = SN; // initialize to strong not-taken
      }
      // If we start the program here, our history is nothing. Initialize globalHistory to 0
      globalHistory = 0;
      break;
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
  //
  //TODO: Implement prediction scheme
  //

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      uint32_t xorResult = pc ^ globalHistory;

      if (globalHistoryTable[xorResult] == WT || globalHistoryTable[xorResult] == ST){
        return TAKEN;
      } else {
        return NOTTAKEN;
      }

      // for (int i = ghistoryBits - 1; i >= 0; i--)
      // {
      //   uint8_t historyBit = globalHistoryTable[i];
      //   xor <<= 1;
      //   xor += (historyBit ^ (pc % 2)); 
      //   pc >>= 1;
      // }

      // Why are you doing pc % 2 suhas, I still don't get it.

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
  //
  //TODO: Implement Predictor training
  //

  switch (bpType) {
  case STATIC:
    break;
  case GSHARE:
    uint8_t prediction = make_prediction(pc);
    if ((outcome == TAKEN) && ((prediction == ST || prediction == WT))) {
      // set index of that entry in globalHistoryTable to ST
      // how to do that?
    }
  case TOURNAMENT:
  case CUSTOM:
  default:
    break;
  }
  
  // Shift global history bits to left and append outcome
  
}