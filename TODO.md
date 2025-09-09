# TODO List for Moving File Management to ResourceManager

## Step 1: Design and Implement ResourceManager
- Add data members to track:
  - Feature WAV files (addedFilePaths equivalent)
  - Sound feature vectors (placeholder)
  - Separation WAV files (placeholder)
- Implement methods for:
  - Adding/removing files and folders
  - Checking duplicates
  - Locking/unlocking files (read-only attribute)
  - Emitting signals for file/folder added/removed

## Step 2: Refactor AddSoundFeatureWidget
- Remove direct file tracking (addedFilePaths, folderMap, singleFileMap)
- Use ResourceManager methods for file management
- Connect ResourceManager signals to update UI accordingly

## Step 3: Refactor MainWindow
- Remove lockedFiles tracking
- Use ResourceManager for locking/unlocking files
- Connect to ResourceManager signals for file events

## Step 4: Testing
- Verify adding/removing files and folders via UI
- Verify locking/unlocking behavior
- Verify signals and UI updates
- Test drag-and-drop and sorting features

## Step 5: Cleanup and Documentation
- Update comments and documentation to reflect changes
- Remove redundant code from AddSoundFeatureWidget and MainWindow
