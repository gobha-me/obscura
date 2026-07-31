add_executable(${PROJECT_NAME}
  main.cpp
)

# PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR: the latter is the top-level build's
# root, which belongs to someone else as soon as this project is consumed. This
# is what puts the generated <version.hpp> on the include path.
target_include_directories(${PROJECT_NAME}
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)

# Guarded, because -Dobscura_BUILD_LIB=OFF is a supported configuration and it
# removes the target outright. main.cpp is written to survive it — it reads only
# the generated version header — so the library-disabled build still produces a
# runnable executable, which is what CI's `library disabled` job installs and
# then checks for.
if (TARGET ${PROJECT_NAME}::lib)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE ${PROJECT_NAME}::lib
  )
endif ()
