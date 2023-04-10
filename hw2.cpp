#include <iostream>
#include <fstream>
#include "pin.H"
#include <vector>
using namespace std;

ofstream OutFile;
ofstream ResultFile;

static ADDRINT g_addrLow, g_addrHigh;
static BOOL g_bMainExecLoaded = FALSE;

static BOOL printFieldReturn = FALSE;
static std::vector<int> hasMines;
static int lines = 0;

const ADDRINT READ_CELL_VAL_1 = 0x1846;
const ADDRINT READ_CELL_VAL_2 = 0x1864;
const ADDRINT READ_CELL_VAL_3 = 0x1870; // right
const ADDRINT READ_CELL_VAL_4 = 0x188e;

const ADDRINT PRINT_FIELD_RETURN = 0x183d;
const ADDRINT PRINT_NEW_LINE = 0x1822;

VOID ImageLoad(IMG img, VOID *v) {
    if( IMG_IsMainExecutable(img) ) {
        g_addrLow = IMG_LowAddress(img); 
        g_addrHigh = IMG_HighAddress(img);
        g_bMainExecLoaded = TRUE;
        OutFile << "Main image from " << g_addrLow << " to " << g_addrHigh << endl;
    }
}

VOID CollectInfo(
    ADDRINT ip, 
    ADDRINT * regRAX,
    ADDRINT * regRBP) {
    int val = *(int*)(*regRBP + 0x18);
    OutFile << "val: " << setbase(16) <<  val << endl; 
    if (!printFieldReturn) {
        hasMines.push_back(val);
    }
}

VOID OutputResult(
    ADDRINT ip) {
    if(!printFieldReturn) {
        ResultFile.open("result.txt");
        int sizeInLine = hasMines.size() / lines;
        for (int i = 0; i < lines; i++) {
            for (int j = 0; j < sizeInLine; j++) {
                ResultFile << hasMines[i * sizeInLine + j];
            }
            ResultFile << "\n";
        }
        ResultFile.close();
    }
    OutFile << "\n" << lines << "\n";
    printFieldReturn = TRUE;
}

VOID CountLines(
    ADDRINT ip) {
    if(!printFieldReturn) {
        ++lines;
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

    switch (relativeAddr)
    {
    case READ_CELL_VAL_1:
        INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)CollectInfo, IARG_INST_PTR, 
                    IARG_REG_REFERENCE, REG_RAX, 
                    IARG_REG_REFERENCE, REG_RBP, 
                    IARG_END); 
        break;
    case PRINT_FIELD_RETURN:
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)OutputResult, IARG_INST_PTR, 
                    IARG_END); 
        break;
    case PRINT_NEW_LINE:
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountLines, IARG_INST_PTR, 
                    IARG_END); 
        break;
    default:
        break;
    }
    
}

INT32 Usage() {
    cerr << "This tool reads the mine positions" << endl;
    return -1;
}

VOID Fini(INT32 code, VOID *v) {
    OutFile.setf(ios::showbase);
    
    OutFile.close();
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