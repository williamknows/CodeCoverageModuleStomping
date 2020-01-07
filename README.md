# CodeCoverageModuleStomping

Tools to support code coverage based module stomping.  Based on the blog post: http://williamknowles.io/?p=14

parse-drcov-identify-untouched.py - analyses DynamoRIO's drcov output.  Run the script as follows, with the argument being the drcov output file.

```
parse-drcov-identify-untouched.py drcov.mspaint.exe.11520.0000.proc-win10-beacon.log
```

CodeCoverageModuleStomping - a simple C++ project for testing injecting into the memory regions of an already loaded module (DLL) at a particular offset.  Shellcode should be included in the only header file of the project.  It's designed for testing on Windows 10 and sets up call targets for Control Flow Guard (CFG); if you want to run this on older operating systems you'll probably need to comment this section of code out.  Run the compiled binary as follows:

```
CodeCoverageMiniStompInjection.exe <program-to-start-to-inject-into> <module-name-to-inject-into> <offset-bytes-into-module>
```

For example:

```
CodeCoverageMiniStompInjection.exe mspaint.exe combase.dll 1599552
```
