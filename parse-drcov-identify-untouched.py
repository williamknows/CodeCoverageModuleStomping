#!/usr/bin/python3

import csv
from prettytable import PrettyTable
import sys

if len(sys.argv) <= 1:
    print("\nGrab code coverage data with DynamoRIO's drcov:\n")
    print("bin64/drrun.exe -t drcov -dump_text -- notepad")
    print("\nOutputs *.log which can be passed here as an argument.\n")
    print("python3 parse-drcov-untouched.py file.log")
    print("\nCode is terrible, and commonly takes 5+ minutes to run, so it requires a bit of patience.\n")
    sys.exit(1)

with open(sys.argv[1], "r") as inputFileRaw:
    inputFile = inputFileRaw.read().splitlines()

startCollectingModules = False
startCollectingExecution = False
modulesList = []
modulesDict = {}
modulesExecution = {}
moduleStartAddress = {}
modulePathLocation = {}

for line in inputFile:
    if "Columns: id, containing_id, start, end, entry, offset, checksum, timestamp, path" in line:
        modulesList.append(line[9:].replace(" ","").lstrip().rstrip())
        startCollectingModules = True
        continue 
    
    if startCollectingModules:
        if "BB Table" in line:
            startCollectingModules=False
            break
        modulesList.append(line.replace(" ","").lstrip().rstrip()) # note that this may mess up the path if it has spaces in it

modulesDict = csv.DictReader(modulesList)

print("Generating dict to track byte-by-byte execution in each module.")

for module in modulesDict:
    modulesExecution[module["id"]] = {}

    for i in range(0, int(module["end"], 0)-int(module["start"], 0)):
        modulesExecution[module["id"]][str("{0:#0{1}x}".format(i,18)).lower()] = 0

    moduleStartAddress[module["id"]] = module["start"]
    modulePathLocation[module["id"]] = module["path"]

print("Analysing execution.")

lineCount=0
for line in inputFile:
    lineCount+=1

    if "module id, start, size:" in line:
        startCollectingExecution = True
        continue # skip to next line
    
    if startCollectingExecution:
        moduleNumber = line.split(":")[0].split("[")[1].split("]")[0].lstrip().rstrip()
        executionLocation = line.split(":")[1].split(",")[0].lstrip().rstrip()
        executionSize = line.split(":")[1].split(",")[1].lstrip().rstrip()

        for i in range(int(executionLocation, 0), int(executionLocation, 0) + int(executionSize) + 1): # + 1 just to be sure
            try:
                addressWithBase = str("{0:#0{1}x}".format(i,18)).lower()
                if addressWithBase in modulesExecution[moduleNumber]:
                    modulesExecution[moduleNumber][addressWithBase] = 1
            except Exception as e: 
                print(f"Exception occurred when adding touched address to dict: {e}")
                print(f"Input file line: {lineCount}")
                print(f"addressWithBase: {addressWithBase}")
                print(f"moduleNumber: {moduleNumber}")


x = PrettyTable()
x.field_names = ["Path", "Touched (Bytes)", "Untouched (Bytes)", "Coverage (%)", "Largest Untouched (Bytes)", "Largest Untouched (Offset Bytes)"]

for moduleKey,moduleValue in modulesExecution.items():
    if "DynamoRIO".lower() in modulePathLocation[moduleKey].lower():
        continue

    try:
        untouchedBytes=0
        touchedBytes=0
        for addressLocation,addressUsed in moduleValue.items():
            if int(addressUsed) == 0:
                untouchedBytes+=1
            if int(addressUsed) == 1:
                touchedBytes+=1

        currentUntouchedCount=0
        currentUntouchedBaseAddress="0x0000000000000000"
        largestUntouchedCount=0
        largestUntouchedBaseAddress="0x0000000000000000"
        lastAddressWasOneOrFirst=True
        for addressLocation,addressUsed in moduleValue.items():
            if int(addressUsed) == 0:
                currentUntouchedCount+=1
                if lastAddressWasOneOrFirst:
                    currentUntouchedBaseAddress=addressLocation
                lastAddressWasOneOrFirst=False
            else:
                if currentUntouchedCount > largestUntouchedCount:
                    largestUntouchedCount = currentUntouchedCount
                    largestUntouchedBaseAddress = currentUntouchedBaseAddress
                currentUntouchedCount=0
                lastAddressWasOneOrFirst=True

        # handle situation where nothing is touched (== the entire binary)
        if touchedBytes==0:
            largestUntouchedCount=untouchedBytes

        # OPTIONAL (can be removed): make sure the largest offset is at a 16 byte offset from the beginning of he file
        amountAdded=0
        largestUntouchedBaseAddress = int(largestUntouchedBaseAddress, 16)
        while (largestUntouchedBaseAddress % 16 != 0):
            largestUntouchedBaseAddress+=1
            amountAdded+=1
        largestUntouchedCount=largestUntouchedCount-amountAdded # removed what's added to reflect smaller untouched buffer

        #largestUntouchedBaseAddressFull = str("{0:#0{1}x}".format(int(largestUntouchedBaseAddress, 16),18)).lower()
        codeCoveragePercentage = format(touchedBytes/untouchedBytes*100, ".3f")

        # Module-by-module output (non-tabular)
        # print("************************")
        # print(f"Module path location: {modulePathLocation[moduleKey]}")
        # print(f"Memory addresses touched (bytes)): {str(touchedBytes)} ({str(size(touchedBytes))})")
        # print(f"Memory addresses untouched (bytes)): {str(untouchedBytes)} ({str(size(untouchedBytes))})")
        # print(f"Code coverage (%): {str(codeCoveragePercentage)}")
        # print(f"Largest untouched space (bytes): {str(largestUntouchedCount)} ({str(size(largestUntouchedCount))})")
        # print(f"Largest untouched space (base address): {str(largestUntouchedBaseAddressFull)}")

        x.add_row([modulePathLocation[moduleKey], touchedBytes, untouchedBytes, codeCoveragePercentage, largestUntouchedCount, largestUntouchedBaseAddress])
    
    except Exception as e: 
        print(f"Exception occurred when analysing module: {e}")
        print(f"largestUntouchedBaseAddress: {largestUntouchedBaseAddress}")
        print(f"largestUntouchedCount: {largestUntouchedCount}")
        print(f"moduleNumber: {moduleKey}")

# print table output
x.sortby = "Largest Untouched (Bytes)"
x.reversesort = True
print(x)
