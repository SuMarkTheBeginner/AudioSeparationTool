# Code Review Summary for AudioSeparationTool

This document summarizes the review of the main source files in the AudioSeparationTool project. It also notes missing comments and records what has been done in each file.

---

## main.cpp

- Contains the main entry point for the application.
- Initializes the Qt application, creates the main window, shows it, and starts the event loop.
- Well commented with file-level and function-level comments.

---

## mainwindow.cpp / mainwindow.h

- Implements the main window UI with a left sidebar and stacked content area.
- Sidebar contains navigation buttons for "How to Use", "Add Feature", and "Use Feature".
- Content area uses QStackedWidget to switch between pages.
- Manages locking/unlocking of files via read-only attribute.
- Comments are present but some methods could use more detailed comments on signal-slot connections and locking logic.

---

## htsatprocessor.cpp / htsatprocessor.h

- Handles loading a PyTorch model, reading and resampling audio files, preparing tensors, and running inference.
- Uses libsndfile and libsamplerate for audio processing.
- Emits signals on errors and processing completion.
- Added detailed comments on constructor, model loading, audio processing, tensor preparation, and inference.
- Header file has comprehensive class and method documentation.

---

## addsoundfeaturewidget.cpp / addsoundfeaturewidget.h

- Provides UI for adding sound features by selecting folders or single WAV files.
- Supports drag-and-drop, sorting, and managing added files.
- Connects signals for file and folder addition/removal.
- Well commented overall, but some complex UI setup and sorting logic could use more inline comments.

---

## fileutils.cpp / fileutils.h

- Provides utility to set or remove read-only attribute on files.
- Handles platform-specific code for Windows and Unix-like systems.
- Well commented with function-level comments.

---

## filewidget.cpp / filewidget.h

- Implements UI widget for individual file entries.
- Contains checkbox, file name, full path, and remove button.
- Event filter toggles checkbox on header click.
- Well commented with constructor, UI setup, and event handling explanations.

---

## resourcemanager.cpp / resourcemanager.h

- Implements a singleton ResourceManager class for centralized file management in the application.
- Handles adding/removing files and folders, duplicate checking, locking/unlocking files, and emitting signals for UI updates.
- Supports multiple file types (WavForFeature, SoundFeature, WavForSeparation), with current focus on WavForFeature.
- Integrates HTSATProcessor for audio processing tasks like model loading and feature generation.
- Well commented with comprehensive class and method documentation in header, and detailed comments in implementation.
- Includes methods for sorting, getters for data access, and signal emissions for file/folder operations.

## folderwidget.cpp / folderwidget.h

- Implements FolderWidget class for displaying folders containing WAV files in the UI.
- Features a collapsible list of file checkboxes with tri-state folder checkbox for selection management.
- Supports adding/removing files, event filtering for expand/collapse, and signal emissions for file/folder removal.
- Uses WideCheckBox for file items and custom styling.
- Well commented with detailed method descriptions, constructor explanations, and UI setup comments.
- Header file has comprehensive class documentation and signal declarations.

## widecheckbox.h

- Simple header-only class inheriting from QCheckBox.
- Provides a custom WideCheckBox with inherited constructors for wider appearance in layouts.
- Minimal implementation with brief documentation.

# Summary of Actions Taken

- Reviewed key source files for comments and clarity.
- Added comments where missing or insufficient, especially in htsatprocessor.cpp.
- Created this summary file to record the review and document what has been done.
- Re-scanned all files in the project, including newly added ones like resourcemanager.cpp/h, folderwidget.cpp/h, and widecheckbox.h.
- Updated the summary with reviews of new files, noting their functionality, comments, and integration.
- Recorded changes such as the addition of ResourceManager for centralized file management and FolderWidget for UI folder representation.
