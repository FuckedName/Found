EXECUTE_FILE_NAME="Found"
CONFIG_FILE_NAME="config.txt"
EXECUTE_PATH_NAME="/usr/bin"



rm -rf ${EXECUTE_FILE_NAME}

rm -rf ${EXECUTE_PATH_NAME}/${EXECUTE_FILE_NAME}

gcc -g -rdynamic -o ${EXECUTE_FILE_NAME} main.c  -std=c99 -lpthread

cp -rf ${EXECUTE_FILE_NAME} ${EXECUTE_PATH_NAME}/${EXECUTE_FILE_NAME}
#cp -rf ${CONFIG_FILE_NAME} ${EXECUTE_PATH_NAME}

${EXECUTE_FILE_NAME}


