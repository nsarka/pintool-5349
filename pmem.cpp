#include <iostream>
#include <map>
#include <set>
#include <sys/time.h>
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <execinfo.h>

#include "pin.H"

using std::cout;
using std::endl;
using std::string;
using std::map;
using std::set;
using std::pair;

string open_rtn_name = "open";
string clflush_rtn_name = "clflush";

KNOB< string > OpenKnobFunctionNameToInstrument(KNOB_MODE_WRITEONCE, "pintool", "open_function_name", "open",
                                            "function name to instrument");
KNOB< string > ClflushKnobFunctionNameToInstrument(KNOB_MODE_WRITEONCE, "pintool", "clflush_function_name", "clflush",
                                            "function name to instrument");

// map for persistent write address : backtrace
map<ADDRINT, string> m;

// set of pairs<start address, end address> of mmap'd pmem pools
set<pair<ADDRINT, ADDRINT>> s;

int AfterPoolOpen()
{
   FILE *fp;
   void * start, *end;
   int ret;

   fp = fopen ("/tmp/pmem.txt", "r");

   if(fp == NULL) {
	cout << "problem opening pmem.txt" << endl;
	return -1;
   }

   fscanf(fp, "Pmem=%p:%p", &start, &end);
   fclose(fp);
   ret = remove("/tmp/pmem.txt");

   if(ret == 0) {
      printf("File deleted successfully\n");
   } else {
      printf("Error: unable to delete the file\n");
   }

   cout << "Pintool pmem is " << (void*)start << ":" << (void*)end << endl;

   s.insert(pair<ADDRINT, ADDRINT>((ADDRINT)start, (ADDRINT)end));

   return 0;
}

int AfterClflush()
{
	cout << "corundum called clflush" << endl;
	return 0;
}

VOID ImageLoad(IMG img, VOID* v)
{
    // corundum is statically linked, so skip everything but the application binary
    if(!IMG_IsMainExecutable(img)) return;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            //if (RTN_Name(rtn).compare(rtn_name) == 0 || RTN_Name(rtn).compare("__" + rtn_name) == 0)
            if (RTN_Name(rtn).find(open_rtn_name) != string::npos) // contains substring
            {
		cout << "Found " << RTN_Name(rtn).c_str() << " in " << IMG_Name(img) << endl;

                ASSERTX(RTN_Valid(rtn));

                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(AfterPoolOpen), IARG_END);
                RTN_Close(rtn);
            } else if(RTN_Name(rtn).find(clflush_rtn_name) != string::npos) {
		cout << "Found " << RTN_Name(rtn).c_str() << " in " << IMG_Name(img) << endl;
                ASSERTX(RTN_Valid(rtn));

                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(AfterClflush), IARG_END);
                RTN_Close(rtn);
	    }
        }
    }
}

string get_backtrace(const CONTEXT* ctxt) {

	string s = "\n";

        void* buf[128];
        PIN_LockClient();
        int nptrs = PIN_Backtrace(ctxt, buf, sizeof(buf) / sizeof(buf[0]));
        ASSERTX(nptrs > 0);
        char** bt = backtrace_symbols(buf, nptrs);
        PIN_UnlockClient();
        ASSERTX(NULL != bt);
        for (int i = 0; i < nptrs; i++)
        {
	    s += "\t";
            s += bt[i];
            s += "\n";
        }
        free(bt);

	return s;
}

VOID RecordMemWrite(VOID* ip, VOID* addr, const CONTEXT* ctxt)
{
    // Just record writes to addresses we know are pmem
    for(auto ele : s) {
        if(ele.first <= (ADDRINT) addr && ele.second >= (ADDRINT) addr) {
             printf("persistent write to address %p\n", addr);
	     string backtrace = get_backtrace(ctxt);

             // Check to see if there was an entry already with this key
	     auto it = m.find((ADDRINT)addr);
	     if (it != m.end()) {
                 cout << "Write to pmem on address already existing in the map: " << (void*)(it->first) << "\n" << it->second << endl;
                 m.erase((ADDRINT)addr);
             }

	     m.insert(pair<ADDRINT, string>((ADDRINT)addr, backtrace));
        }
    }
}

VOID flush(VOID* addr)
{
    cout << "clflush called on address " << (void*)(addr) << endl;
    auto it = m.find((ADDRINT)addr);
    if (it == m.end()) {
        cout << "clflush called with address not stored in the map: " << (void*)(addr) << endl;
    } else {
        // Remove entry from the map for this address since it will be clflush'd after this function returns
        m.erase(it);
    }
}

VOID Instruction(INS ins, VOID* v)
{
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    if (INS_MemoryOperandCount(ins) == 0) return;

    UINT32 memOp = 0;

    // If this instruction is a clflush or clflushopt, instrument it
    OPCODE op = INS_Opcode(ins);
    if(op == XED_ICLASS_CLFLUSHOPT || op == XED_ICLASS_CLFLUSH) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)flush, IARG_MEMORYOP_EA, memOp, IARG_END);
	return;
    }

    // Otherwise instrument memory writes
    if (!INS_IsStandardMemop(ins)) return;

    // Iterate over each memory operand of the instruction.
    for (memOp = 0; memOp < memOperands; memOp++)
    {
        // Note that in some architectures a single memory operand can be
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_CONST_CONTEXT,
                                     IARG_END);
        }
    }

}

VOID Fini(INT32 code, VOID *v)
{
    cout << "\nFinished application.\n\n" << endl;

    cout << "Pool address ranges captured:" << endl << endl;

    for(auto it = s.begin(); it != s.end(); it++) {
        cout << (void*)(it->first) << " : " << (void*)(it->second) << endl;
    }

    cout << "Backtraces of writes that did not have a corresponding clflush:" << endl;

    for(auto it = m.begin(); it != m.end(); it++) {
        cout << (void*)(it->first) << " : " << it->second << endl;
	cout << endl;
    }
}

int main(INT32 argc, CHAR* argv[])
{
    // Initialize pin
    //
    if (PIN_Init(argc, argv)) return 0;

    // Initialize symbol processing
    //
    PIN_InitSymbolsAlt(IFUNC_SYMBOLS);

   int ret = remove("/tmp/pmem.txt");

   if(ret == 0) {
      printf("File existed before execution and deleted successfully\n");
   }

    //Initialize global variables
    open_rtn_name = OpenKnobFunctionNameToInstrument.Value();
    cout << "open_rtn_name : " << open_rtn_name << endl;

    clflush_rtn_name = ClflushKnobFunctionNameToInstrument.Value();
    cout << "clflush_rtn_name : " << clflush_rtn_name << endl;

    // Register ImageLoad to be called when an image is loaded
    //
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Instrument every instruction to record memory writes
    INS_AddInstrumentFunction(Instruction, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    //
    PIN_StartProgram();

    return 0;
}
