#include <iostream>
#include <fstream>
#include "pin.H"
#include <vector>
using namespace std;

ofstream OutFile;
static ADDRINT g_addrLow, g_addrHigh;
static BOOL g_bMainExecLoaded = FALSE;



const ADDRINT READ_HAS_GROUND = 0xae4b;
const ADDRINT WRITE_HIT_METEOR_HIT = 0xaf03;
const ADDRINT WRITE_CRASH_DETECTED = 0xaf08;
const ADDRINT READ_CAR_X = 0xaec0;
const ADDRINT WRITE_SCORE = 0x9214;

VOID ImageLoad(IMG img, VOID *v) {
    if( IMG_IsMainExecutable(img) ) {
        g_addrLow = IMG_LowAddress(img); 
        g_addrHigh = IMG_HighAddress(img);
        g_bMainExecLoaded = TRUE;
        OutFile << "Main image from " << g_addrLow << " to " << g_addrHigh << endl;
    }
}

VOID OverwriteHasGround(
    ADDRINT ip, 
    ADDRINT * regEAX) {
    //OutFile << "has ground " << *regEAX << endl; 
    int* addr = (int*)regEAX;
    *addr = 0;
    //OutFile << *(addr) << " " << *(addr + 1) << " " << *(addr + 2) << endl;
}

VOID OverwriteHitMeteor(
    ADDRINT ip, 
    ADDRINT * regEAX) {
    //OutFile << "hit meteor value of eax is " << *regEAX << endl; 
    int* addr = (int*)regEAX;
    *addr = 0;
    //OutFile << *(addr) << " " << *(addr + 1) << " " << *(addr + 2) << endl;
}

VOID OverwriteCarX(
    ADDRINT ip, 
    ADDRINT * regEDX) {
    //OutFile << "Car x is " << *regEDX << endl; 
    int* addr = (int*)regEDX;
    *addr = 1000;
    //OutFile << *(addr) << " " << *(addr + 1) << " " << *(addr + 2) << endl;
}

VOID OverwriteScore(
    ADDRINT ip, 
    ADDRINT * regEDI) {
    OutFile << "ADD Score " << *regEDI << endl; 
    int* addr = (int*)regEDI;
    OutFile << *(addr) << " " << *(addr + 1) << " " << *(addr + 2) << endl;
    *addr = 100 * (*addr);
}

VOID RewriteCrashDetected(VOID * ip, VOID * addr, UINT32 size) {
    OutFile << "size is " << size << endl;
    unsigned char* p = (unsigned char*)addr;
    OutFile << "Crash Detected: ";
    for (UINT32 i = 0; i < size; i++) {
        *p = 0;
        OutFile << *p << " ";
        p++;
    }
    OutFile << endl;
}


INT32 Usage() {
    cerr << "This tool reads the mine positions" << endl;
    return -1;
}

VOID Fini(INT32 code, VOID *v) {
    OutFile.setf(ios::showbase);
    
    OutFile.close();
}

VOID Instruction(INS ins, VOID *v) {
    if (!g_bMainExecLoaded) {
        return;
    }

    string strInst = INS_Disassemble(ins);
    ADDRINT addr = INS_Address(ins);

    if (!(addr >= g_addrLow && addr <= g_addrHigh)) {
        return;
    }

    ADDRINT relativeAddr = addr - g_addrLow;

    OutFile << "[" << setbase(16) << relativeAddr << "] " << strInst << endl;

    switch (relativeAddr) {
        case READ_HAS_GROUND:
            INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)OverwriteHasGround, IARG_INST_PTR, 
                    IARG_REG_REFERENCE, REG_EAX, 
                    IARG_END); 
            break;
        /* case WRITE_HIT_METEOR_HIT:
            INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)OverwriteHitMeteor, IARG_INST_PTR, 
                    IARG_REG_REFERENCE, REG_EAX, 
                    IARG_END); 
            break; */
        /*case WRITE_CRASH_DETECTED:
            UINT32 memOperands = INS_MemoryOperandCount(ins);
            for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
                if (INS_MemoryOperandIsWritten(ins, memOp)) {
                    OutFile << "operation is write" << endl;
                    INS_InsertCall(
                        ins, IPOINT_AFTER, (AFUNPTR)RewriteCrashDetected,
                        IARG_INST_PTR,
                        IARG_MEMORYOP_EA, memOp,
                        IARG_MEMORYWRITE_SIZE,
                    IARG_END);
                }
            }
            break;*/
        case READ_CAR_X:
            INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)OverwriteCarX, IARG_INST_PTR, 
                    IARG_REG_REFERENCE, REG_EDX, 
                    IARG_END); 
            break;
        case WRITE_SCORE:
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)OverwriteScore, IARG_INST_PTR, 
                    IARG_REG_REFERENCE, REG_EDI, 
                    IARG_END); 
            break;
    }
    
}


int main(int argc, char * argv[]) {
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open("log.txt");

    INS_AddInstrumentFunction(Instruction, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_AddFiniFunction(Fini, 0);
    
    PIN_StartProgram();
    
    return 0;
}