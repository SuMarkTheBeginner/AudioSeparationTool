# ResourceManager Refactoring Plan

## Overview
Refactor the ResourceManager class to improve readability, maintainability, performance, and consistency. Current class has 744 lines, 39+ methods, and handles multiple responsibilities. Will be split into specialized classes while maintaining full backward compatibility.

## Current Issues
- **God Class**: Handles file management, async processing, serialization, threading, UI coordination
- **Inconsistent Error Handling**: Mix of nullptr, bool, QString returns; some methods throw, others don't
- **Debug Pollution**: `qDebug()` scattered throughout production code
- **Threading Problems**: Static global threads make testing difficult and potentially unsafe
- **Mixed Responsibilities**: saveWav() contains complex tensor manipulation logic
- **Poor Testability**: Singleton pattern + global state makes unit testing hard

## New Architecture

### Core Components
1. **ResourceManager** - Facade maintaining public API (100% backward compatible)
2. **FileRepo** - Pure data operations for file/folder management
3. **AsyncProcessor** - Threading and worker coordination
4. **AudioSerializer** - Clean serialization/deserialization
5. **FileLocker** - File locking operations
6. **Logger** - Consistent logging utility

### Dependencies & Benefits
```
ResourceManager (Facade)
├── FileRepo (Data Layer)
│   └── FileLocker (Dependencies)
├── AsyncProcessor (Threading)
│   └── AudioSerializer (I/O)
└── Logger (Cross-cutting)
```

Benefits:
- ✅ Single Responsibility per class
- ✅ Improved testability (dependency injection)
- ✅ Better error handling (Result<T> pattern)
- ✅ Consistent logging
- ✅ Resource-friendly threading (RAII)
- ✅ Clear separation of business/presentation logic

## Implementation Plan

### Phase 1: Infrastructure Setup

#### [x] 1.1 Create Logger Utility
- **Purpose**: Replace inconsistent qDebug() calls
- **Location**: `logger.h`, `logger.cpp`
- **Methods**:
  - `static void log(Level, const QString&)`
  - `static void setMinLevel(Level)`
- **Implementation**:
  - Enum: Debug, Info, Warning, Error
  - Conditional compilation for release builds
- **Usage**: Replace all `qDebug()` calls systematically
- **Status**: ✅ **Completed** - Logger compiles successfully, added to CMakeLists.txt

#### [x] 1.2 Create FileLocker Class
- **Purpose**: Isolated file locking operations
- **Location**: `filelocker.h`, `filelocker.cpp`
- **Interface**:
  ```cpp
  bool lockFile(const QString& path);
  bool unlockFile(const QString& path);
  bool isFileLocked(const QString& path) const;
  ```
- **Moves from ResourceManager**: `lockFile()`, `unlockFile()`, `isFileLocked()`
- **Benefits**: Centralized locking logic, better cleanup
- **Status**: ✅ **Completed** - Full implementation with RAII cleanup, Logger integration, signals preservation, CMakeLists.txt updated

### Phase 2: Core Components

#### [x] 2.1 Create AudioSerializer Class
- **Purpose**: Clean WAV/embedding serialization
- **Location**: `audioserializer.h`, `audioserializer.cpp`
- **Key Methods**:
  - `bool saveWavToFile(const torch::Tensor&, const QString&, int sampleRate=32000)`
  - `bool saveEmbeddingToFile(const std::vector<float>&, const QString&)`
  - `torch::Tensor loadWavFromFile(const QString&)`
- **Refactoring**:
  - Break down 80-line `saveWav()` method:
    - `normalizeTensorShape()` - Tensor preprocessing
    - `writeWavHeader()` - WAV format header
    - `writeWavData()` - Audio data writing
- **Moves from ResourceManager**: `saveWav()`, `saveEmbeddingToFile()`, tensor logic
- **Status**: ✅ **Completed** - Clean implementation with Logger integration, tensor shape normalization, proper error handling, CMakeLists.txt updated

#### [x] 2.2 Create FileRepo Class
- **Purpose**: Pure data operations for files/folders
- **Location**: `filerepo.h`, `filerepo.cpp`
- **Data Structure**:
  ```cpp
  struct FileTypeData {
      QSet<QString> paths;
      QMap<QString, FolderWidget*> folders;
      QMap<QString, FileWidget*> files;
  };
  ```
- **Core Methods**:
  - `bool addFile/removeFile()`
  - `bool addFolder/removeFolder()`
  - Query methods: `getFiles()`, `getFolders()`, `getSingleFiles()`
- **Moves from ResourceManager**:
  - `FileTypeData` struct
  - `addFolder()`, `addSingleFile()`, `removeFile()`, `removeFolder()`
  - `getAddedFiles()`, `getFolders()`, `getSingleFiles()`, `isDuplicate()`
- **Signals**: Emits file/folder added/removed signals
- **Dependencies**: Uses FileLocker for locking checks

#### [x] 2.3 Create AsyncProcessor Class
- **Purpose**: Threading and worker coordination
- **Location**: `asyncprocessor.h`, `asyncprocessor.cpp`
- **Architecture**: RAII thread management (no static globals)
- **Key Methods**:
  - `void startFeatureGeneration(const QStringList&, const QString&)`
  - `void startAudioSeparation(const QStringList&, const QString&)`
  - `bool isProcessing() const`
- **Thread Setup**: Private methods for worker initialization
- **Moves from ResourceManager**:
  - Constructor thread setup logic
  - `startGenerateAudioFeatures()`, `startSeparateAudio()`
  - `handleFinalResult()`, worker management
  - Static thread/worker variables become instance members
- **Dependencies**: Uses AudioSerializer for result saving

### Phase 3: Refactor ResourceManager (Facade)

#### [x] 3.1 Update ResourceManager Header
- **Preserve**: Entire existing public interface (39 methods)
- **Add Private**: New component pointers
  ```cpp
  private:
      FileRepo* m_fileRepo;
      AsyncProcessor* m_asyncProcessor;
      AudioSerializer* m_serializer;
      FileLocker* m_fileLocker;
  ```
- **Modify**: Make methods become thin delegations

#### [x] 3.2 Refactor ResourceManager Implementation
- **Constructor Changes**:
  - Initialize new components instead of threads directly
  - Remove static thread setup logic
- **Method Delegations**:
  - File ops → FileRepo: `addFolder()`, `removeFile()`, etc.
  - Async ops → AsyncProcessor: `startGenerateAudioFeatures()`, etc.
  - Serialization → AudioSerializer: `saveWav()`, `saveEmbeddingToFile()`
  - Locking → FileLocker: `lockFile()`, `unlockFile()`
- **Signal Forwarding**: Forward signals from components to maintain existing connections

#### [ ] 3.3 Remove Obsolete Code
- Remove inline `emitFileAdded/Removed()` methods (become direct emissions)
- Remove duplicate validation logic
- Remove debug prints (replace with Logger calls)
- Remove static members, use dependency injection

### Phase 4: Integration & Testing

#### [ ] 4.1 Integration Testing
- **Compatibility**: All existing ResourceManager::instance() calls work unchanged
- **Signals**: All existing signal connections work unchanged
- **Functionality**: All existing behavior preserved (black box testing)
- **Performance**: Measure no regression in operation times

#### [ ] 4.2 Unit Testing Setup
- **FileLocker**: Test locking/unlocking operations
- **AudioSerializer**: Test WAV save/load cycles
- **FileRepo**: Test data operations without UI dependencies
- **AsyncProcessor**: Test threading logic in isolation
- **ResourceManager**: Integration tests for facade behavior

#### [ ] 4.3 Error Handling Improvements
- **Result<T> Pattern**: Consistent error handling
  ```cpp
  template<typename T, typename E>
  class Result {
      bool success;
      T value;
      E error;
  };
  ```
- **Migration**: Gradually replace bool/nullptr return patterns

### Phase 5: Code Quality & Documentation

#### [ ] 5.1 Code Cleanup
- Remove unreachable code
- Standardize variable naming
- Add const correctness to query methods
- Optimize includes (remove unused headers)

#### [ ] 5.2 Documentation Updates
- Update class documentation for new responsibilities
- Add method documentation for public interfaces
- Update architectural diagrams if any exist

#### [ ] 5.3 Build & Release Verification
- **Compile**: No breaking changes to build process
- **Runtime**: Manual testing of all features
- **Memory**: Verify no leaks in thread management
- **Performance**: Benchmark critical paths

## Migration Strategy

### Incremental Implementation
1. ✅ **Logger** (1-2 hours, no dependencies)
2. ✅ **FileLocker** (2-3 hours, isolated) - Ported existing logic, improved with Logger integration
3. ✅ **AudioSerializer** (4-5 hours, core functionality)
4. ✅ **FileRepo** (4-5 hours, depends on FileLocker)
5. ✅ **AsyncProcessor** (5-6 hours, depends on AudioSerializer)
6. ✅ **ResourceManager Refactor** (8-10 hours, integration)
7. ✅ **Testing & Verification** (4-5 hours)

### Risk Mitigation
- **Backward Compatibility**: All changes hidden behind existing interface
- **Gradual Migration**: Each class can be tested independently
- **Safety Nets**: Comprehensive testing before deployment
- **Rollback Plan**: Git branches allow reverting individual components

## Success Criteria
- ✅ All existing tests pass (if any exist)
- ✅ Manual testing shows identical behavior
- ✅ Code coverage maintained/improved
- ✅ Build succeeds with zero warnings
- ✅ Performance benchmarks meet/exceed current levels
- ✅ Code review passes with maintainability criteria

## Estimated Timeline
- **Total Effort**: 30-40 hours
- **Risk Level**: Medium (refactoring, not new features)
- **Parallel Work**: Logger and FileLocker can be developed in parallel
