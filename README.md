# AudioSeparationTool Project Analysis Report

## Overview

The AudioSeparationTool is a Qt-based desktop application for audio separation using deep learning models. It allows users to create sound features from reference audio files and apply these features to separate similar sounds from target audio files. The application uses machine learning models (HTSAT for feature extraction and Zero-Shot ASP for separation) combined with traditional signal processing techniques like overlap-add.

## Project Structure

The project follows a typical Qt application structure with separation of concerns into UI, business logic, and audio processing components.

### Root Directory Files

- **Core Application Files**:
  - `main.cpp` - Application entry point, initializes Qt, registers model resources, and shows main window
  - `mainwindow.cpp/h` - Primary UI window with sidebar navigation and stacked widget content
  - `mainwindow.ui` - Qt Designer form for main window layout

- **UI Widget Classes**:
  - `addsoundfeaturewidget.cpp/h` - Widget for creating sound features from reference audio
  - `usefeaturewidget.cpp/h` - Widget for applying features to process audio files
  - `filewidget.cpp/h` - Individual file widgets with play/remove buttons
  - `folderwidget.cpp/h` - Folder picker widgets for batch processing
  - `filemanagerwidget.cpp/h` - Base class for file management functionality
  - `widecheckbox.cpp/h` - Custom checkbox component

- **Audio Processing Classes**:
  - `separationworker.cpp/h` - Core separation logic using chunks and overlap-add
  - `htsatprocessor.cpp/h` - HTSAT model wrapper for audio feature extraction
  - `htsatworker.cpp/h` - Worker class for HTSAT processing (likely threaded)
  - `zero_shot_asp_feature_extractor.cpp/h` - Zero-Shot ASP model interface
  - `audio_preprocess_utils.cpp/h` - Audio preprocessing utilities

- **Utility Classes**:
  - `resourcemanager.cpp/h` - Manages file resources and prevents concurrent access
  - `audioplayer.cpp/h` - Audio playback functionality
  - `errorhandler.cpp/h` - Centralized error handling
  - `fileutils.cpp/h` - File system utilities
  - `constants.h` - Application constants and configuration

- **Build and Configuration**:
  - `CMakeLists.txt` - CMake build configuration
  - `models.qrc` - Qt resource file for embedding models
  - `AudioSeparationTool_zh_TW.ts` - Chinese Traditional language translation
  - `.gitignore`, `.blackboxrules` - Version control and development rules

### Directories

- **`models/`** - Contains pre-trained neural network models:
  - `htsat_embedding_model.pt` - Hierarchical Token-Semantic Audio Transformer for feature extraction
  - `zero_shot_asp_separation_model.pt` - Zero-Shot Audio Source Separation model
  - `test.txt` - Possibly test configuration data

- **`vcpkg/`** - vcpkg dependency management system (not committed to git)

### Output Directories (Runtime Generated)
- `output_features/` - Stores extracted sound feature embedding files
- `separated_results/` - Contains separated audio output files
- `temp_chunks/` - Temporary audio chunks during processing

## Architecture and Interactions

### Application Flow

1. **Initialization**: `main.cpp` creates QApplication, registers model resources from `models.qrc`, instantiates MainWindow
2. **UI Management**: MainWindow provides sidebar navigation between "Add Feature" and "Use Feature" pages
3. **Feature Creation**: AddSoundFeatureWidget allows users to select reference audio files and generate embeddings using HTSAT
4. **Audio Processing**: UseFeatureWidget lets users select target files and apply stored features using Zero-Shot ASP separation

### Class Interactions

#### UI Layer
- **MainWindow**: Manages overall UI layout and coordinates between widgets. Connects signals from widgets to update progress bar and audio player.
  - Emits: `playRequested()`, `onFileAdded/Removed()`, `onProcessingStarted/Finished/Error()`

#### Widget Layer
- **AddSoundFeatureWidget**: Inherits from FileManagerWidget, allows selecting audio files for feature extraction
- **UseFeatureWidget**: Inherits from FileManagerWidget, allows selecting feature files and target audio for separation
- **FileManagerWidget**: Base class providing common file/folder selection, drag-and-drop, and progress UI
- **FileWidget/FolderWidget**: Atomic UI components for individual files/folders

#### Processing Layer
- **SeparationWorker**: Qt thread worker for audio separation. Processes audio in chunks to handle large files.
  - Methods: `processFile()`, `loadFeature()`, `processChunk()`, `doOverlapAdd()`
  - Signals: `chunkReady()`, `separationFinished()`, `progressUpdated()`, `error()`

- **HTSATProcessor**: TorchScript model wrapper for feature extraction
  - Loads model from `htsat_embedding_model.pt`
  - Process audio tensors to generate embeddings using HTSAT transformer

- **ZeroShotASPFeatureExtractor**: Interface to ASP separation model
  - Uses `zero_shot_asp_separation_model.pt` for conditional source separation
  - Takes query embedding + target audio tensor, produces separated output

#### Utility Layer
- **ResourceManager**: Singleton managing file locks to prevent concurrent processing of same files
- **AudioPlayer**: Qt Multimedia-based audio playback for previewing files
- **AudioPreprocessUtils**: Audio loading, resampling, normalization utilities
- **ErrorHandler**: Centralized error reporting and user notification

### Signal-Slot Communication

The application heavily uses Qt's signal-slot mechanism for loose coupling:

```cpp
// Example from MainWindow
connect(useFeatureWidget, &UseFeatureWidget::playRequested,
        audioPlayer, &AudioPlayer::playFile);

// Example from Processing Chain
connect(separationWorker, &SeparationWorker::progressUpdated,
        this, &MainWindow::updateProgress);
connect(separationWorker, &SeparationWorker::error,
        this, &MainWindow::onProcessingError);
```

### Audio Processing Pipeline

1. **Preprocessing**: Audio loaded and resampled to 32kHz using libsndfile and libsamplerate
2. **Chunking**: Long audio files split into 10-second chunks with 50% overlap
3. **Feature Loading**: Query embeddings loaded from PTH files (output_features/)
4. **Separation**: Each chunk processed through Zero-Shot ASP model with query condition
5. **Overlap-Add**: Chunk outputs reconstructed using weighted overlap-add
6. **Output**: Separated audio saved as WAV files

### Build System

- **CMake-based**: Uses Qt6, LibTorch (PyTorch), libsndfile, libsamplerate
- **vcpkg Integration**: Manages external dependencies via vcpkg
- **Resource Management**: Models compiled into binary RCC files for distribution
- **Conditional Compilation**: Supports CUDA acceleration (disabled) and CPU-only operation

## Key Technical Details

- **Qt Version**: Qt6 with C++17
- **Deep Learning**: PyTorch (LibTorch) for model inference
- **Audio Libraries**: libsndfile (file I/O), libsamplerate (resampling)
- **Processing**: Chunk-based with overlap-add for memory-efficient processing of long files
- **Threading**: Qt's signal-slot for thread-safe UI updates
- **Localization**: Supports Chinese Traditional (Qt Linguist)

## Naming Conventions

The project establishes and follows these Qt C++ naming conventions:

### Primary Convention Sets:
- **Classes**: PascalCase (e.g., `MainWindow`, `HTSATProcessor`, `UseFeatureWidget`)
- **Methods/Functions**: camelCase (e.g., `setupUI()`, `loadFeature()`, `processFile()`, utility functions like `sizesToString()`)
- **Signals and Slots**: camelCase (e.g., signals: `playRequested()`, `separationFinished()`; slots: `onProcessClicked()`, `onProcessingFinished()`)

### Variables and Constants:
- **Member Variables**: m_ prefix with camelCase (Hungarian notation, e.g., `m_filePath`, `m_folderPath`, `m_centralWidgetContainer`)
- **Constants**: ALL_CAPS with underscores (e.g., `AUDIO_SAMPLE_RATE`, `OUTPUT_FEATURES_DIR`)
- **Local Variables**: camelCase (e.g., `fileName`, `audioTensor`)
- **Static Members**: s_ prefix with camelCase (if any)

### File and Project Naming:
- **File Names**: all lowercase, matching class name without separators (e.g., `mainwindow.h`, `htsatprocessor.h`, `addsoundfeaturewidget.h`)
- **Directories**: snake_case (e.g., `audio_preprocess_utils`, `output_features`)
- **Enums**: PascalCase for enum names, camelCase for values (e.g., `enum class SortType { NameAsc, NameDesc }`)

These conventions ensure consistent, readable code following Qt standards. During the naming consistency review, all member variables were updated to comply with the m_ prefix requirement, eliminating previous inconsistencies. No other naming issues were identified.

## Documentation Standards

The project follows Qt documentation standards using Doxygen-compatible comments for maintainability and IDE integration. All public APIs, classes, complex logic, and non-obvious code sections must be documented.

### Class Documentation
Classes use detailed header documentation:

```cpp
/**
 * @brief Brief description of class purpose and functionality.
 *
 * Detailed description explaining what the class does,
 * how it fits into the architecture, and any important
 * usage notes or constraints.
 */
class ClassName : public BaseClass
{
    Q_OBJECT  // If Qt class with signals/slots
```

### Method and Function Documentation
All public methods include full documentation:

```cpp
/**
 * @brief Brief description of method purpose.
 *
 * Detailed explanation of what the method does,
 * algorithms used, and edge cases handled.
 * @param parameterName Description of parameter purpose and expected values
 * @param anotherParam Another parameter description
 * @return Description of return value and when different values are returned
 * @see relatedClass::relatedMethod - Cross-reference related functionality
 */
ReturnType methodName(ParameterType parameterName, AnotherType anotherParam);
```

### Member Variable Documentation
Use inline comments for member variables:

```cpp
private:
    Type* m_variableName;  ///< Description of variable purpose and usage
    bool m_flag;           ///< True if [condition], affects [behavior]
```

### Complex Logic and Algorithm Documentation
Complex algorithms and business logic are documented with inline comments:

```cpp
//==============================================================================
// Overlap-Add Processing Algorithm
//
// This method implements the overlap-add algorithm for reconstructing
// long audio signals from processed chunks with overlap. The algorithm:
// 1. Receives vector of processed chunks from separation model
// 2. Calculates step size based on overlap rate (1.0 - m_overlapRate)
// 3. Applies Hann windowing to each chunk for smooth reconstruction
// 4. Accumulates weighted contributions in output buffer
// 5. Normalizes by weight to prevent amplitude scaling artifacts
//
// Reference: https://en.wikipedia.org/wiki/Overlap%E2%80%93add_method
//==============================================================================
torch::Tensor doOverlapAdd(const std::vector<torch::Tensor>& chunks)
{
    // Step 1: Calculate overlap parameters
    int64_t chunkSize = chunks[0].size(1);
    int64_t step = static_cast<int64_t>(chunkSize * (1.0f - m_overlapRate));
    int64_t totalLength = step * (chunks.size() - 1) + chunkSize;

    // Create Hann window for overlap weighting
    torch::Tensor window = torch::ones({chunkSize}, torch::kFloat);
    // ... implementation with detailed inline comments for each step
}
```

### File Header Documentation
Each source file includes standard header:

```cpp
//==============================================================================
// FileName.cpp/h - Brief description of file contents
//
// Detailed description of file purpose, classes contained,
// and relationship to other files in the system.
//
// Copyright (c) Year Author - License information
//==============================================================================
```

### Constants Documentation
Constants are documented at definition:

```cpp
// Audio processing constants
const int AUDIO_SAMPLE_RATE = 32000;       ///< Standard sample rate for model input
const float AUDIO_OVERLAP_RATE = 0.5f;     ///< Overlap proportion for overlap-add (50%)
const int AUDIO_CLIP_SAMPLES = 320000;     ///< Samples per chunk (10 sec @ 32kHz)
```

### Documentation Requirements
- **Public APIs**: Full documentation with @brief, @param, @return
- **Private Methods**: Document if complex or non-obvious
- **Complex Algorithms**: Inline comments explaining steps and rationale
- **Threading/Synchronization**: Document thread-safety and locking behavior
- **Error Handling**: Document exceptions/errors that may be thrown
- **Dependencies**: Note external library dependencies and requirements

Use TODO comments for incomplete implementations:
```cpp
// TODO: Implement CUDA acceleration for model inference (Issue #123)
```

### Tools and Validation
- Doxygen can generate HTML documentation from these comments
- Qt Creator IDE can display formatted documentation on hover
- Code reviews enforce documentation completeness for new code

## Current State and TODO

After comprehensive naming consistency review:

### Completed Refactoring Tasks:
- **Class/File Naming**: All classes follow PascalCase convention, no renaming needed. File names are correctly lowercase matching class names (e.g., `SeparationWorker` class in `separationworker.h`). The example in TODO.md was incorrect - this naming is standard and consistent.
- **Method Naming**: All methods follow camelCase convention with clear, descriptive names. No methods identified for renaming - naming is consistent and self-documenting (e.g., `doOverlapAdd()`, `processSingleFile()`, `setupConnections()`).
- **Documentation Standards**: Established with Doxygen/Qt compliant commenting standards
- **Member Variables**: All updated to use m_ prefix for Hungarian notation consistency

### Remaining TODO Items:
- Documentation improvements needed (standards now established, implementation pending)
- UI/UX enhancements planned
- Consider adding unit tests for core audio processing algorithms
- Evaluate CUDA acceleration implementation (currently disabled)

The application successfully integrates modern deep learning approaches (HTSAT, Zero-Shot ASP) with traditional digital signal processing techniques in a user-friendly desktop interface for audio source separation tasks.
