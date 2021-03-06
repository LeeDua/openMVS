INCLUDE_DIRECTORIES(".")

# List sources files
FILE(GLOB PCH_C "Common.cpp")

FILE(GLOB LIBRARY_FILES_C "*.cpp")
FILE(GLOB LIBRARY_FILES_H "*.h" "*.inl")

FILE(GLOB IBFS_LIBRARY_FILES_C "IBFS/*.cpp")
FILE(GLOB IBFS_LIBRARY_FILES_H "IBFS/*.h" "IBFS/*.inl")
SOURCE_GROUP("IBFS" FILES ${IBFS_LIBRARY_FILES_C} ${IBFS_LIBRARY_FILES_H})

FILE(GLOB LMFit_LIBRARY_FILES_C "LMFit/*.cpp")
FILE(GLOB LMFit_LIBRARY_FILES_H "LMFit/*.h" "LMFit/*.inl")
SOURCE_GROUP("LMFit" FILES ${LMFit_LIBRARY_FILES_C} ${LMFit_LIBRARY_FILES_H})

FILE(GLOB TRWS_LIBRARY_FILES_C "TRWS/*.cpp")
FILE(GLOB TRWS_LIBRARY_FILES_H "TRWS/*.h" "TRWS/*.inl")
SOURCE_GROUP("TRWS" FILES ${TRWS_LIBRARY_FILES_C} ${TRWS_LIBRARY_FILES_H})

# Place Common.cpp as the first file in the list
# needed by cotire when setting PCH manually
LIST(REMOVE_ITEM LIBRARY_FILES_C ${PCH_C})
SET(LIBRARY_FILES_C "${PCH_C};${LIBRARY_FILES_C}")

cxx_library_with_type_no_pch(Math "Libs" "STATIC" "${cxx_default}"
	${LIBRARY_FILES_C} ${LIBRARY_FILES_H}
	${IBFS_LIBRARY_FILES_C} ${IBFS_LIBRARY_FILES_H}
	${LMFit_LIBRARY_FILES_C} ${LMFit_LIBRARY_FILES_H}
	${TRWS_LIBRARY_FILES_C} ${TRWS_LIBRARY_FILES_H}
)

# Manually set Common.h as the precompiled header
set_target_pch(Math Common.h)

# Link its dependencies
TARGET_LINK_LIBRARIES(Math Common)

# Install
INSTALL(FILES ${LIBRARY_FILES_H} DESTINATION "${INSTALL_INCLUDE_DIR}/Math" COMPONENT dev)
INSTALL(FILES ${IBFS_LIBRARY_FILES_H} DESTINATION "${INSTALL_INCLUDE_DIR}/Math/IBFS" COMPONENT dev)
INSTALL(FILES ${LMFit_LIBRARY_FILES_H} DESTINATION "${INSTALL_INCLUDE_DIR}/Math/LMFit" COMPONENT dev)
INSTALL(FILES ${TRWS_LIBRARY_FILES_H} DESTINATION "${INSTALL_INCLUDE_DIR}/Math/TRWS" COMPONENT dev)
INSTALL(TARGETS Math
	EXPORT OpenMVSTargets
	RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
	LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
	ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib)
