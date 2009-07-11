/*
 * Copyright 2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


#include "DwarfImageDebugInfo.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <new>

#include <AutoDeleter.h>
#include <AutoLocker.h>

#include "Architecture.h"
#include "CompilationUnit.h"
#include "DebugInfoEntries.h"
#include "Dwarf.h"
#include "DwarfFile.h"
#include "DwarfFunctionDebugInfo.h"
#include "DwarfUtils.h"
#include "ElfFile.h"
#include "FileManager.h"
#include "FileSourceCode.h"
#include "LocatableFile.h"
#include "SourceFile.h"
#include "Statement.h"
#include "StringUtils.h"


DwarfImageDebugInfo::DwarfImageDebugInfo(const ImageInfo& imageInfo,
	Architecture* architecture, FileManager* fileManager, DwarfFile* file)
	:
	fLock("dwarf image debug info"),
	fImageInfo(imageInfo),
	fArchitecture(architecture),
	fFileManager(fileManager),
	fFile(file),
	fTextSegment(NULL),
	fRelocationDelta(0)
{
}


DwarfImageDebugInfo::~DwarfImageDebugInfo()
{
}


status_t
DwarfImageDebugInfo::Init()
{
	status_t error = fLock.InitCheck();
	if (error != B_OK)
		return error;

	fTextSegment = fFile->GetElfFile()->TextSegment();
	if (fTextSegment == NULL)
		return B_ENTRY_NOT_FOUND;

	fRelocationDelta = fImageInfo.TextBase() - fTextSegment->LoadAddress();

	return B_OK;
}


status_t
DwarfImageDebugInfo::GetFunctions(BObjectList<FunctionDebugInfo>& functions)
{
printf("DwarfImageDebugInfo::GetFunctions()\n");
printf("  %ld compilation units\n", fFile->CountCompilationUnits());

	for (int32 i = 0; CompilationUnit* unit = fFile->CompilationUnitAt(i);
			i++) {
		DIECompileUnitBase* unitEntry = unit->UnitEntry();
//		printf("  %s:\n", unitEntry->Name());
//		printf("    address ranges:\n");
//		TargetAddressRangeList* rangeList = unitEntry->AddressRanges();
//		if (rangeList != NULL) {
//			int32 count = rangeList->CountRanges();
//			for (int32 i = 0; i < count; i++) {
//				TargetAddressRange range = rangeList->RangeAt(i);
//				printf("      %#llx - %#llx\n", range.Start(), range.End());
//			}
//		} else {
//			printf("      %#llx - %#llx\n", (target_addr_t)unitEntry->LowPC(),
//				(target_addr_t)unitEntry->HighPC());
//		}

//		printf("    functions:\n");
		for (DebugInfoEntryList::ConstIterator it
					= unitEntry->OtherChildren().GetIterator();
				DebugInfoEntry* entry = it.Next();) {
			if (entry->Tag() != DW_TAG_subprogram)
				continue;

			DIESubprogram* subprogramEntry = static_cast<DIESubprogram*>(entry);

			// ignore declarations, prototypes, and inlined functions
			if (subprogramEntry->IsDeclaration()
				|| subprogramEntry->IsPrototyped()
				|| subprogramEntry->Inline() == DW_INL_inlined
				|| subprogramEntry->Inline() == DW_INL_declared_inlined) {
				continue;
			}

			// get the name
			BString name;
			DwarfUtils::GetFullyQualifiedDIEName(subprogramEntry, name);
			if (name.Length() == 0)
				continue;

			// get the address ranges
			TargetAddressRangeList* rangeList
				= subprogramEntry->AddressRanges();
			Reference<TargetAddressRangeList> rangeListReference(rangeList);
			if (rangeList == NULL) {
				target_addr_t lowPC = subprogramEntry->LowPC();
				target_addr_t highPC = subprogramEntry->HighPC();
				if (lowPC >= highPC)
					continue;

				rangeList = new(std::nothrow) TargetAddressRangeList(
					TargetAddressRange(lowPC, highPC - lowPC));
				if (rangeList == NULL)
					return B_NO_MEMORY;
						// TODO: Clean up already added functions!
				rangeListReference.SetTo(rangeList, true);
			}

			// get the source location
			const char* directoryPath = NULL;
			const char* fileName = NULL;
			int32 line = -1;
			int32 column = -1;
			DwarfUtils::GetDeclarationLocation(fFile, subprogramEntry,
				directoryPath, fileName, line, column);

			LocatableFile* file = NULL;
			if (fileName != NULL) {
				file = fFileManager->GetSourceFile(directoryPath,
					fileName);
			}
			Reference<LocatableFile> fileReference(file, true);

			// create and add the functions
			DwarfFunctionDebugInfo* function
				= new(std::nothrow) DwarfFunctionDebugInfo(this, unit,
					subprogramEntry, rangeList, name, file,
					SourceLocation(line, std::max(column, 0L)));
			if (function == NULL || !functions.AddItem(function)) {
				delete function;
				return B_NO_MEMORY;
					// TODO: Clean up already added functions!
			}

//			BString name;
//			DwarfUtils::GetFullyQualifiedDIEName(subprogramEntry, name);
//			printf("      subprogram entry: %p, name: %s, declaration: %d\n",
//				subprogramEntry, name.String(),
//				subprogramEntry->IsDeclaration());
//
//			rangeList = subprogramEntry->AddressRanges();
//			if (rangeList != NULL) {
//				int32 count = rangeList->CountRanges();
//				for (int32 i = 0; i < count; i++) {
//					TargetAddressRange range = rangeList->RangeAt(i);
//					printf("        %#llx - %#llx\n", range.Start(), range.End());
//				}
//			} else {
//				printf("        %#llx - %#llx\n",
//					(target_addr_t)subprogramEntry->LowPC(),
//					(target_addr_t)subprogramEntry->HighPC());
//			}
		}
	}

	return B_OK;
}


status_t
DwarfImageDebugInfo::CreateFrame(Image* image, FunctionDebugInfo* function,
	CpuState* cpuState, StackFrame*& _previousFrame,
	CpuState*& _previousCpuState)
{
	// TODO:...
	return B_UNSUPPORTED;
}


status_t
DwarfImageDebugInfo::GetStatement(FunctionDebugInfo* _function,
	target_addr_t address, Statement*& _statement)
{
printf("DwarfImageDebugInfo::GetStatement(function: %p, address: %#llx)\n",
_function, address);
	DwarfFunctionDebugInfo* function
		= dynamic_cast<DwarfFunctionDebugInfo*>(_function);
	if (function == NULL)
{
printf("  -> no dwarf function\n");
		return B_BAD_VALUE;
}

	AutoLocker<BLocker> locker(fLock);

	// check whether we have the source code
	CompilationUnit* unit = function->GetCompilationUnit();
	LocatableFile* file = function->SourceFile();
	if (file == NULL) {
printf("  -> no source file\n");
		// no source code -- rather return the assembly statement
		return fArchitecture->GetStatement(function, address, _statement);
	}

	// get the index of the source file in the compilation unit for cheaper
	// comparison below
	int32 fileIndex = _GetSourceFileIndex(unit, file);

	// Get the statement by executing the line number program for the
	// compilation unit.
	LineNumberProgram& program = unit->GetLineNumberProgram();
	if (!program.IsValid())
{
printf("  -> no line number program\n");
		return B_BAD_DATA;
}

	// adjust address
	address -= fRelocationDelta;

	LineNumberProgram::State state;
	program.GetInitialState(state);

	target_addr_t statementAddress = 0;
	int32 statementLine = -1;
	int32 statementColumn = -1;
	while (program.GetNextRow(state)) {
		bool isOurFile = state.file == fileIndex;

		if (statementAddress != 0
			&& (!isOurFile || state.isStatement || state.isSequenceEnd)) {
			target_addr_t endAddress = state.address;
			if (address >= statementAddress && address < endAddress) {
				ContiguousStatement* statement = new(std::nothrow)
					ContiguousStatement(
						SourceLocation(statementLine, statementColumn),
						TargetAddressRange(fRelocationDelta + statementAddress,
							endAddress - statementAddress));
				if (statement == NULL)
					return B_NO_MEMORY;

				_statement = statement;
				return B_OK;
			}

			statementAddress = 0;
		}

		// skip statements of other files
		if (!isOurFile)
			continue;

		if (state.isStatement) {
			statementAddress = state.address;
			statementLine = state.line - 1;
			statementColumn = std::max(state.column - 1, 0L);
		}
	}

printf("  -> no line number program match\n");
	return B_ENTRY_NOT_FOUND;
}


status_t
DwarfImageDebugInfo::GetStatementAtSourceLocation(FunctionDebugInfo* _function,
	const SourceLocation& sourceLocation, Statement*& _statement)
{
	DwarfFunctionDebugInfo* function
		= dynamic_cast<DwarfFunctionDebugInfo*>(_function);
	if (function == NULL)
		return B_BAD_VALUE;
target_addr_t functionStartAddress = function->Address() - fRelocationDelta;
target_addr_t functionEndAddress = functionStartAddress + function->Size();
printf("DwarfImageDebugInfo::GetStatementAtSourceLocation(%p, (%ld, %ld)): function range: %#llx - %#llx\n",
function, sourceLocation.Line(), sourceLocation.Column(), functionStartAddress, functionEndAddress);

	AutoLocker<BLocker> locker(fLock);

	// get the source file
	LocatableFile* file = function->SourceFile();
	if (file == NULL)
		return B_ENTRY_NOT_FOUND;

	CompilationUnit* unit = function->GetCompilationUnit();

	// get the index of the source file in the compilation unit for cheaper
	// comparison below
	int32 fileIndex = _GetSourceFileIndex(unit, file);

//	target_addr_t functionStartAddress = function->Address() - fRelocationDelta;
//	target_addr_t functionEndAddress = functionStartAddress + function->Size();

	// Get the statement by executing the line number program for the
	// compilation unit.
	LineNumberProgram& program = unit->GetLineNumberProgram();
	if (!program.IsValid())
		return B_BAD_DATA;

	LineNumberProgram::State state;
	program.GetInitialState(state);

	target_addr_t statementAddress = 0;
	int32 statementLine = -1;
	int32 statementColumn = -1;
	while (program.GetNextRow(state)) {
		bool isOurFile = state.file == fileIndex;

		if (statementAddress != 0
			&& (!isOurFile || state.isStatement || state.isSequenceEnd)) {
			target_addr_t endAddress = state.address;
if (statementAddress < endAddress) {
printf("  statement: %#llx - %#llx, location: (%ld, %ld)\n", statementAddress, endAddress, statementLine, statementColumn);
}
			if (statementAddress < endAddress
				&& statementAddress >= functionStartAddress
				&& statementAddress < functionEndAddress
				&& statementLine == (int32)sourceLocation.Line()
				&& statementColumn == (int32)sourceLocation.Column()) {
printf("  -> found statement!\n");
				ContiguousStatement* statement = new(std::nothrow)
					ContiguousStatement(
						SourceLocation(statementLine, statementColumn),
						TargetAddressRange(fRelocationDelta + statementAddress,
							endAddress - statementAddress));
				if (statement == NULL)
					return B_NO_MEMORY;

				_statement = statement;
				return B_OK;
			}

			statementAddress = 0;
		}

		// skip statements of other files
		if (!isOurFile)
			continue;

		if (state.isStatement) {
			statementAddress = state.address;
			statementLine = state.line - 1;
			statementColumn = std::max(state.column - 1, 0L);
		}
	}

	return B_ENTRY_NOT_FOUND;
}


ssize_t
DwarfImageDebugInfo::ReadCode(target_addr_t address, void* buffer, size_t size)
{
	target_addr_t offset = address - fRelocationDelta
		- fTextSegment->LoadAddress() + fTextSegment->FileOffset();
	ssize_t bytesRead = pread(fFile->GetElfFile()->FD(), buffer, size, offset);
	return bytesRead >= 0 ? bytesRead : errno;
}


status_t
DwarfImageDebugInfo::AddSourceCodeInfo(LocatableFile* file,
	FileSourceCode* sourceCode)
{
	bool addedAny = false;
	for (int32 i = 0; CompilationUnit* unit = fFile->CompilationUnitAt(i);
			i++) {
		int32 fileIndex = _GetSourceFileIndex(unit, file);
		if (fileIndex < 0)
			continue;

		status_t error = _AddSourceCodeInfo(unit, sourceCode, fileIndex);
		if (error == B_NO_MEMORY)
			return error;
		addedAny |= error == B_OK;
	}

	return addedAny ? B_OK : B_ENTRY_NOT_FOUND;
}


status_t
DwarfImageDebugInfo::_AddSourceCodeInfo(CompilationUnit* unit,
	FileSourceCode* sourceCode, int32 fileIndex)
{
	// Get the statements by executing the line number program for the
	// compilation unit and filtering the rows for our source file.
	LineNumberProgram& program = unit->GetLineNumberProgram();
	if (!program.IsValid())
		return B_BAD_DATA;

	LineNumberProgram::State state;
	program.GetInitialState(state);

	target_addr_t statementAddress = 0;
	int32 statementLine = -1;
	int32 statementColumn = -1;
	while (program.GetNextRow(state)) {
printf("  %#lx  (%ld, %ld, %ld)  %d\n", state.address, state.file, state.line, state.column, state.isStatement);
		bool isOurFile = state.file == fileIndex;

		if (statementAddress != 0
			&& (!isOurFile || state.isStatement || state.isSequenceEnd)) {
			target_addr_t endAddress = state.address;
			if (endAddress > statementAddress) {
				// add the statement
				status_t error = sourceCode->AddSourceLocation(
					SourceLocation(statementLine, statementColumn));
				if (error != B_OK)
					return error;
printf("  -> statement: %#llx - %#llx, source location: (%ld, %ld)\n", statementAddress, endAddress, statementLine, statementColumn);
			}

			statementAddress = 0;
		}

		// skip statements of other files
		if (!isOurFile)
			continue;

		if (state.isStatement) {
			statementAddress = state.address;
			statementLine = state.line - 1;
			statementColumn = std::max(state.column - 1, 0L);
		}
	}

	return B_OK;
}


int32
DwarfImageDebugInfo::_GetSourceFileIndex(CompilationUnit* unit,
	LocatableFile* sourceFile) const
{
	// get the index of the source file in the compilation unit for cheaper
	// comparison below
	const char* directory;
	for (int32 i = 0; const char* fileName = unit->FileAt(i, &directory); i++) {
		LocatableFile* file = fFileManager->GetSourceFile(directory, fileName);
		if (file != NULL) {
			file->ReleaseReference();
			if (file == sourceFile) {
				return i + 1;
					// indices are one-based
			}
		}
	}

	return -1;
}
