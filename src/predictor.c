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

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

// Utilities
uint8_t update_counter(uint8_t counter, int8_t increment)
{
  return min(max(counter + increment, 0), 3);
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

uint32_t get_mask(int number_of_ones)
{
  uint32_t mask = 0;
  for (int i = 0; i < number_of_ones; ++i)
  {
    mask = (mask << 1) | 1;
  }
  return mask;
}


//
//TODO: Add your own Branch Predictor data structures here
struct GSharePredictor
{
    int ghistoryBits;

    // Global history variable.
    uint32_t ghistory;

    // Global predictor.
    // Size: 2^ghistoryBits (each entry is 2 bits: 00: strongly not taken, 01: weakly not taken, 10: weakly taken, 11: strongly taken)
    uint8_t *globalPrediction;

};


void init_gshare_predictor(struct GSharePredictor *gsharePredictor, int ghistoryBits)
{
  // Initialize bits in the tournament predictor.
  gsharePredictor->ghistoryBits = ghistoryBits;
  gsharePredictor->ghistory = 0;

  // Initialize the global prediction table.
  uint32_t globalPredictionSize = (1 << ghistoryBits); // 2^ghistoryBits
  gsharePredictor->globalPrediction = (uint8_t *) malloc(globalPredictionSize * sizeof(uint8_t));
  for (int i = 0; i < globalPredictionSize; i++) { gsharePredictor->globalPrediction[i] = 1; }
  assert(gsharePredictor->globalPrediction != NULL);
}

void gc_gshare_predictor(struct GSharePredictor *gsharePredictor)
{
  free(gsharePredictor->globalPrediction);
}


uint8_t make_prediction_gshare_predictor_raw(struct GSharePredictor *gsharePredictor, uint32_t pc)
{
  uint32_t pcMask = get_mask(gsharePredictor->ghistoryBits);
  uint32_t pcLastGlobalHistoryBits = pc & pcMask; // pc % 2^ghistoryBits
  int globalHistory = gsharePredictor->ghistory & pcMask; // ghistory % 2^ghistoryBits
  int globalPredictionIndex = globalHistory ^ pcLastGlobalHistoryBits;

  // Get counter from global prediction.
  uint8_t globalPredictionCounter = gsharePredictor->globalPrediction[globalPredictionIndex];

  // Return last two bits of the counter.
  return globalPredictionCounter & 0b11;
}

uint8_t make_prediction_gshare_predictor(struct GSharePredictor *gsharePredictor, uint32_t pc)
{
  uint8_t rawPrediction = make_prediction_gshare_predictor_raw(gsharePredictor, pc);

  // Get the upper bit from the last 2 bits of the counter.
  return ((rawPrediction >> 1) & 1) == 1 ? TAKEN : NOTTAKEN;
}

void train_gshare_predictor(struct GSharePredictor *gsharePredictor, uint32_t pc, uint8_t outcome) {

  uint32_t pcMask = get_mask(gsharePredictor->ghistoryBits);
  uint32_t pcLastGlobalHistoryBits = pc & pcMask; // pc % 2^ghistoryBits
  int globalHistory = gsharePredictor->ghistory & pcMask; // ghistory % 2^ghistoryBits
  int globalPredictionIndex = globalHistory ^ pcLastGlobalHistoryBits;

  // Get counter from global prediction.
  uint8_t *globalPredictionCounterPtr = &gsharePredictor->globalPrediction[globalPredictionIndex];
  uint32_t* globalHistoryPtr = &gsharePredictor->ghistory;

  // Update counter.
  *globalPredictionCounterPtr = update_counter(*globalPredictionCounterPtr, outcome == TAKEN ? 1 : -1);
  // Update global history
  *globalHistoryPtr = ((*globalHistoryPtr << 1) | outcome) % (1 << gsharePredictor->ghistoryBits);

}


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


void init_tournament_predictor(struct TournamentPredictor *tournamentPredictor, int ghistoryBits, int lhistoryBits, int pcIndexBits)
{
  // Initialize bits in the tournament predictor.
  tournamentPredictor->ghistoryBits = ghistoryBits;
  tournamentPredictor->lhistoryBits = lhistoryBits;
  tournamentPredictor->pcIndexBits = pcIndexBits;
  tournamentPredictor->ghistory = 0;

  // Initialize the local history table.
  uint32_t localHistoryTableSize = (1 << pcIndexBits); // 2^pcIndexBits
  tournamentPredictor->localHistoryTable = (uint32_t *) malloc(localHistoryTableSize * sizeof(uint32_t));
  for (int i = 0; i < localHistoryTableSize; i++) { tournamentPredictor->localHistoryTable[i] = 0; }
  assert(tournamentPredictor->localHistoryTable != NULL);

  // Initialize the local prediction table.
  uint32_t localPredictionSize = (1 << lhistoryBits); // 2^lhistoryBits
  tournamentPredictor->localPrediction = (uint8_t *) malloc(localPredictionSize * sizeof(uint8_t));
  for (int i = 0; i < localPredictionSize; i++) { tournamentPredictor->localPrediction[i] = 1; }
  assert(tournamentPredictor->localPrediction != NULL);

  // Initialize the global prediction table.
  uint32_t globalPredictionSize = (1 << ghistoryBits); // 2^ghistoryBits
  tournamentPredictor->globalPrediction = (uint8_t *) malloc(globalPredictionSize * sizeof(uint8_t));
  for (int i = 0; i < globalPredictionSize; i++) { tournamentPredictor->globalPrediction[i] = 1; }
  assert(tournamentPredictor->globalPrediction != NULL);

  // Initialize the choice prediction table.
  uint32_t choicePredictionSize = (1 << ghistoryBits); // 2^ghistoryBits
  tournamentPredictor->choicePrediction = (uint8_t *) malloc(choicePredictionSize * sizeof(uint8_t));
  for (int i = 0; i < choicePredictionSize; i++) { tournamentPredictor->choicePrediction[i] = 1; }
  assert(tournamentPredictor->choicePrediction != NULL);
}

void gc_tournament_predictor(struct TournamentPredictor *tournamentPredictor)
{
  free(tournamentPredictor->localHistoryTable);
  free(tournamentPredictor->localPrediction);
  free(tournamentPredictor->globalPrediction);
  free(tournamentPredictor->choicePrediction);
}


uint8_t make_prediction_tournament_predictor_local_raw(struct TournamentPredictor *tournamentPredictor, uint32_t pc)
{
  // Get the local history at address `pc`.
  uint32_t pcMask = get_mask(tournamentPredictor->pcIndexBits);
  uint32_t localHistoryIndex = pc & pcMask; // pc % 2^pcIndexBits
  int localHistory = tournamentPredictor->localHistoryTable[localHistoryIndex];

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
      if (verbose != 0)
      {
        printf("Prediction using GLOBAL: %d\n", prediction);
      }
      break;
    case kTournamentPredictorLocalChoice:
      prediction = make_prediction_tournament_predictor_local(tournamentPredictor, pc);
      if (verbose != 0)
      {
        printf("Prediction using LOCAL: %d\n", prediction);
      }
      break;
    default:
      break;
  }
  return prediction;
}

void train_tournament_predictor(struct TournamentPredictor *tournamentPredictor, uint32_t pc, uint8_t outcome) {
  // Update choice predictor.
  uint8_t local_prediction = make_prediction_tournament_predictor_local(tournamentPredictor, pc);
  uint8_t global_prediction = make_prediction_tournament_predictor_global(tournamentPredictor);

  if (local_prediction != global_prediction)
  {
    int8_t increment = (local_prediction == outcome) ? 1 : -1;
    uint8_t* choicePrediction = &tournamentPredictor->choicePrediction[tournamentPredictor->ghistory];
    *choicePrediction = update_counter(*choicePrediction, increment);
    if (verbose != 0)
    {
      printf("Choice predictor updated to: %d\n", *choicePrediction);
    }
  }

  // Update local predictor.
  // printf("Updating local predictor\n");

  // Get the local history at address `pc`.
  uint32_t localHistoryIndex = pc % (1 << tournamentPredictor->pcIndexBits); // pc % 2^pcIndexBits
  uint32_t *localHistory = &tournamentPredictor->localHistoryTable[localHistoryIndex];
  // Update local prediction.
  // Get counter from local prediction.
  uint8_t *localPredictionCounter = &tournamentPredictor->localPrediction[*localHistory];
  if (verbose != 0)
  {
    printf("Local prediction counter [before]: %d\n", *localPredictionCounter);
  }
  // Update counter.
  *localPredictionCounter = update_counter(*localPredictionCounter, outcome == TAKEN ? 1 : -1);
  // Update local history.
  *localHistory = ((*localHistory << 1) | outcome) % (1 << tournamentPredictor->lhistoryBits);

  // Update global predictor.
  // Get the global history.
  uint32_t* globalHistory = &tournamentPredictor->ghistory;
  // Get counter from global prediction.
  uint8_t* globalPredictionCounter = &tournamentPredictor->globalPrediction[*globalHistory];
  if (verbose != 0)
  {
    printf("Global prediction counter [before]: %d\n", *globalPredictionCounter);
  }
  // Update counter.
  *globalPredictionCounter = update_counter(*globalPredictionCounter, outcome == TAKEN ? 1 : -1);
  // Update global history
  *globalHistory = ((*globalHistory << 1) | outcome) % (1 << tournamentPredictor->ghistoryBits);

  if (verbose != 0)
  {
    printf("Local prediction counter updated to: %d\n", *localPredictionCounter);
    printf("Local history at pc = %d updated to: %d\n", pc, *localHistory);
    printf("Global prediction counter updated to: %d\n", *globalPredictionCounter);
    printf("Global history updated to: %d\n", *globalHistory);
    // printf("Updated global history: ");
    // print_all_the_bits_after_consecutive_zeros(tournamentPredictor->ghistory);
  }
}
//

struct CustomPredictor {
  int32_t** perceptrons;
  uint64_t ghistory;
};

void init_custom_predictor(struct CustomPredictor *customPredictor)
{
  // Setting ghistory.
  customPredictor->ghistory = 0;
  ghistoryBits = CUSTOM_GHISTORY_BITS;
  pcIndexBits = CUSTOM_PC_INDEX_BITS;

  uint32_t nPerceptrons = (1 << pcIndexBits);
  uint32_t nWeights = ghistoryBits;

  // Setting up perceptrons.
  customPredictor->perceptrons = (int32_t**) malloc(sizeof(int32_t*) * nPerceptrons);
  for (int i = 0; i < nPerceptrons; ++i)
  {
    customPredictor->perceptrons[i] = (int32_t*) malloc(sizeof(int32_t) * (nWeights + 1));
    for (int j = 0; j < nWeights + 1; j++)
    {
      customPredictor->perceptrons[i][j] = 0;
    }
  }

  int perceptronsMemory = nPerceptrons * (nWeights + 1) * (CUSTOM_WEIGHTS_BITS + 1);
  printf("Size of the predictor is %d bits + %lu\n", perceptronsMemory, sizeof(uint64_t) * 4);
}

int32_t make_prediction_custom_predictor_raw(struct CustomPredictor *customPredictor, uint32_t pc)
{
  uint32_t nPerceptrons = (1 << pcIndexBits);
  uint32_t nWeights = ghistoryBits;

  // Get the global history.
  uint64_t globalHistory = customPredictor->ghistory;

  // Get the perceptron index.
  uint32_t perceptronIndex = pc % nPerceptrons;

  // Get the perceptron weights.
  int32_t* weights = customPredictor->perceptrons[perceptronIndex];

  // Get the perceptron output.
  int32_t output = 0;
  output += weights[0];

  for (int i = 0; i < nWeights; ++i)
  {
    output += weights[i + 1] * ((globalHistory >> i) & 1 ? 1 : -1);
  }

  return output;
}

uint8_t make_prediction_custom_predictor(struct CustomPredictor *customPredictor, uint32_t pc)
{
  int32_t rawOutput = make_prediction_custom_predictor_raw(customPredictor, pc);

  // Get the prediction.
  uint8_t prediction = rawOutput >= 0 ? TAKEN : NOTTAKEN;

  return prediction;
}

int32_t abs(int32_t x)
{
  return x < 0 ? -x : x;
}

void train_custom_predictor(struct CustomPredictor *customPredictor, uint32_t pc, uint8_t outcome)
{
  uint32_t nPerceptrons = (1 << pcIndexBits);
  uint32_t nWeights = ghistoryBits;
  int32_t trainingThreshold = 1 << CUSTOM_TRAINING_THRESHOLD_BITS;
  int32_t absMaxWeights = 1 << (CUSTOM_WEIGHTS_BITS - 1);

  // Get the prediction.
  int32_t predictionRaw = make_prediction_custom_predictor_raw(customPredictor, pc);

  // Get the perceptron weights.
  int32_t* weights = customPredictor->perceptrons[pc % nPerceptrons];

  // Update the perceptron weights.
  if ((predictionRaw < 0 && outcome == TAKEN || predictionRaw >= 0 && outcome == NOTTAKEN) || abs(predictionRaw) < trainingThreshold)
  {
    int8_t outcomeMultiplier = outcome == TAKEN ? 1 : -1;
    weights[0] += outcomeMultiplier;
    for (int i = 0; i < nWeights; ++i)
    {
      weights[i + 1] += ((customPredictor->ghistory >> i) & 1 ? 1 : -1) * outcomeMultiplier;
      weights[i + 1] = max(min(weights[i + 1], (absMaxWeights - 1)), -absMaxWeights);
    }
  }

  // Update the global history.
  customPredictor->ghistory = ((customPredictor->ghistory << 1) | outcome) % ((uint64_t) 1 << ghistoryBits);
}

struct GSharePredictor gsharePredictor;
struct TournamentPredictor tournamentPredictor;
struct CustomPredictor customPredictor;

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
    init_gshare_predictor(&gsharePredictor, ghistoryBits);
      break;
    case TOURNAMENT:
      init_tournament_predictor(&tournamentPredictor, ghistoryBits, lhistoryBits, pcIndexBits);
      break;
    case CUSTOM:
      init_custom_predictor(&customPredictor);
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
      return make_prediction_gshare_predictor(&gsharePredictor, pc);
    case TOURNAMENT:
      return make_prediction_tournament_predictor(&tournamentPredictor, pc);
    case CUSTOM:
      return make_prediction_custom_predictor(&customPredictor, pc);
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
      train_gshare_predictor(&gsharePredictor, pc, outcome);
      break;
    case TOURNAMENT:
      train_tournament_predictor(&tournamentPredictor, pc, outcome);
      break;
    case CUSTOM:
      train_custom_predictor(&customPredictor, pc, outcome);
      break;
    default:
      break;
  }
}
