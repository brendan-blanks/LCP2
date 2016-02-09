/* =============================================================== */
/* PROGRAM */

/*
    AUTHOR: Brendan Blanks
    FSU MAIL NAME: bfb09c@fsu.edu
    COP 4150 - FALL 2011
    PROJECT NUMBER: 2
    DUE DATE: Thursday 10/27/2011
*/

/* PROGRAM */
/* =============================================================== */
/* HEADER FILES */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

/* HEADER FILES */
/* =============================================================== */
/* CONSTANTS */   

#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define MULT 5
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

#define PREDICTSTRONGNO 0
#define PREDICTWEAKNO 1
#define PREDICTSTRONGYES 2
#define PREDICTWEAKYES 3
#define INITIALPREDICTION 1
#define NUMPREDICTIONS 4

#define COOLPROCESSOR 3

#define SIZEOFBTBUFFER 4

#define SIZEOFRENAMETABLE 8
#define NUMRESERVATIONSTATIONS 3
#define SIZEOFREORDERBUFFER 16

#define NUMMULTPIPES 6

/* CONSTANTS */
/* =============================================================== */
/* TYPE DEFINITIONS */

typedef struct IFRNStruct {
	int instr;
	int pcPlus1;
} IFRNType;

typedef struct RNALStruct {
	int instr;
	int pcPlus1;
	int regA;
	int Arenamed;
	int forwardA;
	int forwardedA;
	int regB;
	int Brenamed;
	int forwardB;
	int forwardedB;
	int destReg;
	int physReg;
	int offset;
} RNALType;

typedef struct FUADDStruct {
	int regA;
	int regB;
	int dest;
	int opcode;
} FUADDType;

typedef struct FUMULTPIPEStruct {
	int opcode;
	int regA;
	int regB;
	int dest;
} FUMULTPIPEType;

typedef struct FUMULTStruct {
	FUMULTPIPEType pipe[NUMMULTPIPES];
} FUMULTType;

typedef struct FUMEMStruct {
	int opcode;
	int regA;
	int regB;
	int offset;
	int dest;
	int timeLeft;
} FUMEMType;

typedef struct BTBStruct {
	int tag;
	int data;
} BTBType;

typedef struct RNTStruct {
	int physReg;
	int valid;
} RNTType;

typedef struct RSStruct {
	int src1;
	int src2;
	int dest;
	int value1;
	int valid1;
	int value2;
	int valid2;
	int offset;
	int opcode;
} RSType;

typedef struct ROBStruct {
	int pc;
	int destReg;
	int value;
	int opcode;
	int ready;
	int branchTarget;
} ROBType;
typedef struct stateStruct {
	int pc;
	int instrMem[NUMMEMORY];
	int dataMem[NUMMEMORY];
	int ARF[NUMREGS];
	int numMemory;
	IFRNType IFRNTop;
	IFRNType IFRNBottom;
	RNALType RNALTop;
	RNALType RNALBottom;
	FUADDType FUADD;
	FUMULTType FUMULT;
	FUMEMType FUMEM;
	BTBType BTB[SIZEOFBTBUFFER];
	RNTType RNT[SIZEOFRENAMETABLE];
	RSType RSadd[NUMRESERVATIONSTATIONS];
	RSType RSmult[NUMRESERVATIONSTATIONS];
	RSType RSmem[NUMRESERVATIONSTATIONS];
	ROBType ROB[SIZEOFREORDERBUFFER];
	int ROBhead;
	int ROBtail;
	int freedRS[3];
	int cycles; /* number of cycles run so far */
	int fetchedInstructions;
	int retiredInstructions;
	int takenBranches;
	int incorrectBranchPredictions;
	int predictor;
	int branching;
	int branched;
	int halting;
	int checkPoint;
	int haltStall;
	int stall;
} stateType;

/* TYPE DEFINITIONS */
/* =============================================================== */
/* FUNCTION PROTOTYPES */
 
void GetCode(FILE**, char*);
void Run(FILE**);
void Initialize(stateType*, FILE**);
void ClearForBranch(stateType*);
void PrintState(stateType*);
void Fetch(stateType*, const stateType*);
void Rename(stateType*, const stateType*);
void Allocate(stateType*, const stateType*);
void Schedule(stateType*, const stateType*);
void Execute(stateType*, const stateType*);
int RTypeInstruction(int, int, int);
int MultInstruction(int, int);
int ITypeInstruction(int, int);
void Commit(stateType*, const stateType*);
int Field0(int);
int Field1(int);
int Field2(int);
int Opcode(int);
void PrintInstruction(int);
void FinishUp(stateType*);

/* FUNCTION PROTOTYPES */
/* =============================================================== */
/* MAIN FUNCTION */

int main(int argc, char* argv[])
{
	FILE* file;
	if(argc != 2)
	{
		/* the user is a moron... */
		printf("Simulator requires a machine code file to run\n");
	}
	else
	{
		/* let's get going! */
		GetCode(&file, argv[1]);
		Run(&file);
	}
	return(0);
}

/* MAIN FUNCTION */
/* =============================================================== */
/* MODULES */

void GetCode(FILE** file, char* fileName)
{
	*file = fopen(fileName, "r");
	if(!(*file))
	{
		printf("Failed to open \"%s\". Aborting...\n", fileName);
		exit(0);
	}
	return;
}

void Run(FILE** file)
{
	/* create the machine */
	stateType state;
	stateType newState;
	/* build me a processor! */
	Initialize(&state, file);

	/* run simulation */
	while(1)
	{
		/* how we doin? */
		PrintState(&state);

		/* check for halt */
		if(state.halting)
		{
			/* we're done! */
			FinishUp(&state);
		}
		newState = state;
		newState.cycles++;

		Commit(&newState, &state);
		if(!newState.branched)
		{
			Execute(&newState, &state);
			Schedule(&newState, &state);
			Allocate(&newState, &state);
			if(!newState.stall)
			{
				Rename(&newState, &state);
				Fetch(&newState, &state);
			}
		}
		/* Don't stall TOO long... */
		newState.stall = 0;
		newState.branched = 0;

		state = newState; /* this is the last statement before
				the end of the loop. It marks the end
				of the cycle and updates the current 
				state with the values calculated in this 
				cycle */
	}
	/* never get here, but still! */
	return;
}

void Initialize(stateType *statePtr, FILE** file)
{
	statePtr->pc = 0;
	int* tempInt;
	int k;
	/* fill out the memory */
	for(k = 0; k < NUMMEMORY && !feof((*file)); k++)
	{
		tempInt = &(statePtr->instrMem[k]);
		fscanf((*file), "%d", tempInt);
		if(!feof((*file)))
			printf("memory[%d]=%d\n", k, statePtr->instrMem[k]);
	}
	/* account for garbage taken in at the end */
	if(statePtr->instrMem[k-1] == 0)
		k--;
	/* make sure it's in the data memory too... */
	for(int i = 0; i < k; i++)
	{
		statePtr->dataMem[i] = statePtr->instrMem[i];
	}
	/* start up the register file! */
	for(int i = 0; i < NUMREGS; i++)
		statePtr->ARF[i] = 0;
	
	statePtr->numMemory = k;
	/* how're we starting out? */
	printf("%d memory words\n", statePtr->numMemory);
	printf("\tinstruction memory:\n");
	for(int i = 0; i < k; i++)
	{
		char* opcode;
		if(Opcode(statePtr->instrMem[i]) == ADD)
			opcode = strdup("add");
		else if(Opcode(statePtr->instrMem[i]) == NAND)
			opcode = strdup("nand");
		else if(Opcode(statePtr->instrMem[i]) == LW)
			opcode = strdup("lw");
		else if(Opcode(statePtr->instrMem[i]) == SW)
			opcode = strdup("sw");
		else if(Opcode(statePtr->instrMem[i]) == BEQ)
			opcode = strdup("beq");
		else if(Opcode(statePtr->instrMem[i]) == MULT)
			opcode = strdup("mult");
		else if(Opcode(statePtr->instrMem[i]) == HALT)
			opcode = strdup("halt");
		else
			opcode = strdup("noop");
		printf("\t\tinstrMem[ %d ] %s %d %d %d\n", i, opcode, Field0(statePtr->instrMem[i]), Field1(statePtr->instrMem[i]), Field2(statePtr->instrMem[i]));
	}
	/* aaaand set everything to initial values */
	statePtr->IFRNTop.instr = NOOPINSTRUCTION;
	statePtr->IFRNTop.pcPlus1 = 0;
	statePtr->IFRNBottom.instr = NOOPINSTRUCTION;
	statePtr->IFRNBottom.pcPlus1 = 0;
	statePtr->RNALTop.instr = NOOPINSTRUCTION;
	statePtr->RNALTop.pcPlus1 = 0;
	statePtr->RNALTop.regA = 0;
	statePtr->RNALTop.Arenamed = 0;
	statePtr->RNALTop.forwardA = 0;
	statePtr->RNALTop.forwardedA = 0;
	statePtr->RNALTop.regB = 0;
	statePtr->RNALTop.Brenamed = 0;
	statePtr->RNALTop.forwardB = 0;
	statePtr->RNALTop.forwardedB = 0;
	statePtr->RNALTop.destReg = 0;
	statePtr->RNALTop.physReg = 0;
	statePtr->RNALTop.offset = 0;
	statePtr->RNALBottom.instr = NOOPINSTRUCTION;
	statePtr->RNALBottom.pcPlus1 = 0;
	statePtr->RNALBottom.regA = 0;
	statePtr->RNALBottom.Arenamed = 0;
	statePtr->RNALBottom.forwardA = 0;
	statePtr->RNALBottom.forwardedA = 0;
	statePtr->RNALBottom.regB = 0;
	statePtr->RNALBottom.Brenamed = 0;
	statePtr->RNALBottom.forwardB = 0;
	statePtr->RNALBottom.forwardedB = 0;
	statePtr->RNALBottom.destReg = 0;
	statePtr->RNALBottom.physReg = 0;
	statePtr->RNALBottom.offset = 0;
	statePtr->FUADD.regA = 0;
	statePtr->FUADD.regB = 0;
	statePtr->FUADD.dest = 0;
	statePtr->FUADD.opcode = NOOP;
	for(int i = 0; i < NUMMULTPIPES; i++)
	{
		statePtr->FUMULT.pipe[i].opcode = NOOP;
		statePtr->FUMULT.pipe[i].regA = 0;
		statePtr->FUMULT.pipe[i].regB = 0;
		statePtr->FUMULT.pipe[i].dest = 0;
	}
	statePtr->FUMEM.opcode = NOOP;
	statePtr->FUMEM.regA = 0;
	statePtr->FUMEM.regB = 0;
	statePtr->FUMEM.offset = 0;
	statePtr->FUMEM.dest = 0;
	statePtr->FUMEM.timeLeft = 0;
	for(int i = 0; i < SIZEOFBTBUFFER; i++)
	{
		statePtr->BTB[i].tag = 0;
		statePtr->BTB[i].data = 0;
	}
	for(int i = 0; i < SIZEOFRENAMETABLE; i++)
	{
		statePtr->RNT[i].physReg = 0;
		statePtr->RNT[i].valid = 0;
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		statePtr->RSadd[i].src1 = 0;
		statePtr->RSadd[i].src2 = 0;
		statePtr->RSadd[i].dest = 0;
		statePtr->RSadd[i].value1 = 0;
		statePtr->RSadd[i].valid1 = 0;
		statePtr->RSadd[i].value2 = 0;
		statePtr->RSadd[i].valid2 = 0;
		statePtr->RSadd[i].offset = 0;
		statePtr->RSadd[i].opcode = NOOP;
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		statePtr->RSmult[i].src1 = 0;
		statePtr->RSmult[i].src2 = 0;
		statePtr->RSmult[i].dest = 0;
		statePtr->RSmult[i].value1 = 0;
		statePtr->RSmult[i].valid1 = 0;
		statePtr->RSmult[i].value2 = 0;
		statePtr->RSmult[i].valid2 = 0;
		statePtr->RSmult[i].offset = 0;
		statePtr->RSmult[i].opcode = NOOP;
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		statePtr->RSmem[i].src1 = 0;
		statePtr->RSmem[i].src2 = 0;
		statePtr->RSmem[i].dest = 0;
		statePtr->RSmem[i].value1 = 0;
		statePtr->RSmem[i].valid1 = 0;
		statePtr->RSmem[i].value2 = 0;
		statePtr->RSmem[i].valid2 = 0;
		statePtr->RSmem[i].offset = 0;
		statePtr->RSmem[i].opcode = NOOP;
	}
	for(int i = 0; i < SIZEOFREORDERBUFFER; i++)
	{
		statePtr->ROB[i].destReg = 0;
		statePtr->ROB[i].value = 0;
		statePtr->ROB[i].opcode = NOOP;
		statePtr->ROB[i].ready = 0;
		statePtr->ROB[i].pc = 0;
		statePtr->ROB[i].branchTarget = 0;
	}
	statePtr->ROBhead = 0;
	statePtr->ROBtail = 0;
	statePtr->cycles = 0;
	statePtr->fetchedInstructions = 0;
	statePtr->retiredInstructions = 0;
	statePtr->takenBranches = 0;
	statePtr->incorrectBranchPredictions = 0;
	statePtr->predictor = INITIALPREDICTION;
	statePtr->branching = 0;
	statePtr->branched = 0;
	statePtr->halting = 0;
	statePtr->checkPoint = 0;
	statePtr->haltStall = 0;
	statePtr->stall = 0;
	/* processor ready! */
	return;
}

void ClearForBranch(stateType* statePtr)
{
	/* hmm...looks like we branched... */
	statePtr->IFRNTop.instr = NOOPINSTRUCTION;
	statePtr->IFRNTop.pcPlus1 = 0;
	statePtr->IFRNBottom.instr = NOOPINSTRUCTION;
	statePtr->IFRNBottom.pcPlus1 = 0;
	statePtr->RNALTop.instr = NOOPINSTRUCTION;
	statePtr->RNALTop.pcPlus1 = 0;
	statePtr->RNALTop.regA = 0;
	statePtr->RNALTop.Arenamed = 0;
	statePtr->RNALTop.forwardA = 0;
	statePtr->RNALTop.forwardedA = 0;
	statePtr->RNALTop.regB = 0;
	statePtr->RNALTop.Brenamed = 0;
	statePtr->RNALTop.forwardB = 0;
	statePtr->RNALTop.forwardB = 0;
	statePtr->RNALTop.destReg = 0;
	statePtr->RNALTop.physReg = 0;
	statePtr->RNALTop.offset = 0;
	statePtr->RNALBottom.instr = NOOPINSTRUCTION;
	statePtr->RNALBottom.pcPlus1 = 0;
	statePtr->RNALBottom.regA = 0;
	statePtr->RNALBottom.Arenamed = 0;
	statePtr->RNALBottom.forwardA = 0;
	statePtr->RNALBottom.forwardedA = 0;
	statePtr->RNALBottom.regB = 0;
	statePtr->RNALBottom.Brenamed = 0;
	statePtr->RNALBottom.forwardB = 0;
	statePtr->RNALBottom.forwardedB = 0;
	statePtr->RNALBottom.destReg = 0;
	statePtr->RNALBottom.physReg = 0;
	statePtr->RNALBottom.offset = 0;
	statePtr->FUADD.regA = 0;
	statePtr->FUADD.regB = 0;
	statePtr->FUADD.dest = 0;
	statePtr->FUADD.opcode = NOOP;
	for(int i = 0; i < NUMMULTPIPES; i++)
	{
		statePtr->FUMULT.pipe[i].opcode = NOOP;
		statePtr->FUMULT.pipe[i].regA = 0;
		statePtr->FUMULT.pipe[i].regB = 0;
		statePtr->FUMULT.pipe[i].dest = 0;
	}
	statePtr->FUMEM.opcode = NOOP;
	statePtr->FUMEM.regA = 0;
	statePtr->FUMEM.regB = 0;
	statePtr->FUMEM.offset = 0;
	statePtr->FUMEM.dest = 0;
	statePtr->FUMEM.timeLeft = 0;
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		statePtr->RSadd[i].src1 = 0;
		statePtr->RSadd[i].src2 = 0;
		statePtr->RSadd[i].dest = 0;
		statePtr->RSadd[i].value1 = 0;
		statePtr->RSadd[i].valid1 = 0;
		statePtr->RSadd[i].value2 = 0;
		statePtr->RSadd[i].valid2 = 0;
		statePtr->RSadd[i].offset = 0;
		statePtr->RSadd[i].opcode = NOOP;
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		statePtr->RSmult[i].src1 = 0;
		statePtr->RSmult[i].src2 = 0;
		statePtr->RSmult[i].dest = 0;
		statePtr->RSmult[i].value1 = 0;
		statePtr->RSmult[i].valid1 = 0;
		statePtr->RSmult[i].value2 = 0;
		statePtr->RSmult[i].valid2 = 0;
		statePtr->RSmult[i].offset = 0;
		statePtr->RSmult[i].opcode = NOOP;
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		statePtr->RSmem[i].src1 = 0;
		statePtr->RSmem[i].src2 = 0;
		statePtr->RSmem[i].dest = 0;
		statePtr->RSmem[i].value1 = 0;
		statePtr->RSmem[i].valid1 = 0;
		statePtr->RSmem[i].value2 = 0;
		statePtr->RSmem[i].valid2 = 0;
		statePtr->RSmem[i].offset = 0;
		statePtr->RSmem[i].opcode = NOOP;
	}
	statePtr->checkPoint = 0;
	statePtr->haltStall = 0;
	statePtr->branching = 0;
	statePtr->stall = 0;
	statePtr->ROBhead = statePtr->ROBtail;
	for(int i = 0; i < SIZEOFRENAMETABLE; i++)
	{
		statePtr->RNT[i].valid = 0;
	}
	/* all clean! */
	return;
}

void Fetch(stateType* newState, const stateType* state)
{
	if(!state->branching && !state->haltStall)
	{ 
		/* branch prediction stall not active */        
		newState->IFRNTop.instr = state->instrMem[state->pc];
		newState->pc++;
		newState->IFRNTop.pcPlus1 = state->pc + 1;
		newState->fetchedInstructions++;
		if(Opcode(state->instrMem[state->pc]) == HALT)
		{
			newState->checkPoint = state->fetchedInstructions + 1;
			newState->haltStall = 1;
		}
	}
	else
	{
		/* branch prediction stall active */
		newState->IFRNTop.instr = NOOPINSTRUCTION;
	}
	/* activate branch prediction stall? */
	if(Opcode(newState->IFRNTop.instr) == BEQ && (state->predictor == PREDICTSTRONGYES || state->branching == PREDICTWEAKYES))
		newState->branching = 1;
	if(newState->branching || newState->haltStall || (Opcode(newState->IFRNTop.instr) == BEQ && Opcode(state->instrMem[state->pc + 1]) == BEQ))
	{
		/* branch prediction stall active */
		/* or two consecutive branches */
		/* or stalling after halt */
		newState->IFRNBottom.instr = NOOPINSTRUCTION;
	}
	else
	{
		/* branch prediction stall not active */
		/* nor two consective branches */
		/* nor stalling after halt */
		newState->IFRNBottom.instr = state->instrMem[state->pc + 1];
		newState->pc++;
		newState->IFRNBottom.pcPlus1 = state->pc + 2;
		newState->fetchedInstructions++;
		if(Opcode(state->instrMem[state->pc + 1]) == HALT)
		{
			newState->checkPoint = state->fetchedInstructions + 2;
			newState->haltStall = 1;
		}
	}
	if(Opcode(newState->IFRNBottom.instr) == BEQ && (state->predictor == PREDICTSTRONGYES || state->branching == PREDICTWEAKYES))
		newState->branching = 1;

	return;
}

void Rename(stateType* newState, const stateType* state)
{
	newState->RNT[0].valid = 0;
	newState->RNT[0].physReg = 0;
	newState->RNALTop.instr = state->IFRNTop.instr;
	newState->RNALTop.pcPlus1 = state->IFRNTop.pcPlus1;
	newState->RNALTop.regA = Field0(state->IFRNTop.instr);
	newState->RNALTop.Arenamed = 0;
	newState->RNALTop.forwardedA = 0;
	if(state->RNT[newState->RNALTop.regA].valid)
	{
		newState->RNALTop.regA = state->RNT[newState->RNALTop.regA].physReg;
		newState->RNALTop.Arenamed = 1;
	}
	newState->RNALTop.regB = Field1(state->IFRNTop.instr);
	newState->RNALTop.Brenamed = 0;
	newState->RNALTop.forwardedB = 0;
	if(state->RNT[newState->RNALTop.regB].valid)
	{
		newState->RNALTop.regB = state->RNT[newState->RNALTop.regB].physReg;
		newState->RNALTop.Brenamed = 1;
	}
	newState->RNALTop.offset = Field2(state->IFRNTop.instr);
	if(Opcode(state->IFRNTop.instr) == ADD || Opcode(state->IFRNTop.instr) == NAND || Opcode(state->IFRNTop.instr) == MULT)
	{
		if(Field2(state->IFRNTop.instr) != 0)
		{
			newState->RNT[Field2(state->IFRNTop.instr)].physReg = newState->ROBtail;
			newState->RNT[Field2(state->IFRNTop.instr)].valid = 1;
		}
		newState->RNALTop.physReg = newState->ROBtail;
		newState->RNALTop.destReg = Field2(state->IFRNTop.instr);
	}
	else if(Opcode(state->IFRNTop.instr) == LW)
	{
		newState->RNT[Field1(state->IFRNTop.instr)].physReg = newState->ROBtail;
		newState->RNT[Field1(state->IFRNTop.instr)].valid = 1;
		newState->RNALTop.physReg = newState->ROBtail;
		newState->RNALTop.destReg = Field1(state->IFRNTop.instr);
	}
	else
	{
		newState->RNALTop.physReg = newState->ROBtail;
		newState->RNALTop.destReg = 0;
	}

	newState->RNALBottom.instr = state->IFRNBottom.instr;
	newState->RNALBottom.pcPlus1 = state->IFRNBottom.pcPlus1;
	newState->RNALBottom.regA = Field0(state->IFRNBottom.instr);
	newState->RNALBottom.Arenamed = 0;
	newState->RNALBottom.forwardedA = 0;
	if(newState->RNT[newState->RNALBottom.regA].valid)
	{
		newState->RNALBottom.regA = newState->RNT[newState->RNALBottom.regA].physReg;
		newState->RNALBottom.Arenamed = 1;
	}
	newState->RNALBottom.regB = Field1(state->IFRNBottom.instr);
	newState->RNALBottom.Brenamed = 0;
	newState->RNALBottom.forwardedB = 0;
	if(newState->RNT[newState->RNALBottom.regB].valid)
	{
		newState->RNALBottom.regB = newState->RNT[newState->RNALBottom.regB].physReg;
		newState->RNALBottom.Brenamed = 1;
	}
	newState->RNALBottom.offset = Field2(state->IFRNBottom.instr);
	if(Opcode(state->IFRNBottom.instr) == ADD || Opcode(state->IFRNBottom.instr) == NAND || Opcode(state->IFRNBottom.instr) == MULT)
	{
		if(Field2(state->IFRNBottom.instr) != 0)
		{
			newState->RNT[Field2(state->IFRNBottom.instr)].physReg = newState->ROBtail+1;
			newState->RNT[Field2(state->IFRNBottom.instr)].valid = 1;
		}
		newState->RNALBottom.physReg = newState->ROBtail+1;
		newState->RNALBottom.destReg = Field2(state->IFRNBottom.instr);
	}
	else if(Opcode(state->IFRNBottom.instr) == LW)
	{
		newState->RNT[Field1(state->IFRNBottom.instr)].physReg = newState->ROBtail+1;
		newState->RNT[Field1(state->IFRNBottom.instr)].valid = 1;
		newState->RNALBottom.physReg = newState->ROBtail + 1;
		newState->RNALBottom.destReg = Field1(state->IFRNBottom.instr);
	}
	else
	{
		newState->RNALBottom.physReg = newState->ROBtail + 1;
		newState->RNALBottom.destReg = 0;
	}

	return;
}

void Allocate(stateType* newState, const stateType* state)
{
	/* First the top.... */
	if(state->ROBhead == (state->ROBtail + 1) % SIZEOFREORDERBUFFER)
	{
		newState->stall = 1;
		return;
	}
	else if(Opcode(state->RNALTop.instr) == ADD || Opcode(state->RNALTop.instr) == NAND || Opcode(state->RNALTop.instr) == BEQ)
	{
		/* R-Type Instruction */
		int i;
		int found = 0;
		for(i = 0; i < NUMRESERVATIONSTATIONS && !found; i++)
		{
			if(newState->RSadd[i].opcode == NOOP)
			{
				newState->RSadd[i].src1 = state->RNALTop.regA;
				newState->RSadd[i].src2 = state->RNALTop.regB;
				newState->RSadd[i].dest = state->RNALTop.physReg;
				if(!newState->RNALTop.forwardedA)
					newState->RSadd[i].value1 = state->ARF[Field0(state->RNALTop.instr)];
				else
					newState->RSadd[i].value1 = newState->RNALTop.forwardA;
				newState->RSadd[i].valid1 = newState->RNALTop.Arenamed;
				if(!newState->RNALTop.forwardedB)
					newState->RSadd[i].value2 = state->ARF[Field1(state->RNALTop.instr)];
				else
					newState->RSadd[i].value2 = newState->RNALTop.forwardB;
				newState->RSadd[i].valid2 = newState->RNALTop.Brenamed;
				newState->RSadd[i].offset = Field2(state->RNALTop.instr);
				newState->RSadd[i].opcode = Opcode(state->RNALTop.instr);
				newState->ROB[state->RNALTop.physReg].pc = state->RNALTop.pcPlus1;
				newState->ROB[state->RNALTop.physReg].destReg = state->RNALTop.destReg;
				newState->ROB[state->RNALTop.physReg].value = 0;
				newState->ROB[state->RNALTop.physReg].opcode = Opcode(state->RNALTop.instr);
				newState->ROB[state->RNALTop.physReg].ready = 0;
				newState->ROB[state->RNALTop.physReg].branchTarget = state->RNALTop.pcPlus1 + state->RNALTop.offset;
				newState->ROBtail = state->RNALTop.physReg + 1;
				newState->ROBtail %= SIZEOFREORDERBUFFER;
				found = 1;
			}
		}
		if(!found)
		{
			/* R-Type Reservation Stations full */
			newState->stall = 1;
			return;
		}
	}
	else if(Opcode(state->RNALTop.instr) == LW || Opcode(state->RNALTop.instr) == SW)
	{
		/* Memory Operation */
		int found = 0;
		for(int i = 0; i < NUMRESERVATIONSTATIONS && !found; i++)
		{
			if(newState->RSmem[i].opcode == NOOP)
			{
			newState->RSmem[i].src1 = state->RNALTop.regA;
			newState->RSmem[i].src2 = state->RNALTop.regB;
			newState->RSmem[i].dest = state->RNALTop.physReg;
			if(!newState->RNALTop.forwardedA)
				newState->RSmem[i].value1 = state->ARF[Field0(state->RNALTop.instr)];
			else
				newState->RSmem[i].value1 = newState->RNALTop.forwardA;
			newState->RSmem[i].valid1 = state->RNALTop.Arenamed;
			if(!newState->RNALTop.forwardedB)
				newState->RSmem[i].value2 = state->ARF[Field1(state->RNALTop.instr)];
			else
				newState->RSmem[i].value2 = newState->RNALTop.forwardB;
			newState->RSmem[i].valid2 = state->RNALTop.Brenamed;
			newState->RSmem[i].offset = Field2(state->RNALTop.instr);
			newState->RSmem[i].opcode = Opcode(state->RNALTop.instr);
			newState->ROB[state->RNALTop.physReg].pc = state->RNALTop.pcPlus1;
			newState->ROB[state->RNALTop.physReg].destReg = state->RNALTop.destReg;
			newState->ROB[state->RNALTop.physReg].value = 0;
			newState->ROB[state->RNALTop.physReg].opcode = Opcode(state->RNALTop.instr);
			newState->ROB[state->RNALTop.physReg].ready = 0;
			newState->ROB[state->RNALTop.physReg].branchTarget = state->RNALTop.pcPlus1 + state->RNALTop.offset;
			newState->ROBtail = state->RNALTop.physReg + 1;
			newState->ROBtail %= SIZEOFREORDERBUFFER;
			found = 1;
			}
		}
		if(!found)
		{
			/* MEM Reservation Stations full */
			newState->stall = 1;
			return;
		}
	}
	else if(Opcode(state->RNALTop.instr) == MULT)
	{
		/* MULT instruction */
		int i;
		int found = 0;
		for(i = 0; i < NUMRESERVATIONSTATIONS && !found; i++)
		{
			if(newState->RSmult[i].opcode == NOOP)
			{
				newState->RSmult[i].src1 = state->RNALTop.regA;
				newState->RSmult[i].src2 = state->RNALTop.regB;
				newState->RSmult[i].dest = state->RNALTop.physReg;
				if(!newState->RNALTop.forwardedA)
					newState->RSmult[i].value1 = state->ARF[Field0(state->RNALTop.instr)];
				else
					newState->RSmult[i].value1 = newState->RNALTop.forwardA;
				newState->RSmult[i].valid1 = state->RNALTop.Arenamed;
				if(!newState->RNALTop.forwardedB)
					newState->RSmult[i].value2 = state->ARF[Field1(state->RNALTop.instr)];
				else
					newState->RSmult[i].value2 = newState->RNALTop.forwardB;
				newState->RSmult[i].valid2 = state->RNALTop.Brenamed;
				newState->RSmult[i].offset = Field2(state->RNALTop.instr);
				newState->RSmult[i].opcode = MULT;
				newState->ROB[state->RNALTop.physReg].pc = state->RNALTop.pcPlus1;
				newState->ROB[state->RNALTop.physReg].destReg = state->RNALTop.destReg;
				newState->ROB[state->RNALTop.physReg].value = 0;
				newState->ROB[state->RNALTop.physReg].opcode = Opcode(state->RNALTop.instr);
				newState->ROB[state->RNALTop.physReg].ready = 0;
				newState->ROB[state->RNALTop.physReg].branchTarget = state->RNALTop.pcPlus1 + state->RNALTop.offset;
				newState->ROBtail = state->RNALTop.physReg + 1;
				newState->ROBtail %= SIZEOFREORDERBUFFER;
				newState->ROB[state->ROBtail].pc = state->RNALTop.pcPlus1;
				newState->ROB[state->ROBtail].destReg = state->RNALTop.destReg;
				newState->ROB[state->ROBtail].value = 0;
				newState->ROB[state->ROBtail].opcode = MULT;
				newState->ROB[state->ROBtail].ready = 0;
				newState->ROB[state->ROBtail].branchTarget = 0;
				newState->ROBtail++;
				newState->ROBtail %= SIZEOFREORDERBUFFER;
				found = 1;
			}
		}
		if(!found)
		{
			/* MULT Reservation Stations full */
			newState->stall = 1;
			return;
		}
	}
	else if(Opcode(state->RNALTop.instr) == HALT)
	{
		/* HALT */
		newState->ROB[state->RNALTop.physReg].opcode = Opcode(state->RNALTop.instr);
		newState->ROB[state->RNALTop.physReg].pc = state->RNALTop.pcPlus1;
		newState->ROB[state->RNALTop.physReg].destReg = 0;
		newState->ROB[state->RNALTop.physReg].value = 0;
		newState->ROB[state->RNALTop.physReg].branchTarget = 0;
		newState->ROBtail = state->RNALTop.physReg + 1;
		newState->ROBtail %= SIZEOFREORDERBUFFER;
		newState->ROB[state->RNALTop.physReg].ready = 1;
	}
	/* ....and then the Bottom. */
	if(state->ROBhead == (state->ROBtail + 2) % SIZEOFREORDERBUFFER)
	{
		newState->stall = 1;
		return;
	}
	else if(Opcode(state->RNALBottom.instr) == ADD || Opcode(state->RNALBottom.instr) == NAND || Opcode(state->RNALBottom.instr) == BEQ)
	{
		/* R-Type Instruction */
		int i;
		int found = 0;
		for(i = 0; i < NUMRESERVATIONSTATIONS && !found; i++)
		{
			if(newState->RSadd[i].opcode == NOOP)
			{
				newState->RSadd[i].src1 = state->RNALBottom.regA;
				newState->RSadd[i].src2 = state->RNALBottom.regB;
				newState->RSadd[i].dest = state->RNALBottom.physReg;
				if(!newState->RNALBottom.forwardedA)
					newState->RSadd[i].value1 = state->ARF[Field0(state->RNALBottom.instr)];
				else
					newState->RSadd[i].value1 = newState->RNALBottom.forwardA;
				newState->RSadd[i].valid1 = state->RNALBottom.Arenamed;
				if(!newState->RNALBottom.forwardedB)
					newState->RSadd[i].value2 = state->ARF[Field1(state->RNALBottom.instr)];
				else
					newState->RSadd[i].value2 = newState->RNALBottom.forwardB;
				newState->RSadd[i].valid2 = state->RNALBottom.Brenamed;
				newState->RSadd[i].offset = Field2(state->RNALBottom.instr);
				newState->RSadd[i].opcode = Opcode(state->RNALBottom.instr);
				newState->ROB[state->RNALBottom.physReg].pc = state->RNALBottom.pcPlus1;
				newState->ROB[state->RNALBottom.physReg].destReg = state->RNALBottom.destReg;
				newState->ROB[state->RNALBottom.physReg].value = 0;
				newState->ROB[state->RNALBottom.physReg].opcode = Opcode(state->RNALBottom.instr);
				newState->ROB[state->RNALBottom.physReg].ready = 0;
				newState->ROB[state->RNALBottom.physReg].branchTarget = state->RNALBottom.pcPlus1 + state->RNALBottom.offset;
				newState->ROBtail = state->RNALBottom.physReg + 1;
				newState->ROBtail %= SIZEOFREORDERBUFFER;
				found = 1;
			}
		}
		if(!found)
		{
			/* R-Type Reservation Stations full */
			newState->stall = 1;
			return;
		}
	}
	else if(Opcode(state->RNALBottom.instr) == LW || Opcode(state->RNALBottom.instr) == SW)
	{
		/* Memory Operation */
		int i;
		int found = 0;
		for(i = 0; i < NUMRESERVATIONSTATIONS && !found; i++)
		{
			if(newState->RSmem[i].opcode == NOOP)
			{
				newState->RSmem[i].src1 = state->RNALBottom.regA;
				newState->RSmem[i].src2 = state->RNALBottom.regB;
				newState->RSmem[i].dest = state->RNALBottom.physReg;
				if(!newState->RNALBottom.forwardedA)
					newState->RSmem[i].value1 = state->ARF[Field0(state->RNALBottom.instr)];
				else
					newState->RSmem[i].value1 = newState->RNALBottom.forwardA;
				newState->RSmem[i].valid1 = state->RNALBottom.Arenamed;
				if(!newState->RNALBottom.forwardedB)
					newState->RSmem[i].value2 = state->ARF[Field1(state->RNALBottom.instr)];
				else
					newState->RSmem[i].value2 = newState->RNALBottom.forwardB;
				newState->RSmem[i].valid2 = state->RNALBottom.Brenamed;
				newState->RSmem[i].offset = Field2(state->RNALBottom.instr);
				newState->RSmem[i].opcode = Opcode(state->RNALBottom.instr);
				newState->ROB[state->RNALBottom.physReg].pc = state->RNALBottom.pcPlus1;
				newState->ROB[state->RNALBottom.physReg].destReg = state->RNALBottom.destReg;
				newState->ROB[state->RNALBottom.physReg].value = 0;
				newState->ROB[state->RNALBottom.physReg].opcode = Opcode(state->RNALBottom.instr);
				newState->ROB[state->RNALBottom.physReg].ready = 0;
				newState->ROB[state->RNALBottom.physReg].branchTarget = state->RNALBottom.pcPlus1 + state->RNALBottom.offset;
				newState->ROBtail = state->RNALBottom.physReg + 1;
				newState->ROBtail %= SIZEOFREORDERBUFFER;
				found = 1;
			}
		}
		if(!found)
		{
			/* MEM Reservation Stations full */
			newState->stall = 1;
			return;
		}
	}
	else if(Opcode(state->RNALBottom.instr) == MULT)
	{
		/* MULT instruction */
		int i;
		int found = 0;
		for(i = 0; i < NUMRESERVATIONSTATIONS && !found; i++)
		{
			if(newState->RSmult[i].opcode == NOOP)
			{
				newState->RSmult[i].src1 = state->RNALBottom.regA;
				newState->RSmult[i].src2 = state->RNALBottom.regB;
				newState->RSmult[i].dest = state->RNALBottom.physReg;
				if(!newState->RNALBottom.forwardedA)
					newState->RSmult[i].value1 = state->ARF[Field0(state->RNALBottom.instr)];
				else
					newState->RSmult[i].value1 = newState->RNALBottom.forwardA;
				newState->RSmult[i].valid1 = state->RNALBottom.Arenamed;
				if(!newState->RNALBottom.forwardedB)
					newState->RSmult[i].value2 = state->ARF[Field1(state->RNALBottom.instr)];
				else
					newState->RSmult[i].value2 = newState->RNALBottom.forwardedB;
				newState->RSmult[i].valid2 = state->RNALBottom.Brenamed;
				newState->RSmult[i].offset = Field2(state->RNALBottom.instr);
				newState->RSmult[i].opcode = MULT;
				newState->ROB[state->RNALBottom.physReg].pc = state->RNALBottom.pcPlus1;
				newState->ROB[state->RNALBottom.physReg].destReg = state->RNALBottom.destReg;
				newState->ROB[state->RNALBottom.physReg].value = 0;
				newState->ROB[state->RNALBottom.physReg].opcode = Opcode(state->RNALBottom.instr);
				newState->ROB[state->RNALBottom.physReg].ready = 0;
				newState->ROB[state->RNALBottom.physReg].branchTarget = state->RNALBottom.pcPlus1 + state->RNALBottom.offset;
				newState->ROBtail = state->RNALBottom.physReg + 1;
				newState->ROBtail %= SIZEOFREORDERBUFFER;
				found = 1;
			}
		}
		if(!found)
		{
			/* MULT Reservation Stations full */
			newState->stall = 1;
			return;
		}
	}
	else if(Opcode(state->RNALBottom.instr) == HALT)
	{
		/* HALT */
		newState->ROB[state->RNALBottom.physReg].opcode = Opcode(state->RNALBottom.instr);
		newState->ROB[state->RNALBottom.physReg].pc = state->RNALBottom.pcPlus1;
		newState->ROB[state->RNALBottom.physReg].destReg = 0;
		newState->ROB[state->RNALBottom.physReg].value = 0;
		newState->ROB[state->RNALBottom.physReg].branchTarget = 0;
		newState->ROBtail = state->RNALBottom.physReg + 1;
		newState->ROB[state->RNALBottom.physReg].ready = 1;
		newState->ROBtail %= SIZEOFREORDERBUFFER;
	}

	return;
}

void Schedule(stateType* newState, const stateType* state)
{
	int age[NUMRESERVATIONSTATIONS];
	int orderedByAge[NUMRESERVATIONSTATIONS];
	int freedRS = -1;
	int temp;
	int scheduling = 0;
	int foundBEQ = 0;

	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		age[i] = (state->RSadd[i].dest - state->ROBhead) % SIZEOFREORDERBUFFER;
		if(age[i] < 0)
			age[i] += SIZEOFREORDERBUFFER;
		orderedByAge[i] = i;
	}
	for(int i = 0; i < (NUMRESERVATIONSTATIONS - 1); i++)
	{
		for(int k = i; k < (NUMRESERVATIONSTATIONS - 1); k++)
		{
			if(age[k] > age[k+1])
			{
				temp = age[k];
				age[k] = age[k+1];
				age[k+1] = temp;
				temp = orderedByAge[k];
				orderedByAge[k] = orderedByAge[k+1];
				orderedByAge[k+1] = temp;
			}
		}
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS && !foundBEQ; i++)
	{
		if(newState->RSadd[i].opcode == BEQ && !newState->RSadd[i].valid1 && !newState->RSadd[i].valid2)
			foundBEQ = 1;
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS && !scheduling; i++)
	{
		if(!newState->RSadd[orderedByAge[i]].valid1 && !newState->RSadd[orderedByAge[i]].valid2 && state->RSadd[orderedByAge[i]].opcode != NOOP && (state->RSadd[orderedByAge[i]].opcode == BEQ || !foundBEQ))
		{
			freedRS = orderedByAge[i];
			scheduling = 1;
		}
	}
	scheduling = 0;
	if(freedRS > -1 && freedRS < NUMRESERVATIONSTATIONS)
	{
		printf("We found %d for add!\n", freedRS);
		newState->FUADD.regA = newState->RSadd[freedRS].value1;
		newState->FUADD.regB = newState->RSadd[freedRS].value2;
		newState->FUADD.dest = state->RSadd[freedRS].dest;
		newState->FUADD.opcode = state->RSadd[freedRS].opcode;
		newState->RSadd[freedRS].src1 = 0;
		newState->RSadd[freedRS].src2 = 0;
		newState->RSadd[freedRS].dest = 0;
		newState->RSadd[freedRS].value1 = 0;
		newState->RSadd[freedRS].valid1 = 0;
		newState->RSadd[freedRS].value2 = 0;
		newState->RSadd[freedRS].valid2 = 0;
		newState->RSadd[freedRS].offset = 0;
		newState->RSadd[freedRS].opcode = NOOP;
	}
	freedRS = -1;
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		age[i] = (state->RSmem[i].dest - state->ROBhead) % SIZEOFREORDERBUFFER;
		if(age[i] < 0)
			age[i] += SIZEOFREORDERBUFFER;
		orderedByAge[i] = i;
	}
	for(int i = 0; i < (NUMRESERVATIONSTATIONS - 1); i++)
	{
		for(int k = i; k < (NUMRESERVATIONSTATIONS - 1); k++)
		{
			if(age[k] > age[k+1])
			{
				temp = age[k];
				age[k] = age[k+1];
				age[k+1] = temp;
				temp = orderedByAge[k];
				orderedByAge[k] = orderedByAge[k+1];
				orderedByAge[k+1] = temp;
			}
		}
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS && !scheduling; i++)
	{
		if(!newState->RSmem[orderedByAge[i]].valid1 && !newState->RSmem[orderedByAge[i]].valid2 && state->RSmem[orderedByAge[i]].opcode != NOOP)
		{
			freedRS = orderedByAge[i];
			scheduling = 1;
		}
	}
	scheduling = 0;
	if(state->FUMEM.timeLeft == 0 && freedRS > -1 && freedRS < NUMRESERVATIONSTATIONS)
	{
		printf("We found %d for mem!\n", freedRS);
		newState->FUMEM.regA = newState->RSmem[freedRS].value1;
		newState->FUMEM.regB = newState->RSmem[freedRS].value2;
		newState->FUMEM.dest = state->RSmem[freedRS].dest;
		newState->FUMEM.opcode = state->RSmem[freedRS].opcode;
		newState->FUMEM.offset = state->RSmem[freedRS].offset;
		newState->FUMEM.timeLeft = 2;
		newState->RSmem[freedRS].src1 = 0;
		newState->RSmem[freedRS].src2 = 0;
		newState->RSmem[freedRS].dest = 0;
		newState->RSmem[freedRS].value1 = 0;
		newState->RSmem[freedRS].valid1 = 0;
		newState->RSmem[freedRS].value2 = 0;
		newState->RSmem[freedRS].valid2 = 0;
		newState->RSmem[freedRS].offset = 0;
		newState->RSmem[freedRS].opcode = NOOP;
	}
	freedRS = -1;
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		age[i] = (state->RSmem[i].dest - state->ROBhead) % SIZEOFREORDERBUFFER;
		if(age[i] < 0)
			age[i] += SIZEOFREORDERBUFFER;
		orderedByAge[i] = i;
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS - 1; i++)
	{
		for(int k = i; k < NUMRESERVATIONSTATIONS - 1; k++)
		{
			if(age[k] > age[k+1])
			{
				temp = age[k];
				age[k] = age[k+1];
				age[k+1] = temp;
				temp = orderedByAge[k];
				orderedByAge[k] = orderedByAge[k+1];
				orderedByAge[k+1] = temp;
			}
		}	
	}
	for(int i = 0; i < NUMRESERVATIONSTATIONS && !scheduling; i++)
	{
		if(!newState->RSmult[orderedByAge[i]].valid1 && !newState->RSmult[orderedByAge[i]].valid2 && state->RSmult[orderedByAge[i]].opcode != NOOP)
		{
			freedRS = orderedByAge[i];
			scheduling = 1;
		}
	}
	if(state->FUMULT.pipe[0].opcode == NOOP && state->FUMULT.pipe[1].opcode == NOOP && freedRS > -1 && freedRS < NUMRESERVATIONSTATIONS)
	{
		printf("We found %d for mult!\n", freedRS);
		newState->FUMULT.pipe[0].regA = newState->RSadd[freedRS].value1;
		newState->FUMULT.pipe[0].regB = newState->RSadd[freedRS].value2;
		newState->FUMULT.pipe[0].dest = state->RSadd[freedRS].dest;
		newState->FUMULT.pipe[0].opcode = state->RSadd[freedRS].opcode;
		newState->RSmult[freedRS].src1 = 0;
		newState->RSmult[freedRS].src2 = 0;
		newState->RSmult[freedRS].dest = 0;
		newState->RSmult[freedRS].value1 = 0;
		newState->RSmult[freedRS].valid1 = 0;
		newState->RSmult[freedRS].value2 = 0;
		newState->RSmult[freedRS].valid2 = 0;
		newState->RSmult[freedRS].offset = 0;
		newState->RSmult[freedRS].opcode = NOOP;
	}

	return;
}

void Execute(stateType* newState, const stateType* state)
{
	for(int i = 5; i > 0; i--)
	{
		newState->FUMULT.pipe[i].opcode = state->FUMULT.pipe[i-1].opcode;
		newState->FUMULT.pipe[i].regA = state->FUMULT.pipe[i-1].regA;
		newState->FUMULT.pipe[i].regB = state->FUMULT.pipe[i-1].regB;
		newState->FUMULT.pipe[i].dest = state->FUMULT.pipe[i-1].dest;
	}
	if(state->FUMULT.pipe[5].opcode != NOOP)
	{
		int theAnswer = MultInstruction(state->FUMULT.pipe[5].regA, state->FUMULT.pipe[5].regB);
		newState->ROB[state->FUMULT.pipe[5].dest].value = theAnswer;
		newState->ROB[state->FUMULT.pipe[5].dest].ready = 1;
		for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
		{
			if(state->RSadd[i].src1 == state->FUMULT.pipe[5].dest && state->RSadd[i].valid1)
			{
				newState->RSadd[i].value1 = theAnswer;
				newState->RSadd[i].valid1 = 0;
			}
			if(state->RSmem[i].src1 == state->FUMULT.pipe[5].dest && state->RSadd[i].valid1)
			{
				newState->RSmem[i].value1 = theAnswer;
				newState->RSmem[i].valid1 = 0;
			}
			if(state->RSmult[i].src1 == state->FUMULT.pipe[5].dest && state->RSmem[i].valid1)
			{
				newState->RSmult[i].value1 = theAnswer;
				newState->RSmult[i].valid1 = 0;
			}
			if(state->RNALTop.regA == state->FUMULT.pipe[5].dest && state->RNALTop.Arenamed)
			{
				newState->RNALTop.forwardA = theAnswer;
				newState->RNALBottom.forwardedA = 1;
				newState->RNALTop.Arenamed = 0;
			}
			if(state->RNALBottom.regA == state->FUMULT.pipe[5].dest && state->RNALBottom.Arenamed)
			{
				newState->RNALBottom.forwardA = theAnswer;
				newState->RNALBottom.forwardedA = 1;
				newState->RNALBottom.Arenamed = 0;
			}
			if(state->RSadd[i].src2 == state->FUMULT.pipe[5].dest && state->RSadd[i].valid2)
			{
				newState->RSadd[i].value2 = theAnswer;
				newState->RSadd[i].valid2 = 0;
			}
			if(state->RSmem[i].src2 == state->FUMULT.pipe[5].dest && state->RSmem[i].valid2)
			{
				newState->RSmem[i].value2 = theAnswer;
				newState->RSmem[i].valid2 = 0;
			}
			if(state->RSmult[i].src2 == state->FUMULT.pipe[5].dest && state->RSmult[i].valid2)
			{
				newState->RSmult[i].value2 = theAnswer;
				newState->RSmult[i].valid2 = 0;
			}
			if(state->RNALTop.regB == state->FUMULT.pipe[5].dest && state->RNALTop.Brenamed)
			{
				newState->RNALTop.forwardB = theAnswer;
				newState->RNALBottom.forwardedB = 1;
				newState->RNALTop.Brenamed = 0;
			}
			if(state->RNALBottom.regB == state->FUMULT.pipe[5].dest && state->RNALBottom.Brenamed)
			{
				newState->RNALBottom.forwardB = theAnswer;
				newState->RNALBottom.forwardedB = 1;
				newState->RNALBottom.Brenamed = 0;
			}
		}
	}
	if(state->FUADD.opcode != NOOP)
	{
		int theAnswer = RTypeInstruction(state->FUADD.opcode, state->FUADD.regA, state->FUADD.regB);
		newState->FUADD.opcode = NOOP;
		newState->ROB[state->FUADD.dest].value = theAnswer;
		newState->ROB[state->FUADD.dest].ready = 1;
		for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
		{
			if(state->RSadd[i].src1 == state->FUADD.dest && state->RSadd[i].valid1)
			{
				newState->RSadd[i].value1 = theAnswer;
				newState->RSadd[i].valid1 = 0;
			}
			if(state->RSmem[i].src1 == state->FUADD.dest && state->RSmem[i].valid1)
			{
				newState->RSmem[i].value1 = theAnswer;
				newState->RSmem[i].valid1 = 0;
			}
			if(state->RSmult[i].src1 == state->FUADD.dest && state->RSmult[i].valid1)
			{
				newState->RSmult[i].value1 = theAnswer;
				newState->RSmult[i].valid1 = 0;
			}
			if(state->RNALTop.regA == state->FUADD.dest && state->RNALTop.Arenamed)
			{
				newState->RNALTop.forwardA = theAnswer;
				newState->RNALBottom.forwardedA = 1;
				newState->RNALTop.Arenamed = 0;
			}
			if(state->RNALBottom.regA == state->FUADD.dest && state->RNALBottom.Arenamed)
			{
				newState->RNALBottom.forwardA = theAnswer;
				newState->RNALBottom.forwardedA = 1;
				newState->RNALBottom.Arenamed = 0;
			}
			if(state->RSadd[i].src2 == state->FUADD.dest && state->RSadd[i].valid2)
			{
				newState->RSadd[i].value2 = theAnswer;
				newState->RSadd[i].valid2 = 0;
			}
			if(state->RSmem[i].src2 == state->FUADD.dest && state->RSmem[i].valid2)
			{
				newState->RSmem[i].value2 = theAnswer;
				newState->RSmem[i].valid2 = 0;
			}
			if(state->RSmult[i].src2 == state->FUADD.dest && state->RSmult[i].valid2)
			{
				newState->RSmult[i].value2 = theAnswer;
				newState->RSmult[i].valid2 = 0;
			}
			if(state->RNALTop.regB == state->FUADD.dest && state->RNALTop.Brenamed)
			{
				newState->RNALTop.forwardB = theAnswer;
				newState->RNALBottom.forwardedB = 1;
				newState->RNALTop.Brenamed = 0;
			}
			if(state->RNALBottom.regB == state->FUADD.dest && state->RNALBottom.Brenamed)
			{
				newState->RNALBottom.forwardB = theAnswer;
				newState->RNALBottom.forwardedB = 1;
				newState->RNALBottom.Brenamed = 0;
			}
		}
	}
	if(state->FUMEM.timeLeft == 0 && state->FUMEM.opcode != NOOP)
	{
		int address = ITypeInstruction(state->FUMEM.regA, state->FUMEM.offset);
		if(state->FUMEM.opcode == LW)
		{
			newState->ROB[state->FUMEM.dest].value = state->dataMem[address];
			newState->ROB[state->FUMEM.dest].ready = 1;
			newState->FUMEM.opcode = NOOP;
			for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
			{
				if(state->RSadd[i].src1 == state->FUMEM.dest && state->RSadd[i].valid1)
				{
					newState->RSadd[i].value1 = state->dataMem[address];
					newState->RSadd[i].valid1 = 0;
				}
				if(state->RSmem[i].src1 == state->FUMEM.dest && state->RSmem[i].valid1)
				{
					newState->RSmem[i].value1 = state->dataMem[address];
					newState->RSmem[i].valid1 = 0;
				}
				if(state->RSmult[i].src1 == state->FUMEM.dest && state->RSmult[i].valid1)
				{
					newState->RSmult[i].value1 = state->dataMem[address];
					newState->RSmult[i].valid1 = 0;
				}
				if(state->RNALTop.regA == state->FUMEM.dest && state->RNALTop.Arenamed)
				{
					newState->RNALTop.forwardA = state->dataMem[address];
					newState->RNALTop.forwardedA = 1;
					newState->RNALTop.Arenamed = 0;
				}
				if(state->RNALBottom.regA == state->FUMEM.dest && state->RNALBottom.Arenamed)
				{
					newState->RNALBottom.forwardA = state->dataMem[address];
					newState->RNALBottom.forwardedA = 1;
					newState->RNALBottom.Arenamed = 0;
				}
				if(state->RSadd[i].src2 == state->FUMEM.dest && state->RSadd[i].valid2)
				{
					newState->RSadd[i].value2 = state->dataMem[address];
					newState->RSadd[i].valid2 = 0;
				}
				if(state->RSmem[i].src2 == state->FUMEM.dest && state->RSmem[i].valid2)
				{
					newState->RSmem[i].value2 = state->dataMem[address];
					newState->RSmem[i].valid2 = 0;
				}
				if(state->RSmult[i].src2 == state->FUMEM.dest && state->RSmult[i].valid2)
				{
					newState->RSmult[i].value2 = state->dataMem[address];
					newState->RSmult[i].valid2 = 0;
				}
				if(state->RNALTop.regB == state->FUMEM.dest && state->RNALTop.Brenamed)
				{
					newState->RNALTop.forwardB = state->dataMem[address];
					newState->RNALTop.forwardedB = 1;
					newState->RNALTop.Brenamed = 0;
				}
				if(state->RNALBottom.regB == state->FUMEM.dest && state->RNALBottom.Brenamed)
				{
					newState->RNALBottom.forwardB = state->dataMem[address];
					newState->RNALTop.forwardedB = 1;
					newState->RNALBottom.Brenamed = 0;
				}
			}
		}
		else if(state->FUMEM.opcode == SW)
		{
			newState->dataMem[address] = state->FUMEM.regB;
			newState->ROB[state->FUMEM.dest].ready = 1;
			newState->FUMEM.opcode = NOOP;
		}
	}
	else if(state->FUMEM.opcode != NOOP)
	{
		newState->FUMEM.timeLeft--;
	}
	return;
}

void Commit(stateType* newState, const stateType* state)
{
	int retiredOne = 0;
	if(state->ROB[newState->ROBhead].ready)
	{
		if(state->ROB[newState->ROBhead].opcode == ADD || state->ROB[newState->ROBhead].opcode == NAND || state->ROB[newState->ROBhead].opcode == MULT || state->ROB[newState->ROBhead].opcode == LW)
		{
			newState->ARF[state->ROB[newState->ROBhead].destReg] = state->ROB[newState->ROBhead].value;
			if(newState->RNT[state->ROB[newState->ROBhead].destReg].physReg == newState->ROBhead)
				newState->RNT[state->ROB[newState->ROBhead].destReg].valid = 0;
		}
		else if(state->ROB[newState->ROBhead].opcode == HALT)
			newState->halting = 1;
		else if(state->ROB[newState->ROBhead].opcode == BEQ && state->ROB[newState->ROBhead].value == 0)
		{
			/* successful branch */
			newState->takenBranches++;
			/* branch predictor accurate? */
			if(state->predictor == PREDICTSTRONGNO || state->predictor == PREDICTWEAKNO)
			{
				/* no.... */
				newState->incorrectBranchPredictions++;
				newState->predictor++;
				newState->predictor %= NUMPREDICTIONS;
			}
			else
			{
				/* yes! */
				newState->predictor = PREDICTSTRONGYES;
			}
			/* wipe the processor */
			int inBTBuffer = -1;
			for(int i = 0; i < SIZEOFBTBUFFER && inBTBuffer == -1; i++)
			{
				if(newState->ROB[newState->ROBhead].pc - 1 == state->BTB[i].tag)
						inBTBuffer = i;
			}
			if(inBTBuffer > -1)
			{
				newState->pc = state->BTB[inBTBuffer].data;
			}
			else
			{
				newState->pc = state->ROB[newState->ROBhead].branchTarget;
				newState->BTB[3].data = state->BTB[2].data;
				newState->BTB[2].data = state->BTB[1].data;
				newState->BTB[1].data = state->BTB[0].data;
				newState->BTB[0].data = state->ROB[newState->ROBhead].branchTarget;
				newState->BTB[3].tag = state->BTB[2].tag;
				newState->BTB[2].tag = state->BTB[1].tag;
				newState->BTB[1].tag = state->BTB[0].tag;
				newState->BTB[0].tag = newState->ROB[newState->ROBhead].pc - 1;
			}
			ClearForBranch(newState);
			newState->branched = 1;
		}
		else if(state->ROB[state->ROBhead].opcode == BEQ && state->ROB[state->ROBhead].value != 0 && !newState->branched)
		{
			/* failed branch */
			/* branch predictor accurate? */
			if(state->predictor == PREDICTSTRONGYES || state->predictor == PREDICTWEAKYES)
			{
				/* no.... */
				newState->incorrectBranchPredictions++;
				newState->predictor++;
				newState->predictor %= NUMPREDICTIONS;
			}
			else
			{
				/* yes! */
				newState->predictor = PREDICTSTRONGNO;
			}
			newState->branching = 0;
		}
		if(state->ROB[newState->ROBhead].opcode != NOOP)
		{
			newState->retiredInstructions++;
			retiredOne = 1;
			newState->ROB[newState->ROBhead].ready = 0;
			if(!newState->branched)
				newState->ROBhead++;
			newState->ROBhead %= SIZEOFREORDERBUFFER;
		}
	}
	if(newState->ROBhead != state->ROBtail && retiredOne && !newState->halting && !newState->branched)
	{
		if(state->ROB[newState->ROBhead].ready)
		{
			if(state->ROB[newState->ROBhead].opcode == ADD || state->ROB[newState->ROBhead].opcode == NAND || state->ROB[newState->ROBhead].opcode == MULT || state->ROB[newState->ROBhead].opcode == LW)
			{
				newState->ARF[state->ROB[newState->ROBhead].destReg] = state->ROB[newState->ROBhead].value;
				if(newState->RNT[state->ROB[newState->ROBhead].destReg].physReg == newState->ROBhead)
					newState->RNT[state->ROB[newState->ROBhead].destReg].valid = 0;
			}
			else if(state->ROB[newState->ROBhead].opcode == HALT)
				newState->halting = 1;
			else if(state->ROB[newState->ROBhead].opcode == BEQ && state->ROB[newState->ROBhead].value == 0)
			{
				/* successful branch */
				newState->takenBranches++;
				/* branch predictor accurate? */
				if(state->predictor == PREDICTSTRONGNO || state->predictor == PREDICTWEAKNO)
				{
					/* no.... */
					newState->incorrectBranchPredictions++;
					newState->predictor++;
					newState->predictor %= NUMPREDICTIONS;
				}
				else
				{
					/* yes! */
					newState->predictor = PREDICTSTRONGYES;
				}
				/* Branch Target Buffer work */
				int inBTBuffer = -1;
				for(int i = 0; i < SIZEOFBTBUFFER && inBTBuffer == -1; i++)
				{
					if(newState->ROB[newState->ROBhead].pc - 1 == state->BTB[i].tag)
							inBTBuffer = i;
				}
				if(inBTBuffer > -1)
				{
					newState->pc = state->BTB[inBTBuffer].data;
				}
				else
				{
					newState->pc = state->ROB[newState->ROBhead].branchTarget;
					newState->BTB[3].data = state->BTB[2].data;
					newState->BTB[2].data = state->BTB[1].data;
					newState->BTB[1].data = state->BTB[0].data;
					newState->BTB[0].data = state->ROB[newState->ROBhead].branchTarget;
					newState->BTB[3].tag = state->BTB[2].tag;
					newState->BTB[2].tag = state->BTB[1].tag;
					newState->BTB[1].tag = state->BTB[0].tag;
					newState->BTB[0].tag = newState->ROB[newState->ROBhead].pc - 1;
				}
				ClearForBranch(newState);
				newState->branched = 1;
			}
			else if(state->ROB[newState->ROBhead].opcode == BEQ && state->ROB[newState->ROBhead].value != 0)
			{
				/* failed branch */
				/* branch predictor accurate? */
				if(state->predictor == PREDICTSTRONGYES || state->predictor == PREDICTWEAKYES)
				{
					/* no.... */
					newState->incorrectBranchPredictions++;
					newState->predictor++;
					newState->predictor %= NUMPREDICTIONS;
				}
				else
				{
					/* yes! */
					newState->predictor = PREDICTSTRONGNO;
				}
				newState->branching = 0;
			}
			if(state->ROB[newState->ROBhead].opcode != NOOP)
			{
				newState->retiredInstructions++;
				newState->ROB[newState->ROBhead].ready = 0;
				if(!newState->branched)
					newState->ROBhead++;
				newState->ROBhead %= SIZEOFREORDERBUFFER;
			}
		}

	}

	return;
}

int RTypeInstruction(int opcode, int regA, int regB)
{
	/* looks like some computation! */
	int answer;
	if(opcode == ADD)
	{
//		printf("add ");
		answer = regA + regB;
	}
	else if(opcode == NAND)
	{
//		printf("nand ");
		answer = ~(regA & regB);
	}
	else if(opcode == BEQ)
	{
//		printf("beq ");
		answer = regA - regB;
	}
//	printf("%d %d\n", regA, regB);
	return(answer);
}

int ITypeInstruction(int regA, int offset)
{
	/* looks like memory access */
//	printf("%d %d\n", regA, offset);
	return(regA + offset);
}

int MultInstruction(int regA, int regB)
{
	/* pulling out my times tables... */
	return(regA * regB);
}
void PrintState(stateType *statePtr)
{
	int i;
	printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
	printf("\tpc %d\n", statePtr->pc);

	printf("\tdata memory:\n");
	for (i=0; i<statePtr->numMemory; i++)
	{
		printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
	}

	printf("\tregisters:\n");
	for (i=0; i<NUMREGS; i++)
	{
		printf("\t\treg[ %d ] %d\n", i, statePtr->ARF[i]);
	}

	printf("\tRename Table:\n");
	for(int i = 0; i < SIZEOFRENAMETABLE; i++)
	{
		printf("\t\treg[ %d ] %d\tValid bit: %d\n", i, statePtr->RNT[i].physReg, statePtr->RNT[i].valid);
	}
	
	printf("\tIFRN:\n");
	printf("\tTop:\n");
	printf("\t\tinstruction ");
	PrintInstruction(statePtr->IFRNTop.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IFRNTop.pcPlus1);
	printf("\tBottom:\n");
	printf("\t\tinstruction ");
	PrintInstruction(statePtr->IFRNBottom.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->IFRNBottom.pcPlus1);

	printf("\tRNAL:\n");
	printf("\tTop:\n");
	printf("\t\tinstruction ");
	PrintInstruction(statePtr->RNALTop.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->RNALTop.pcPlus1);
	printf("\t\treadRegA %d\n", statePtr->RNALTop.regA);
	printf("\t\tArenamed bit %d\n", statePtr->RNALTop.Arenamed);
	printf("\t\treadRegB %d\n", statePtr->RNALTop.regB);
	printf("\t\tBrenamed bit %d\n", statePtr->RNALTop.Brenamed);
	printf("\t\tDestination %d\n", statePtr->RNALTop.destReg);
	printf("\t\tPhys. Reg. Selected %d\n", statePtr->RNALTop.physReg);
	printf("\t\toffset %d\n", statePtr->RNALTop.offset);
	printf("\tBottom:\n");
	printf("\t\tinstruction ");
	PrintInstruction(statePtr->RNALBottom.instr);
	printf("\t\tpcPlus1 %d\n", statePtr->RNALBottom.pcPlus1);
	printf("\t\treadRegA %d\n", statePtr->RNALBottom.regA);
	printf("\t\tArenamed bit %d\n", statePtr->RNALBottom.Arenamed);
	printf("\t\treadRegB %d\n", statePtr->RNALBottom.regB);
	printf("\t\tBrenamed bit %d\n", statePtr->RNALBottom.Brenamed);
	printf("\t\tDestination %d\n", statePtr->RNALBottom.destReg);
	printf("\t\tPhys. Reg. Selected %d\n", statePtr->RNALBottom.physReg);
	printf("\t\toffset %d\n", statePtr->RNALBottom.offset);

	printf("\tFunctional Units\n");
	printf("\tADD/NAND/BEQ\n");
	printf("\t\tValue A: %d\tValue B: %d\n", statePtr->FUADD.regA, statePtr->FUADD.regB);
	printf("\t\tDestination(ROB entry) :%d\n", statePtr->FUADD.dest);
	printf("\t\tOpcode: ");
	if(statePtr->FUADD.opcode == ADD)
		printf("ADD\n");
	else if(statePtr->FUADD.opcode == NAND)
		printf("NAND\n");
	else if(statePtr->FUADD.opcode == BEQ)
		printf("BEQ\n");
	else if(statePtr->FUADD.opcode == NOOP)
		printf("NOOP\n");
	else
	{
		printf("OPCODE ERROR!!!\n");
		printf("\t\tOpcode is %d\n", statePtr->FUADD.opcode);
	}

	printf("\tReservation Stations:\n");
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		printf("\tRSadd[ %d ]:\n", i);
		printf("\t\tSource 1: %d\tSource 2: %d\n", statePtr->RSadd[i].src1, statePtr->RSadd[i].src2);
		printf("\t\tDestination: %d\n", statePtr->RSadd[i].dest);
		printf("\t\tValue 1: %d\tValid bit: %d\n", statePtr->RSadd[i].value1, statePtr->RSadd[i].valid1);
		printf("\t\tValue 2: %d\tValid bit: %d\n", statePtr->RSadd[i].value2, statePtr->RSadd[i].valid2);
		printf("\t\tOffset: %d\n", statePtr->RSadd[i].offset);
		printf("\t\tOpcode: ");
		if(statePtr->RSadd[i].opcode == ADD)
			printf("ADD\n");
		else if(statePtr->RSadd[i].opcode == NAND)
			printf("NAND\n");
		else if(statePtr->RSadd[i].opcode == BEQ)
			printf("BEQ\n");
		else if(statePtr->RSadd[i].opcode == NOOP)
			printf("NOOP\n");
		else
		{
			printf("OPCODE ERROR!!!\n");
			printf("\t\tOpcode is %d\n", statePtr->RSadd[i].opcode);
		}
	}

	printf("\tMULT\n");
	for(int i = 0; i < NUMMULTPIPES; i++)
	{
		printf("\t\tPipe[ %d ]\tValue A: %d\tValue B: %d\tDestination:  %d\n", i, statePtr->FUMULT.pipe[i].regA, statePtr->FUMULT.pipe[i].regB, statePtr->FUMULT.pipe[i].dest);
	}
	printf("\tReservation Stations:\n");
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		printf("\tRSmult[ %d ]:\n", i);
		printf("\t\tSource 1: %d\tSource 2: %d\n", statePtr->RSmult[i].src1, statePtr->RSmult[i].src2);
		printf("\t\tDestination: %d\n", statePtr->RSmult[i].dest);
		printf("\t\tValue 1: %d\tValid bit: %d\n", statePtr->RSmult[i].value1, statePtr->RSmult[i].valid1);
		printf("\t\tValue 2: %d\tValid bit: %d\n", statePtr->RSmult[i].value2, statePtr->RSmult[i].valid2);
		printf("\t\tOffset: %d\n", statePtr->RSmult[i].offset);
		printf("\t\tOpcode: ");
		if(statePtr->RSmult[i].opcode == MULT)
			printf("MULT\n");
		else if(statePtr->RSmult[i].opcode == NOOP)
			printf("NOOP\n");
		else
		{
			printf("OPCODE ERROR!!!\n");
			printf("Opcode is %d\n", statePtr->RSmult[i].opcode);
		}
	}

	printf("\tMEMOPS\n");
	printf("\t\tAddress: %d\tOffset: %d\n", statePtr->FUMEM.regA, statePtr->FUMEM.offset);
	printf("\t\tData to be stored(SW ONLY) %d\n", statePtr->FUMEM.regB);
	printf("\t\tDestination(ROB entry): %d\n", statePtr->FUMEM.dest);
	printf("\t\tTime Left: %d\n", statePtr->FUMEM.timeLeft);
	printf("\t\tOpcode: ");
	if(statePtr->FUMEM.opcode == SW)
		printf("SW\n");
	else if(statePtr->FUMEM.opcode == LW)
		printf("LW\n");
	else if(statePtr->FUMEM.opcode == NOOP)
		printf("NOOP\n");
	else
	{
		printf("OPCODE ERROR!!!\n");
		printf("Opcode is %d\n", statePtr->FUMEM.opcode);
	}
	printf("\tReservation Stations:\n");
	for(int i = 0; i < NUMRESERVATIONSTATIONS; i++)
	{
		printf("\tRSmem[ %d ]:\n", i);
		printf("\t\tSource 1: %d\tSource 2: %d\n", statePtr->RSmem[i].src1, statePtr->RSmem[i].src2);
		printf("\t\tDestination: %d\n", statePtr->RSmem[i].dest);
		printf("\t\tValue 1: %d\tValid bit: %d\n", statePtr->RSmem[i].value1, statePtr->RSmem[i].valid1);
		printf("\t\tValue 2: %d\tValid bit: %d\n", statePtr->RSmem[i].value2, statePtr->RSmem[i].valid2);
		printf("\t\tOffset: %d\n", statePtr->RSmem[i].offset);
		printf("\t\tOpcode: ");
		if(statePtr->RSmem[i].opcode == SW)
			printf("SW\n");
		else if(statePtr->RSmem[i].opcode == LW)
			printf("LW\n");
		else if(statePtr->RSmem[i].opcode == NOOP)
			printf("NOOP\n");
		else
		{
			printf("OPCODE ERROR!!!\n");
			printf("Opcode is %d\n", statePtr->RSmem[i].opcode);
		}
	}

	printf("\tReorder Buffer\n");
	printf("\tHead: %d\tTail: %d\n", statePtr->ROBhead, statePtr->ROBtail);
	for(int i = 0; i < SIZEOFREORDERBUFFER; i++)
	{
		printf("\t\tEntry[ %d ]\tDestination %d\tValue %d\n", i, statePtr->ROB[i].destReg, statePtr->ROB[i].value);
		printf("\t\tPC + 1: %d\tOpcode ", statePtr->ROB[i].pc);
		if(statePtr->ROB[i].opcode == ADD)
			printf("ADD\n");
		if(statePtr->ROB[i].opcode == NAND)
			printf("NAND\n");
		if(statePtr->ROB[i].opcode == BEQ)
			printf("BEQ\n");
		if(statePtr->ROB[i].opcode == MULT)
			printf("MULT\n");
		if(statePtr->ROB[i].opcode == SW)
			printf("SW\n");
		if(statePtr->ROB[i].opcode == LW)
			printf("LW\n");
		if(statePtr->ROB[i].opcode == NOOP)
			printf("NOOP\n");
		if(statePtr->ROB[i].opcode == HALT)
			printf("HALT\n");
		printf("\t\tBranch Target: %d\n", statePtr->ROB[i].branchTarget);
		printf("\t\tReady bit %d\n", statePtr->ROB[i].ready);
	}

	printf("\tBranch Prediction:\n");
	printf("\t\t");
	if(statePtr->predictor == PREDICTSTRONGYES)
		printf("strong yes\n");
	else if(statePtr->predictor == PREDICTSTRONGNO)
		printf("strong no\n");
	else if(statePtr->predictor == PREDICTWEAKYES)
		printf("weak yes\n");
	else if(statePtr->predictor == PREDICTWEAKNO)
		printf("weak no\n");
	else
		printf("PREDICTOR ERROR!!!\n");

	printf("\tBranch Target Buffer:\n");
	for(int i = 0; i < SIZEOFBTBUFFER; i++)
		printf("\t\t%d - %d\n", statePtr->BTB[i].tag, statePtr->BTB[i].data);
	
	return;
}

int Field0(int instruction)
{
    return((instruction >> 19) & 0x7);
}

int Field1(int instruction)
{
    return((instruction >> 16) & 0x7);
}

int Field2(int instruction)
{
    return((instruction << 16) >> 16);
}

int Opcode(int instruction)
{
    return(instruction >> 22);
}

void PrintInstruction(int instr)
{
	char opcodeString[10];
	if (Opcode(instr) == ADD)
	{
		strcpy(opcodeString, "add");
	}
	else if (Opcode(instr) == NAND)
	{
		strcpy(opcodeString, "nand");
	}
	else if (Opcode(instr) == LW)
	{
		strcpy(opcodeString, "lw");
	}
	else if (Opcode(instr) == SW)
	{
		strcpy(opcodeString, "sw");
	}
	else if (Opcode(instr) == BEQ)
	{
		strcpy(opcodeString, "beq");
	}
	else if (Opcode(instr) == MULT)
	{
		strcpy(opcodeString, "mult");
	}
	else if (Opcode(instr) == HALT)
	{
		strcpy(opcodeString, "halt");
	}
	else if (Opcode(instr) == NOOP)
	{
		strcpy(opcodeString, "noop");
	}
	else
	{
		strcpy(opcodeString, "data");
	}

	printf("%s %d %d %d\n", opcodeString, Field0(instr), Field1(instr), Field2(instr));
}

void FinishUp(stateType* statePtr)
{
	/* oh? we're already done? */
	printf("machine halted\n");
	printf("CYCLES:\t%d\n", statePtr->cycles - 1);
	printf("FETCHED:\t%d\n", statePtr->checkPoint);
	printf("RETIRED:\t%d\n", statePtr->retiredInstructions);
	printf("BRANCHES:\t%d\n", statePtr->takenBranches);
	printf("MISPRED:\t%d\n", statePtr->incorrectBranchPredictions);
	/* and...pushing the red button! */
	exit(0);
}

/* MODULES */
/* =============================================================== */
/* END PROGRAM */

