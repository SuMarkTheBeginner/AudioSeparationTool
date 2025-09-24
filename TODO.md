<task_progress>

- [x] Analyze current codebase structure and naming patterns
- [x] Review all classes, methods, variables, slots, and signals for naming consistency
- [x] Establish naming conventions (camelCase, PascalCase, snake_case, etc.)
- [x] Create documentation standards for classes, methods, and complex logic
- [x] Identify classes needing renaming (e.g., htsatprocessor, separationworker)
- [x] Identify methods needing renaming for clarity and consistency
- [x] Identify variables needing renaming for better readability
- [x] Identify Qt slots and signals needing renaming
- [x] Plan documentation comment additions for public APIs
- [x] Plan documentation comment additions for complex private methods
- [x] Plan refactoring of core audio processing classes
- [x] Plan refactoring of UI widget classes
- [x] Plan refactoring of worker/utility classes
- [ ] Create backup/commit point before major changes
- [ ] Implement class renamings systematically
- [ ] Implement method renamings systematically
- [ ] Implement variable renamings systematically
- [ ] Implement slot/signal renamings systematically
- [ ] Add comprehensive documentation comments
- [ ] Update any external references or documentation
- [ ] Test compilation after each major refactoring phase
- [ ] Verify functionality after refactoring completion
- [ ] Final review and cleanup of any remaining inconsistencies </task_progress>

# Audio Processing Refactoring Plan

## Current Architecture Analysis

### Core Classes Identified:
- **HTSATProcessor**: Handles HTSAT model loading and inference
- **SeparationWorker**: Manages audio separation processing pipeline
- **HTSATWorker**: Handles feature generation using HTSAT
- **ZeroShotASPFeatureExtractor**: Manages zero-shot audio separation model

### Key Issues Found:

1. **Tight Coupling**: Classes are tightly coupled with Qt-specific concerns (signals/slots) mixed with business logic
2. **Code Duplication**:
   - Model loading logic duplicated between HTSATProcessor and ZeroShotASPFeatureExtractor
   - Error handling patterns repeated across classes
   - Progress reporting mechanisms similar across workers
3. **Mixed Responsibilities**:
   - HTSATProcessor handles model loading, inference, AND resource management
   - Workers manage both processing logic and Qt signaling
   - ResourceManager coordinates too many concerns (files, processing, resources)
4. **Resource Management**: Inconsistent cleanup of torch tensors and temporary files
5. **Error Handling**: Inconsistent error propagation and handling patterns

## Proposed Refactored Architecture

### 1. **Abstract Base Classes**
Create abstract base classes to separate concerns:

```cpp
// AudioProcessorBase - Common model loading and inference interface
class AudioProcessorBase : public QObject {
protected:
    virtual bool loadModelInternal(const QString& path) = 0;
    virtual torch::Tensor processInternal(const torch::Tensor& input) = 0;
public:
    bool loadModel(const QString& path);
    torch::Tensor process(const torch::Tensor& input);
    // Common error handling and resource management
};

// WorkerBase - Common worker functionality
class AudioWorkerBase : public QObject {
protected:
    virtual void processInternal() = 0;
public:
    void startProcessing();
    // Common progress reporting and error handling
};
```

### 2. **Refactored Class Structure**

**Model Layer (Pure Processing Logic):**
- `HTSATModel` - Pure HTSAT model wrapper (no Qt dependencies)
- `ZeroShotASPModel` - Pure zero-shot model wrapper (no Qt dependencies)
- `AudioPreprocessor` - Centralized audio preprocessing utilities

**Processing Layer (Qt Integration):**
- `HTSATProcessor` - Inherits from AudioProcessorBase, adds Qt signals
- `SeparationProcessor` - Inherits from AudioProcessorBase, adds Qt signals
- `FeatureGenerator` - Specialized for feature generation

**Worker Layer (Threading & Coordination):**
- `HTSATWorker` - Inherits from AudioWorkerBase
- `SeparationWorker` - Inherits from AudioWorkerBase
- `BatchProcessor` - Handles batch processing coordination

### 3. **Utility Classes**
- `ModelLoader` - Centralized model loading with error handling
- `AudioFileManager` - Dedicated audio file operations
- `ProgressReporter` - Standardized progress reporting
- `ErrorHandler` - Centralized error handling and propagation

### 4. **Key Improvements**

**Better Separation of Concerns:**
- Pure processing logic separated from Qt concerns
- Model loading abstracted from specific model types
- Resource management centralized

**Reduced Code Duplication:**
- Common model loading interface
- Shared error handling patterns
- Unified progress reporting mechanism

**Improved Resource Management:**
- RAII patterns for torch tensors
- Automatic cleanup of temporary files
- Better memory management for large audio files

**Enhanced Error Handling:**
- Consistent error propagation
- Centralized error logging
- Better error recovery mechanisms

### 5. **Implementation Phases**

**Phase 1: Foundation**
- [ ] Create abstract base classes
- [ ] Implement utility classes (ModelLoader, ErrorHandler, etc.)
- [ ] Refactor HTSATProcessor to use new architecture

**Phase 2: Model Layer**
- [ ] Extract pure model wrappers
- [ ] Implement AudioPreprocessor
- [ ] Create centralized audio processing utilities

**Phase 3: Processing Layer**
- [ ] Refactor SeparationWorker
- [ ] Implement FeatureGenerator
- [ ] Update HTSATWorker

**Phase 4: Worker Layer**
- [ ] Implement AudioWorkerBase
- [ ] Refactor existing workers
- [ ] Add BatchProcessor for coordination

**Phase 5: Integration & Testing**
- [ ] Update ResourceManager to use new architecture
- [ ] Comprehensive testing of refactored components
- [ ] Performance optimization

### 6. **Benefits of This Refactoring**

- **Maintainability**: Clear separation of concerns makes code easier to modify
- **Testability**: Pure model classes can be unit tested without Qt dependencies
- **Reusability**: Abstract interfaces allow for easy addition of new model types
- **Performance**: Better resource management and reduced memory overhead
- **Reliability**: Consistent error handling and better resource cleanup

This refactoring will significantly improve the codebase's architecture while maintaining backward compatibility with the existing UI layer.

# UI Widget Classes Refactoring Plan

## Current State Analysis

**UI Widget Classes Identified:**
- **FileManagerWidget** - Base class for file/folder management with drag-and-drop
- **AddSoundFeatureWidget** - For adding sound features (inherits from FileManagerWidget)
- **UseFeatureWidget** - For using features in audio separation (inherits from FileManagerWidget)
- **FileWidget** - Individual file representation with checkbox and play functionality
- **FolderWidget** - Folder representation with expandable file list
- **WideCheckBox** - Custom checkbox with enhanced hit area

**Issues Identified:**
1. **Code Duplication**: Similar UI setup and event handling code repeated across widgets
2. **Inconsistent Interfaces**: Different widgets handle similar functionality differently
3. **Tight Coupling**: Direct dependencies between related widgets
4. **Missing Abstractions**: No common base class for selection widgets (FileWidget/FolderWidget)
5. **Event Handling**: Similar event filtering logic in multiple places
6. **Layout Management**: Repeated layout setup patterns

## Detailed Refactoring Plan

### Phase 1: Create Common Base Classes
- **SelectionWidgetBase** - Common base for FileWidget and FolderWidget
- **UIUtilities** - Static utility class for common UI setup patterns
- **EventHandlerBase** - Common event handling functionality

### Phase 2: Standardize Widget Interfaces
- **ISelectableWidget** - Interface for consistent selection behavior
- **IPlayableWidget** - Interface for audio playback functionality
- **WidgetFactory** - Pattern for consistent widget creation

### Phase 3: Extract Common Functionality
- **DragDropHandler** - Consistent drag-and-drop behavior
- **FileValidator** - Common file/folder validation logic
- **LayoutBuilder** - Reusable layout setup methods

### Phase 4: Improve Architecture
- **WidgetManager** - Centralized widget lifecycle management
- **Observer Pattern** - For widget state synchronization
- **Command Pattern** - For UI actions and undo functionality

### Phase 5: Enhance Maintainability
- **Comprehensive Documentation** - For all refactored classes
- **Unit Tests** - For common functionality
- **Error Handling** - Consistent error handling patterns

## Benefits of This Refactoring
1. **Reduced Code Duplication**: Common functionality centralized in base classes
2. **Improved Maintainability**: Changes to common behavior only need to be made in one place
3. **Better Testability**: Separated concerns make unit testing easier
4. **Consistent User Experience**: Standardized interfaces ensure consistent behavior
5. **Enhanced Extensibility**: New widget types can easily inherit from established base classes
6. **Cleaner Architecture**: Clear separation between UI and business logic

## Implementation Priority
1. **High Priority**: Create base classes and extract common functionality
2. **Medium Priority**: Standardize interfaces and create factory pattern
3. **Low Priority**: Add comprehensive documentation and testing

This comprehensive refactoring plan addresses all major architectural issues while maintaining backward compatibility and improving the overall codebase quality.

# Worker/Utility Classes Refactoring Plan

## Current State Analysis

**Worker/Utility Classes Identified:**
- **HTSATWorker** - Handles HTSAT feature generation in a separate thread
- **SeparationWorker** - Manages audio separation processing pipeline
- **ResourceManager** - Singleton for managing all file resources and coordinating processing
- **FileUtils** - Utility functions for file operations

**Issues Identified:**
1. **Mixed Responsibilities**: ResourceManager handles file management, processing coordination, AND resource cleanup
2. **Tight Coupling**: Workers are tightly coupled with Qt-specific concerns (signals/slots) mixed with business logic
3. **Code Duplication**: Similar progress reporting and error handling patterns across workers
4. **Resource Management**: Inconsistent cleanup of torch tensors and temporary files
5. **Error Handling**: Inconsistent error propagation and handling patterns
6. **Threading Concerns**: Qt-specific threading mixed with business logic

## Detailed Refactoring Plan

### Phase 1: Create Abstract Base Classes
- **WorkerBase** - Common worker functionality with Qt signal handling
- **ProcessorBase** - Common processing interface for audio processors
- **ResourceManagerBase** - Core resource management without processing coordination

### Phase 2: Separate Processing Logic
- **HTSATProcessorCore** - Pure HTSAT processing logic (no Qt dependencies)
- **SeparationProcessorCore** - Pure separation processing logic (no Qt dependencies)
- **AudioFileManager** - Dedicated audio file operations and validation

### Phase 3: Extract Common Functionality
- **ProgressReporter** - Standardized progress reporting across all workers
- **ErrorHandler** - Centralized error handling and propagation
- **ResourceCleaner** - RAII patterns for torch tensors and temporary files
- **ThreadSafeQueue** - Thread-safe communication between workers

### Phase 4: Improve Resource Management
- **TensorPool** - Memory pool for torch tensor reuse
- **TempFileManager** - Automatic cleanup of temporary files
- **ModelCache** - Efficient model loading and caching
- **MemoryMonitor** - Monitor and manage memory usage during processing

### Phase 5: Enhance Architecture
- **ProcessingCoordinator** - Coordinates between different processing stages
- **PipelineManager** - Manages the overall processing pipeline
- **ResultAggregator** - Handles result collection and aggregation
- **QualityValidator** - Validates processing results

## Benefits of This Refactoring
1. **Better Separation of Concerns**: Pure processing logic separated from Qt concerns
2. **Improved Testability**: Core logic can be unit tested without Qt dependencies
3. **Reduced Code Duplication**: Common functionality centralized in base classes
4. **Better Resource Management**: RAII patterns and automatic cleanup
5. **Enhanced Error Handling**: Consistent error propagation and recovery
6. **Improved Performance**: Memory pooling and caching mechanisms
7. **Better Maintainability**: Clear interfaces and responsibilities

## Implementation Priority
1. **High Priority**: Create base classes and extract common functionality
2. **Medium Priority**: Separate processing logic from Qt concerns
3. **Low Priority**: Add advanced features like memory pooling and caching

This comprehensive refactoring plan addresses all major architectural issues while maintaining backward compatibility and improving the overall codebase quality.
