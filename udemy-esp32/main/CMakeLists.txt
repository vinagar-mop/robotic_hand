idf_component_register(SRCS "main.c" "wifi_app.c" "http_server.c" "a0090_servo_motor.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "webpage/app.css" "webpage/app.js" "webpage/index.html" "webpage/favicon.ico" "webpage/jquery-3.3.1.min.js" "webpage/three.min.js")
