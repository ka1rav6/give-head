add_library(lexer
    lexer/lexer.cpp
)

add_library(generator
    generator/generator.cpp
)

target_include_directories(generator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lexer
)

add_library(parser
    parser/parser.cpp
)

# Target for the full executable (add main.cpp later)
add_executable(copa
    main.cpp
)

target_link_libraries(copa
    PRIVATE
        lexer
        generator
        parser
)

target_include_directories(lexer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lexer
        ${CMAKE_CURRENT_SOURCE_DIR}/logger
)

target_include_directories(parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lexer
        ${CMAKE_CURRENT_SOURCE_DIR}/logger
)

target_include_directories(copa
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lexer
        ${CMAKE_CURRENT_SOURCE_DIR}/generator
        ${CMAKE_CURRENT_SOURCE_DIR}/parser
        ${CMAKE_CURRENT_SOURCE_DIR}/logger
)
