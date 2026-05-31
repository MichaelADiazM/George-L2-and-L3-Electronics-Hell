idf_component_register(
    SRCS 
        "main.cpp"
        "sensor.cpp"
        "radio.cpp"
        "logger.cpp"
        "server.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino
)