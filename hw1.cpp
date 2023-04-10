#include <iostream>
#include <fstream>
#include "pin.H"
using namespace std;

const ADDRINT CALL_ADDR1 = 0x1c65;
const ADDRINT CALL_ADDR2 = 0x1d0b;

const ADDRINT WRITE_IS_OVER_ADDR = 0x1d9a;

ofstream OutFile;
static UINT64 icount = 0;
static ADDRINT g_addrLow, g_addrHigh;
static BOOL g_bMainExecLoaded = FALSE;

VOID docount() { icount++; }


VOID ImageLoad(IMG img, VOID *v) {
    if( IMG_IsMainExecutable(img) ) {
        g_addrLow = IMG_LowAddress(img); 
        g_addrHigh = IMG_HighAddress(img);
        g_bMainExecLoaded = TRUE;
        OutFile << "Main image from " << g_addrLow << " to " << g_addrHigh << endl;
    }
}

VOID EveryInst(
    ADDRINT ip, 
    ADDRINT * regRAX, 
    ADDRINT * regRBX,
    ADDRINT * regRCX, 
    ADDRINT * regRDX) {
    OutFile << "ip is " << setbase(16) << ip << " old value of eax is " << *regRAX << endl; 
    *regRAX = 0;
}

VOID tryChangeCollisionReturnvalue(ADDRINT relativeAddr, INS ins) {
    if (relativeAddr == CALL_ADDR1 || relativeAddr == CALL_ADDR2) {
        OutFile << "insert call before " << relativeAddr << endl;
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EveryInst, IARG_INST_PTR, 
                    IARG_REG_REFERENCE, REG_RAX, 
                    IARG_REG_REFERENCE, REG_RBX, 
                    IARG_REG_REFERENCE, REG_RCX, 
                    IARG_REG_REFERENCE, REG_RDX, 
                    IARG_END); 
    }
}

VOID RecordMemWriteAfter(VOID * ip, VOID * addr, UINT32 size) {
    OutFile << "size is " << size << endl;
    unsigned char* p = (unsigned char*)addr;
    for (UINT32 i = 0; i < size; i++) {
        *p = 0;
        p++;
    }
    
}

VOID tryOverwriteIsOver(ADDRINT relativeAddr, INS ins) {
    if (relativeAddr != WRITE_IS_OVER_ADDR) {
        return;
    }
    OutFile << "trying to change memory" << endl;
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
        if (INS_MemoryOperandIsWritten(ins, memOp)) {
            OutFile << "operation is write" << endl;
            INS_InsertCall(
                ins, IPOINT_AFTER, (AFUNPTR)RecordMemWriteAfter,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_MEMORYWRITE_SIZE,
                IARG_END);

        }
    }
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
    tryChangeCollisionReturnvalue(relativeAddr, ins);
    tryOverwriteIsOver(relativeAddr, ins);
}

VOID Fini(INT32 code, VOID *v) {
    OutFile.setf(ios::showbase);
    OutFile << "Count " << icount << endl;
    OutFile.close();
}

INT32 Usage() {
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    return -1;
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