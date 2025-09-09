# TODO: Move File Management to ResourceManager

## Step 1: Design and Implement ResourceManager Structure
- [x] Add singleton pattern to ResourceManager
- [x] Add data members: QSet for addedFilePaths, QMap for folderMap, QMap for singleFileMap, QSet for lockedFiles
- [x] Prepare data structures for three file types (focus on type 1 initially)
- [x] Add signals: fileAdded, fileRemoved, folderRemoved, fileLocked, fileUnlocked

## Step 2: Implement Core Methods in ResourceManager
- [x] Implement addFolder method with duplicate checking
- [x] Implement addSingleFile method with duplicate checking
- [x] Implement removeFile and removeFolder methods
- [x] Implement lockFile and unlockFile using FileUtils
- [x] Implement sortAll method
- [x] Add getters for accessing data (getAddedFiles, getFolders, etc.)

## Step 3: Refactor AddSoundFeatureWidget
- [x] Remove local data members: addedFilePaths, folderMap, singleFileMap
- [x] Update addFolder method to delegate to ResourceManager
- [x] Update addSingleFile method to delegate to ResourceManager
- [x] Update sortAll method to delegate to ResourceManager
- [x] Connect to ResourceManager signals instead of emitting own signals
- [x] Update signal connections for file/folder removal

## Step 4: Refactor MainWindow
- [x] Remove lockedFiles data member
- [x] Remove locking logic from setupContent
- [x] Connect to ResourceManager signals for locking operations
- [x] Update folderRemoved handling to use ResourceManager

## Step 5: Update Dependent Files
- [x] Check and update FolderWidget if needed
- [x] Check and update FileWidget if needed
- [x] Ensure all connections are properly set up

## Step 6: Testing and Cleanup
- [x] Test add/remove file functionality
- [x] Test locking/unlocking functionality
- [x] Test sorting functionality
- [x] Verify no regressions in UI behavior
- [x] Update documentation and comments
