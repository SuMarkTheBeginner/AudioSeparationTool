# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "AudioSeparationTool_autogen"
  "CMakeFiles/AudioSeparationTool_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/AudioSeparationTool_autogen.dir/ParseCache.txt"
  )
endif()
