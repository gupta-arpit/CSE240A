//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"
#include <string.h>
#include <assert.h>

const char *studentName = "Arpit Gupta";
const char *studentID   = "A59010899";
const char *email       = "arg005@ucsd.edu";

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

// Utilities
uint8_t update_counter(uint8_t counter, int8_t increment)
{
  return (counter + increment) & 0b11;
}

void print_all_the_bits_after_consecutive_zeros(uint32_t num)
{
  int i = 0;
  while (num != 0)
  {
    printf("%d ", (num & 1));
    num >>= 1;
    i++;
  }
  printf("\n");
}

//
//TODO: Add your own Branch Predictor data structures here
struct TournamentPredictor
{
    int ghistoryBits;
    int lhistoryBits;
    int pcIndexBits;

    // Global history variable.
    uint32_t ghistory;

    // Local history table.
    // Size: 2^pcIndexBits (each entry is lhistoryBits bits)
    uint32_t *localHistoryTable;

    // Local predictor.
    // Size: 2^pcIndexBits (each entry is 2 bits: 00: strongly not taken, 01: weakly not taken, 10: weakly taken, 11: strongly taken)
    uint8_t *localPrediction;

    // Global predictor.
    // Size: 2^ghistoryBits (each entry is 2 bits: 00: strongly not taken, 01: weakly not taken, 10: weakly taken, 11: strongly taken)
    uint8_t *globalPrediction;

    // Choice predictor.
    // Size: 2^ghistoryBits (each entry is 2 bits: 00: strongly global, 01: weakly global, 10: weakly local, 11: strongly local)
    uint8_t *choicePrediction;
};

// Choice constants.
const uint8_t kTournamentPredictorGlobalChoice = 0;
const uint8_t kTournamentPredictorLocalChoice = 1;


void init_tournament_predictor(struct TournamentPredictor *tournamentPredictor, int ghistoryBits, int lhistoryBits, int pcIndexBits)
{
  // Initialize bits in the tournament predictor.
  tournamentPredictor->ghistoryBits = ghistoryBits;
  tournamentPredictor->lhistoryBits = lhistoryBits;
  tournamentPredictor->pcIndexBits = pcIndexBits;

  // Initialize the local history table.
  uint32_t localHistoryTableSize = (1 << pcIndexBits); // 2^pcIndexBits
  tournamentPredictor->localHistoryTable = (uint32_t *) malloc(localHistoryTableSize * sizeof(uint32_t));
  for (int i = 0; i < localHistoryTableSize; i++) { tournamentPredictor->localHistoryTable[i] = 0; }
  assert(tournamentPredictor->localHistoryTable != NULL);

  // Initialize the local prediction table.
  uint8_t localPredictionSize = (1 << lhistoryBits); // 2^lhistoryBits
  tournamentPredictor->localPrediction = (uint8_t *) malloc(localPredictionSize * sizeof(uint8_t));
  for (int i = 0; i < localPredictionSize; i++) { tournamentPredictor->localPrediction[i] = 1; }
  assert(tournamentPredictor->localPrediction != NULL);

  // Initialize the global prediction table.
  uint8_t globalPredictionSize = (1 << ghistoryBits); // 2^ghistoryBits
  tournamentPredictor->globalPrediction = (uint8_t *) malloc(globalPredictionSize * sizeof(uint8_t));
  for (int i = 0; i < globalPredictionSize; i++) { tournamentPredictor->globalPrediction[i] = 1; }
  assert(tournamentPredictor->globalPrediction != NULL);

  // Initialize the choice prediction table.
  uint8_t choicePredictionSize = (1 << ghistoryBits); // 2^ghistoryBits
  tournamentPredictor->choicePrediction = (uint8_t *) malloc(choicePredictionSize * sizeof(uint8_t));
  for (int i = 0; i < choicePredictionSize; i++) { tournamentPredictor->choicePrediction[i] = 0; }
  assert(tournamentPredictor->choicePrediction != NULL);
}

void gc_tournament_predictor(struct TournamentPredictor *tournamentPredictor)
{
  free(tournamentPredictor->localHistoryTable);
  free(tournamentPredictor->localPrediction);
  free(tournamentPredictor->globalPrediction);
  free(tournamentPredictor->choicePrediction);
}

uint32_t get_mask(int number_of_ones)
{
  uint32_t mask = 0;
  for (int i = 0; i < number_of_ones; ++i)
  {
    mask = (mask << 1) | 1;
  }
  return mask;
}

uint8_t make_prediction_tournament_predictor_local_raw(struct TournamentPredictor *tournamentPredictor, uint32_t pc)
{
  // Get the local history at address `pc`.
  uint32_t pcMask = get_mask(tournamentPredictor->pcIndexBits);
  uint32_t localHistoryIndex = pc & pcMask; // pc % 2^pcIndexBits
  // printf("localHistoryIndex: %d\n", localHistoryIndex);
  // printf("localHistoryTable[0]: %d\n", tournamentPredictor->localHistoryTable[0]);
  // printf("localHistoryTable[1]: %d\n", tournamentPredictor->localHistoryTable[1]);
  int localHistory = tournamentPredictor->localHistoryTable[localHistoryIndex];
  // printf("localhistory value = %d\n", localHistory);

  // Get counter from local prediction.
  uint8_t localPredictionCounter = tournamentPredictor->localPrediction[localHistory];

  // Return last two bits of the counter.
  return localPredictionCounter & 0b11;
}

uint8_t make_prediction_tournament_predictor_local(struct TournamentPredictor *tournamentPredictor, uint32_t pc)
{
  uint8_t rawPrediction = make_prediction_tournament_predictor_local_raw(tournamentPredictor, pc);

  // Get the upper bit from the last 2 bits of the counter.
  return ((rawPrediction >> 1) & 1) == 1 ? TAKEN : NOTTAKEN;
}

uint8_t make_prediction_tournament_predictor_global_raw(struct TournamentPredictor *tournamentPredictor)
{
  // Get the global history.
  uint32_t globalHistory = tournamentPredictor->ghistory;

  // Get counter from global prediction.
  uint8_t globalPredictionCounter = tournamentPredictor->globalPrediction[globalHistory];

  // Return last two bits of the counter.
  return globalPredictionCounter & 0b11;
}

uint8_t make_prediction_tournament_predictor_global(struct TournamentPredictor *tournamentPredictor)
{
  uint8_t rawPrediction = make_prediction_tournament_predictor_global_raw(tournamentPredictor);

  // Get the upper bit from the last 2 bits of the counter.
  return ((rawPrediction >> 1) & 1) == 1 ? TAKEN : NOTTAKEN;
}

uint8_t get_choice_tournament_predictor(struct TournamentPredictor *tournamentPredictor)
{
  // Get the global history.
  uint32_t globalHistory = tournamentPredictor->ghistory;

  // Get counter from choice prediction.
  uint8_t choicePredictionCounter = tournamentPredictor->choicePrediction[globalHistory];

  // Get the upper bit from the last 2 bits of the counter.
  uint8_t choice = (choicePredictionCounter >> 1) & 1;

  return choice ? kTournamentPredictorLocalChoice : kTournamentPredictorGlobalChoice;
}

uint8_t make_prediction_tournament_predictor(struct TournamentPredictor *tournamentPredictor, uint32_t pc)
{
  uint8_t prediction = 0;
  uint8_t choice = get_choice_tournament_predictor(tournamentPredictor);

  switch(choice)
  {
    case kTournamentPredictorGlobalChoice:
      prediction = make_prediction_tournament_predictor_global(tournamentPredictor);
      break;
    case kTournamentPredictorLocalChoice:
      prediction = make_prediction_tournament_predictor_local(tournamentPredictor, pc);
      break;
    default:
      break;
  }
  return prediction;
}

void train_tournament_predictor(struct TournamentPredictor *tournamentPredictor, uint32_t pc, uint8_t outcome) {
  // Update choice predictor.
  uint8_t local_prediction_raw = make_prediction_tournament_predictor_local_raw(tournamentPredictor, pc);
  uint8_t local_prediction = (local_prediction_raw >> 1) & 1;

  uint8_t global_prediction_raw = make_prediction_tournament_predictor_global_raw(tournamentPredictor);
  uint8_t global_prediction = (global_prediction_raw >> 1) & 1;

  if (local_prediction != global_prediction)
  {
    int8_t increment = (local_prediction == outcome) ? 1 : -1;
    uint8_t* choicePrediction = &tournamentPredictor->choicePrediction[tournamentPredictor->ghistory];
    *choicePrediction = update_counter(*choicePrediction, increment);
  }

  // Update local predictor.
  // printf("Updating local predictor\n");

  // Get the local history at address `pc`.
  uint32_t localHistoryIndex = pc % (1 << tournamentPredictor->pcIndexBits); // pc % 2^pcIndexBits
  uint32_t *localHistory = &tournamentPredictor->localHistoryTable[localHistoryIndex];
  // Update local prediction.
  // Get counter from local prediction.
  uint8_t *localPredictionCounter = &tournamentPredictor->localPrediction[*localHistory];
  // Update counter.
  *localPredictionCounter = update_counter(*localPredictionCounter, outcome == TAKEN ? 1 : -1);
  // Update local history.
  *localHistory = ((*localHistory << 1) | outcome) % (1 << tournamentPredictor->lhistoryBits);

  if (verbose)
  {
    printf("Updated local history: ");
    print_all_the_bits_after_consecutive_zeros(tournamentPredictor->localHistoryTable[localHistoryIndex]);
  }

  // Update global predictor.
  // Get the global history.
  uint32_t* globalHistory = &tournamentPredictor->ghistory;
  // Get counter from global prediction.
  uint8_t* globalPredictionCounter = &tournamentPredictor->globalPrediction[*globalHistory];
  // Update counter.
  *globalPredictionCounter = update_counter(*globalPredictionCounter, outcome == TAKEN ? 1 : -1);
  // Update global history
  *globalHistory = ((*globalHistory << 1) | outcome) % (1 << tournamentPredictor->ghistoryBits);

  if (verbose)
  {
    printf("Updated global history: ");
    print_all_the_bits_after_consecutive_zeros(tournamentPredictor->ghistory);
  }
}
//

struct TournamentPredictor tournamentPredictor;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  switch(bpType) {
    case STATIC:
      break;
    case GSHARE:
      break;
    case TOURNAMENT:
      init_tournament_predictor(&tournamentPredictor, ghistoryBits, lhistoryBits, pcIndexBits);
      break;
    case CUSTOM:
      break;
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
    case TOURNAMENT:
      return make_prediction_tournament_predictor(&tournamentPredictor, pc);
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
  switch (bpType)
  {
    case STATIC:
      break;
    case GSHARE:
      break;
    case TOURNAMENT:
      train_tournament_predictor(&tournamentPredictor, pc, outcome);
      break;
    case CUSTOM:
      break;
    default:
      break;
  }
}
