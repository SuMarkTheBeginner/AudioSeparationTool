# TODO: Integrate Progress Bar for generateAudioFeatures

- [x] Add signal void progressUpdated(int value) in ResourceManager (resourcemanager.h)
- [x] Emit progressUpdated in generateAudioFeatures loop (resourcemanager.cpp)
- [x] Add private slot void updateProgress(int value) in MainWindow (mainwindow.h)
- [x] Implement updateProgress slot in mainwindow.cpp
- [x] Connect progressUpdated signal to updateProgress slot in setupContent() (mainwindow.cpp)

# TODO: Modify generateAudioFeatures to save in specific folder with unique names

- [x] Modify generateAudioFeatures to save output files in build/Desktop-Debug/output_features folder
- [x] Create the output folder if it does not exist
- [x] Append timestamp to output file name to handle duplicates
- [x] Update file saving logic in generateAudioFeatures
- [ ] Test the new save functionality
