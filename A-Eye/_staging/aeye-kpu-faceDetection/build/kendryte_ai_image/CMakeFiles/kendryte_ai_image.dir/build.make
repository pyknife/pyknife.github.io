# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.13

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = D:/Green/KendryteIDE/LocalPackage/cmake/bin/cmake.exe

# The command to remove a file.
RM = D:/Green/KendryteIDE/LocalPackage/cmake/bin/cmake.exe -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build

# Include any dependencies generated for this target.
include kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/depend.make

# Include the progress variables for this target.
include kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/progress.make

# Include the compile flags for this target's objects.
include kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/flags.make

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/image_process.c.obj: kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/flags.make
kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/image_process.c.obj: ../kendryte_libraries/kendryte_ai_image/src/image_process.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/image_process.c.obj"
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && D:/Green/KendryteIDE/LocalPackage/toolchain/bin/riscv64-unknown-elf-gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/kendryte_ai_image.dir/src/image_process.c.obj   -c C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/kendryte_libraries/kendryte_ai_image/src/image_process.c

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/image_process.c.i: cmake_force
	@echo "Preprocessing C source to CMakeFiles/kendryte_ai_image.dir/src/image_process.c.i"
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && D:/Green/KendryteIDE/LocalPackage/toolchain/bin/riscv64-unknown-elf-gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/kendryte_libraries/kendryte_ai_image/src/image_process.c > CMakeFiles/kendryte_ai_image.dir/src/image_process.c.i

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/image_process.c.s: cmake_force
	@echo "Compiling C source to assembly CMakeFiles/kendryte_ai_image.dir/src/image_process.c.s"
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && D:/Green/KendryteIDE/LocalPackage/toolchain/bin/riscv64-unknown-elf-gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/kendryte_libraries/kendryte_ai_image/src/image_process.c -o CMakeFiles/kendryte_ai_image.dir/src/image_process.c.s

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.obj: kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/flags.make
kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.obj: ../kendryte_libraries/kendryte_ai_image/src/region_layer.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.obj"
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && D:/Green/KendryteIDE/LocalPackage/toolchain/bin/riscv64-unknown-elf-gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.obj   -c C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/kendryte_libraries/kendryte_ai_image/src/region_layer.c

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.i: cmake_force
	@echo "Preprocessing C source to CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.i"
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && D:/Green/KendryteIDE/LocalPackage/toolchain/bin/riscv64-unknown-elf-gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/kendryte_libraries/kendryte_ai_image/src/region_layer.c > CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.i

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.s: cmake_force
	@echo "Compiling C source to assembly CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.s"
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && D:/Green/KendryteIDE/LocalPackage/toolchain/bin/riscv64-unknown-elf-gcc.exe $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/kendryte_libraries/kendryte_ai_image/src/region_layer.c -o CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.s

# Object files for target kendryte_ai_image
kendryte_ai_image_OBJECTS = \
"CMakeFiles/kendryte_ai_image.dir/src/image_process.c.obj" \
"CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.obj"

# External object files for target kendryte_ai_image
kendryte_ai_image_EXTERNAL_OBJECTS =

kendryte_ai_image/libkendryte_ai_image.a: kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/image_process.c.obj
kendryte_ai_image/libkendryte_ai_image.a: kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/src/region_layer.c.obj
kendryte_ai_image/libkendryte_ai_image.a: kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/build.make
kendryte_ai_image/libkendryte_ai_image.a: kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --progress-dir=C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking C static library libkendryte_ai_image.a"
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && $(CMAKE_COMMAND) -P CMakeFiles/kendryte_ai_image.dir/cmake_clean_target.cmake
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/kendryte_ai_image.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/build: kendryte_ai_image/libkendryte_ai_image.a

.PHONY : kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/build

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/clean:
	cd C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image && $(CMAKE_COMMAND) -P CMakeFiles/kendryte_ai_image.dir/cmake_clean.cmake
.PHONY : kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/clean

kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/depend:
	$(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/kendryte_libraries/kendryte_ai_image C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image C:/Users/Pengzhihui/Desktop/aeye-kpu-01/aeye-kpu/build/kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/DependInfo.cmake
.PHONY : kendryte_ai_image/CMakeFiles/kendryte_ai_image.dir/depend

