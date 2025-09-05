
aux_source_directory(. SRC_LIST)


add_executable(chatClient ${SRC_LIST})
target_link_libraries(chatClient pthread)