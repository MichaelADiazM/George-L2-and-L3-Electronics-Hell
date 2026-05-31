idf_component_register(
    SRCS 
        "main.cpp"
        "sensor.cpp"
        "radio.cpp"
        "logger.cpp"
        "server.cpp"
    INCLUDE_DIRS 
        "."
        "C:/Users/diazm/OneDrive/Documents/Arduino/libraries/Adafruit_BMP3XX_Library"
        "C:/Users/diazm/OneDrive/Documents/Arduino/libraries/RadioHead"
    REQUIRES arduino
)