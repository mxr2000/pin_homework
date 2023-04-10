#include "pin.H"
#include <iostream>
#include <fstream>


using namespace std;

unsigned short g_accessMap[0xFFFF];
ADDRINT g_addrLow, g_addrHigh;
BOOL g_bMainExecLoaded = FALSE;
FILE *g_fpLog = 0;
ofstream OutFile;

const ADDRINT ADDR_HEIGHT = 0x14dd;
const ADDRINT ADDR_WIDTH = 0x14e6;
const ADDRINT ADDR_IS_MINE = 0x1602;

int matrix_height = 0;
int matrix_width = 0;
vector<int> is_mines;
BOOL is_set_up_over = FALSE;


void log_init()
{
    g_fpLog = fopen("log.txt", "wt");
}

void log(const char *format, ...)
{
    if (g_fpLog == 0)
        log_init();

    va_list args;
    va_start(args, format);
    vfprintf(g_fpLog, format, args);
    va_end(args);
}

VOID ImageLoad(IMG img, VOID *v)
{
    if (IMG_IsMainExecutable(img))
    {
        g_addrLow = IMG_LowAddress(img);
        g_addrHigh = IMG_HighAddress(img);

        // Use the above addresses to prune out non-interesting instructions.
        g_bMainExecLoaded = TRUE;
        log("[IMG] Main Exec.: %lx ~%lx\n", IMG_LowAddress(img), IMG_HighAddress(img));
    }
    else
    {
        log("[IMG] Library: %lx ~%lx\n", IMG_LowAddress(img), IMG_HighAddress(img));
    }
}

int IsStackMem_Heuristic(ADDRINT rsp, ADDRINT mem)
{   
    if ((rsp - 0x10000) < mem && mem < (rsp + 0x10000))
    {
        return 1;
    }
    return 0;
}

int LogData(VOID *addr, UINT32 size)
{
    switch (size)
    {
    case 4:
    {
        unsigned int *pData = (unsigned int *)addr;
        log("%d\n", *pData);
        return *pData;
    }
    case 8:
    {
        unsigned long int *pData = (unsigned long int *)addr;
        log("%lld\n", *pData);
        return *pData;
    }
    default:
    {
        unsigned char *pData = (unsigned char *)addr;
        for (unsigned int i = 0; i < size; i++, pData++)
        {
            log("%02x ", (unsigned char)*pData);
        }
        log("\n");
    }
    break;
    }
    return 0;
}

VOID OutputResult() {
    for (int i = 0; i < matrix_height; i++) {
        for (int j = 0; j < matrix_width; j++) {
            OutFile << is_mines[matrix_width * i + j];
        }
        OutFile << endl;
    }
}

VOID RecordMemWriteAfter(VOID *ip, VOID *addr, UINT32 size, ADDRINT *regRSP)
{
    ADDRINT offset = (ADDRINT)ip - g_addrLow;
    if (IsStackMem_Heuristic(*regRSP, (ADDRINT)addr))
        return;

    g_accessMap[offset]++;

    log("[MEMWRITE(AFTER)] %p, (hitcount: %d), mem: %p(sz: %d)(stack:%p)->", offset, g_accessMap[offset], addr, size, *regRSP);
    int val = LogData(addr, size);

    if (is_set_up_over) {
        return;
    }
    switch (offset) {
        case ADDR_HEIGHT:
            matrix_height = val;
            log("Write height %d\n", val);
            break;
        case ADDR_WIDTH:
            matrix_width = val;
            log("Write width %d\n", val);
            break;
        case ADDR_IS_MINE:
            is_mines.push_back(val);
            log("Write is mine %d\n", val);
            break;
    }
    if (matrix_height != 0 && matrix_width != 0 && matrix_height * matrix_width == (int)is_mines.size()) {
        OutputResult();
        log("? %d %d\n", matrix_height, matrix_width);
        is_set_up_over = TRUE;
    }
}

VOID Instruction(INS ins, VOID *v)
{
    string strInst = INS_Disassemble(ins);
    ADDRINT addr = INS_Address(ins);

    if (!g_bMainExecLoaded || !(g_addrLow <= addr && addr < g_addrHigh))
    {
        return;
    }

    ADDRINT offset = addr - g_addrLow;
    log("[Read/Parse/Translate] [%lx] %s\n", offset, strInst.c_str());
    const char *pszInst = strInst.c_str();
    if (strstr(pszInst, "push r") == pszInst)
    {
        return;
    }
    if (INS_IsCall(ins) || !INS_IsMemoryWrite(ins))
    {
        return;
    }
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_OperandIsImplicit(ins, memOp))
            continue;
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertCall(
                ins, IPOINT_AFTER, (AFUNPTR)RecordMemWriteAfter,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_MEMORYWRITE_SIZE,
                IARG_REG_REFERENCE, REG_RSP,
                IARG_END);
        }
    }
}

VOID Fini(INT32 code, VOID *v)
{
    for (int i = 0; i < 0xFFFF; i++)
    {
        if (g_accessMap[i])
        {
            log("offset: %x, max-hitcount:%d\n", i, g_accessMap[i]);
        }
    }


    OutFile.close();
}

INT32 Usage()
{
    cerr << "This tool reads the mine positions" << endl;
    return -1;
}

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }
    OutFile.open("result.txt");
    log_init();
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_StartProgram();

    return 0;
}
