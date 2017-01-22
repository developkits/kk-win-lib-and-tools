# kk_win_lib_and_tools

## kk-win-checker-memory-mismatch

check memory operation mismatch.

* target program
  * compile Release build.
  * compile /MD (use dll crt)
  * compile /Zi (generage debug info)
  * link /DEBUG (generate pdb)
  * disable expand inline function, global replacement.  
    compile /Ob0 (disable inline function expansion)  
    or control by #pragma optimize("",off) and #pragma optimize("",on) to global replacement


* target program 32bit

  launch monitor.
  
  > x64\kk-win-checker-memory-operation-mismatch-monitor.exe

  launch program.
  
  > cd "target current directory"  
  > x86\kk-win-dll-injection-cui kk-win-checker-memory-operation-mismatch-dll.dll *target.exe* *target's arguments*

* target program 64bit

  launch monitor.
  
  > x64\kk-win-checker-memory-operation-mismatch-monitor.exe

  launch program.
  
  > cd "target current directory"  
  > x64\kk-win-dll-injection-cui kk-win-checker-memory-operation-mismatch-dll.dll *target.exe* *target's arguments*

